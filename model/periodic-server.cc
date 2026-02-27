/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "periodic-server.h"

#include "scaling-command-header.h"
#include "simple-task.h"
#include "tcp-connection-manager.h"

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PeriodicServer");

NS_OBJECT_ENSURE_REGISTERED(PeriodicServer);

TypeId
PeriodicServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::PeriodicServer")
            .SetParent<Application>()
            .SetGroupName("Distributed")
            .AddConstructor<PeriodicServer>()
            .AddAttribute("Port",
                          "Port on which to listen for incoming connections",
                          UintegerValue(9000),
                          MakeUintegerAccessor(&PeriodicServer::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("ConnectionManager",
                          "Connection manager for transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&PeriodicServer::m_connMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddTraceSource("FrameReceived",
                            "A frame has been received for processing",
                            MakeTraceSourceAccessor(&PeriodicServer::m_frameReceivedTrace),
                            "ns3::PeriodicServer::FrameReceivedTracedCallback")
            .AddTraceSource("FrameProcessed",
                            "A frame has been processed and response sent",
                            MakeTraceSourceAccessor(&PeriodicServer::m_frameProcessedTrace),
                            "ns3::PeriodicServer::FrameProcessedTracedCallback");
    return tid;
}

PeriodicServer::PeriodicServer()
    : m_port(9000),
      m_connMgr(nullptr),
      m_accelerator(nullptr),
      m_framesReceived(0),
      m_framesProcessed(0),
      m_totalRx(0)
{
    NS_LOG_FUNCTION(this);
}

PeriodicServer::~PeriodicServer()
{
    NS_LOG_FUNCTION(this);
}

void
PeriodicServer::DoDispose()
{
    NS_LOG_FUNCTION(this);

    if (m_accelerator)
    {
        m_accelerator->TraceDisconnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&PeriodicServer::OnTaskCompleted, this));
    }

    if (m_connMgr)
    {
        m_connMgr->Close();
        m_connMgr = nullptr;
    }

    m_rxBuffer.clear();
    m_pendingTasks.clear();
    m_accelerator = nullptr;

    Application::DoDispose();
}

uint64_t
PeriodicServer::GetFramesReceived() const
{
    return m_framesReceived;
}

uint64_t
PeriodicServer::GetFramesProcessed() const
{
    return m_framesProcessed;
}

uint64_t
PeriodicServer::GetTotalRx() const
{
    return m_totalRx;
}

uint16_t
PeriodicServer::GetPort() const
{
    return m_port;
}

void
PeriodicServer::StartApplication()
{
    NS_LOG_FUNCTION(this);

    m_accelerator = GetNode()->GetObject<Accelerator>();
    if (!m_accelerator)
    {
        NS_LOG_WARN("No Accelerator aggregated to this node. Frames will be dropped.");
    }
    else
    {
        m_accelerator->TraceConnectWithoutContext("TaskCompleted",
                                                  MakeCallback(&PeriodicServer::OnTaskCompleted, this));
    }

    if (!m_connMgr)
    {
        m_connMgr = CreateObject<TcpConnectionManager>();
    }

    m_connMgr->SetNode(GetNode());
    m_connMgr->SetReceiveCallback(MakeCallback(&PeriodicServer::HandleReceive, this));

    Ptr<TcpConnectionManager> tcpConnMgr = DynamicCast<TcpConnectionManager>(m_connMgr);
    if (tcpConnMgr)
    {
        tcpConnMgr->SetCloseCallback(MakeCallback(&PeriodicServer::HandleClientClose, this));
    }

    m_connMgr->Bind(m_port);

    NS_LOG_INFO("PeriodicServer listening on port " << m_port);
}

void
PeriodicServer::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_accelerator)
    {
        m_accelerator->TraceDisconnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&PeriodicServer::OnTaskCompleted, this));
    }

    if (m_connMgr)
    {
        m_connMgr->Close();
    }

    m_rxBuffer.clear();
}

void
PeriodicServer::HandleReceive(Ptr<Packet> packet, const Address& from)
{
    NS_LOG_FUNCTION(this << packet << from);

    if (packet->GetSize() == 0)
    {
        return;
    }

    m_totalRx += packet->GetSize();
    NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from " << from);

    auto it = m_rxBuffer.find(from);
    if (it == m_rxBuffer.end())
    {
        m_rxBuffer[from] = packet->Copy();
    }
    else
    {
        it->second->AddAtEnd(packet);
    }

    ProcessBuffer(from);
}

void
PeriodicServer::HandleClientClose(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);
    NS_LOG_INFO("Client disconnected: " << clientAddr);
    CleanupClient(clientAddr);
}

void
PeriodicServer::ProcessBuffer(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);

    auto it = m_rxBuffer.find(clientAddr);
    if (it == m_rxBuffer.end())
    {
        return;
    }

    Ptr<Packet> buffer = it->second;

    while (buffer->GetSize() > 0)
    {
        uint8_t firstByte;
        buffer->CopyData(&firstByte, 1);

        if (firstByte == ScalingCommandHeader::SCALING_COMMAND)
        {
            if (buffer->GetSize() < ScalingCommandHeader::SERIALIZED_SIZE)
            {
                break;
            }
            HandleScalingCommand(buffer);
            continue;
        }

        uint64_t consumedBytes = 0;
        Ptr<Task> task = SimpleTask::Deserialize(buffer, consumedBytes);

        if (consumedBytes == 0)
        {
            break;
        }

        buffer->RemoveAtStart(consumedBytes);

        if (task)
        {
            ProcessTask(task, clientAddr);
        }
        else
        {
            NS_LOG_WARN("Deserializer consumed " << consumedBytes
                                                 << " bytes but returned null task");
        }
    }

    if (buffer->GetSize() == 0)
    {
        m_rxBuffer.erase(it);
    }
}

void
PeriodicServer::ProcessTask(Ptr<Task> task, const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << task->GetTaskId() << clientAddr);

    m_framesReceived++;
    m_frameReceivedTrace(task);

    NS_LOG_INFO("Received frame (task " << task->GetTaskId()
                                        << ", compute=" << task->GetComputeDemand()
                                        << ", input=" << task->GetInputSize() << ")");

    if (!m_accelerator)
    {
        NS_LOG_ERROR("No accelerator available, dropping frame " << task->GetTaskId());
        return;
    }

    task->SetArrivalTime(Simulator::Now());

    PendingTask pending;
    pending.clientAddr = clientAddr;
    pending.task = task;
    m_pendingTasks[task->GetTaskId()] = pending;

    m_accelerator->SubmitTask(task);
    NS_LOG_DEBUG("Submitted task " << task->GetTaskId() << " to accelerator");
}

void
PeriodicServer::OnTaskCompleted(Ptr<const Task> task, Time duration)
{
    NS_LOG_FUNCTION(this << task->GetTaskId() << duration);

    auto it = m_pendingTasks.find(task->GetTaskId());
    if (it == m_pendingTasks.end())
    {
        NS_LOG_DEBUG("Task " << task->GetTaskId() << " not found in pending tasks (not ours)");
        return;
    }

    Address clientAddr = it->second.clientAddr;
    Ptr<Task> pendingTask = it->second.task;
    m_pendingTasks.erase(it);

    SendResponse(clientAddr, pendingTask, duration);
}

void
PeriodicServer::SendResponse(const Address& clientAddr, Ptr<const Task> task, Time duration)
{
    NS_LOG_FUNCTION(this << clientAddr << task->GetTaskId() << duration);

    Ptr<Packet> packet = task->Serialize(true);

    m_connMgr->Send(packet, clientAddr);

    m_framesProcessed++;
    m_frameProcessedTrace(task, duration);

    NS_LOG_INFO("Sent result for frame (task "
                << task->GetTaskId() << ", output=" << task->GetOutputSize()
                << " bytes, duration=" << duration.GetMilliSeconds() << "ms)");
}

void
PeriodicServer::HandleScalingCommand(Ptr<Packet> buffer)
{
    NS_LOG_FUNCTION(this);

    Ptr<Packet> fragment = buffer->CreateFragment(0, ScalingCommandHeader::SERIALIZED_SIZE);
    buffer->RemoveAtStart(ScalingCommandHeader::SERIALIZED_SIZE);

    ScalingCommandHeader header;
    fragment->RemoveHeader(header);

    if (m_accelerator)
    {
        m_accelerator->SetFrequency(header.GetTargetFrequency());
        m_accelerator->SetVoltage(header.GetTargetVoltage());
        NS_LOG_INFO("Applied scaling command: freq=" << header.GetTargetFrequency()
                                                     << " volt=" << header.GetTargetVoltage());
    }
}

void
PeriodicServer::CleanupClient(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);

    for (auto it = m_pendingTasks.begin(); it != m_pendingTasks.end();)
    {
        if (it->second.clientAddr == clientAddr)
        {
            NS_LOG_DEBUG("Removing pending task " << it->first << " for disconnected client");
            it = m_pendingTasks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    auto bufferIt = m_rxBuffer.find(clientAddr);
    if (bufferIt != m_rxBuffer.end())
    {
        NS_LOG_DEBUG("Removing rx buffer for disconnected client");
        m_rxBuffer.erase(bufferIt);
    }
}

} // namespace ns3
