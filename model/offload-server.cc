/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-server.h"

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

NS_LOG_COMPONENT_DEFINE("OffloadServer");

NS_OBJECT_ENSURE_REGISTERED(OffloadServer);

TypeId
OffloadServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OffloadServer")
            .SetParent<Application>()
            .SetGroupName("Distributed")
            .AddConstructor<OffloadServer>()
            .AddAttribute("Port",
                          "Port on which to listen for incoming connections",
                          UintegerValue(9000),
                          MakeUintegerAccessor(&OffloadServer::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("ConnectionManager",
                          "Connection manager for transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&OffloadServer::m_connMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddTraceSource("TaskReceived",
                            "A task request has been received",
                            MakeTraceSourceAccessor(&OffloadServer::m_taskReceivedTrace),
                            "ns3::OffloadServer::TaskReceivedTracedCallback")
            .AddTraceSource("TaskCompleted",
                            "A task has been completed and response sent",
                            MakeTraceSourceAccessor(&OffloadServer::m_taskCompletedTrace),
                            "ns3::OffloadServer::TaskCompletedTracedCallback");
    return tid;
}

OffloadServer::OffloadServer()
    : m_port(9000),
      m_connMgr(nullptr),
      m_accelerator(nullptr),
      m_tasksReceived(0),
      m_tasksCompleted(0),
      m_totalRx(0)
{
    NS_LOG_FUNCTION(this);
}

OffloadServer::~OffloadServer()
{
    NS_LOG_FUNCTION(this);
}

void
OffloadServer::DoDispose()
{
    NS_LOG_FUNCTION(this);

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
OffloadServer::GetTasksReceived() const
{
    return m_tasksReceived;
}

uint64_t
OffloadServer::GetTasksCompleted() const
{
    return m_tasksCompleted;
}

uint64_t
OffloadServer::GetTotalRx() const
{
    return m_totalRx;
}

uint16_t
OffloadServer::GetPort() const
{
    return m_port;
}

void
OffloadServer::StartApplication()
{
    NS_LOG_FUNCTION(this);

    // Get the Accelerator aggregated to this node
    m_accelerator = GetNode()->GetObject<Accelerator>();
    if (!m_accelerator)
    {
        NS_LOG_WARN("No Accelerator aggregated to this node. Tasks will be rejected.");
    }
    else
    {
        // Subscribe to task completion events
        m_accelerator->TraceConnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&OffloadServer::OnTaskCompleted, this));
    }

    if (!m_connMgr)
    {
        m_connMgr = CreateObject<TcpConnectionManager>();
    }

    m_connMgr->SetNode(GetNode());
    m_connMgr->SetReceiveCallback(MakeCallback(&OffloadServer::HandleReceive, this));

    // Set close callback for TCP to handle client disconnections
    Ptr<TcpConnectionManager> tcpConnMgr = DynamicCast<TcpConnectionManager>(m_connMgr);
    if (tcpConnMgr)
    {
        tcpConnMgr->SetCloseCallback(MakeCallback(&OffloadServer::HandleClientClose, this));
    }

    m_connMgr->Bind(m_port);

    NS_LOG_INFO("OffloadServer listening on port " << m_port);
}

void
OffloadServer::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_accelerator)
    {
        m_accelerator->TraceDisconnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&OffloadServer::OnTaskCompleted, this));
    }

    if (m_connMgr)
    {
        m_connMgr->Close();
    }

    m_rxBuffer.clear();
}

void
OffloadServer::HandleReceive(Ptr<Packet> packet, const Address& from)
{
    NS_LOG_FUNCTION(this << packet << from);

    if (packet->GetSize() == 0)
    {
        return;
    }

    m_totalRx += packet->GetSize();
    NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from " << from);

    // Add to buffer for this client
    auto it = m_rxBuffer.find(from);
    if (it == m_rxBuffer.end())
    {
        m_rxBuffer[from] = packet->Copy();
    }
    else
    {
        it->second->AddAtEnd(packet);
    }

    // Try to process complete messages from the buffer
    ProcessBuffer(from);
}

void
OffloadServer::HandleClientClose(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);
    NS_LOG_INFO("Client disconnected: " << clientAddr);
    CleanupClient(clientAddr);
}

void
OffloadServer::ProcessBuffer(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);

    auto it = m_rxBuffer.find(clientAddr);
    if (it == m_rxBuffer.end())
    {
        return;
    }

    Ptr<Packet> buffer = it->second;

    // Process all complete messages in the buffer
    while (buffer->GetSize() > 0)
    {
        // Peek first byte to determine message type
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

        // Task message — deserialize via SimpleTask
        uint64_t consumedBytes = 0;
        Ptr<Task> task = SimpleTask::Deserialize(buffer, consumedBytes);

        if (consumedBytes == 0)
        {
            // Not enough data for a complete message
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

    // Update or remove the buffer entry
    if (buffer->GetSize() == 0)
    {
        m_rxBuffer.erase(it);
    }
}

void
OffloadServer::ProcessTask(Ptr<Task> task, const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << task->GetTaskId() << clientAddr);

    m_tasksReceived++;
    m_taskReceivedTrace(task);

    NS_LOG_INFO("Received task " << task->GetTaskId() << " (compute=" << task->GetComputeDemand()
                                 << ", input=" << task->GetInputSize()
                                 << ", output=" << task->GetOutputSize() << ")");

    if (!m_accelerator)
    {
        NS_LOG_ERROR("No accelerator available, dropping task " << task->GetTaskId());
        return;
    }

    task->SetArrivalTime(Simulator::Now());

    // Track the pending task for response routing
    PendingTask pending;
    pending.clientAddr = clientAddr;
    pending.task = task;
    m_pendingTasks[task->GetTaskId()] = pending;

    // Submit to the accelerator
    m_accelerator->SubmitTask(task);

    NS_LOG_DEBUG("Submitted task " << task->GetTaskId() << " to accelerator");
}

void
OffloadServer::OnTaskCompleted(Ptr<const Task> task, Time duration)
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
OffloadServer::SendResponse(const Address& clientAddr, Ptr<const Task> task, Time duration)
{
    NS_LOG_FUNCTION(this << clientAddr << task->GetTaskId() << duration);

    Ptr<Packet> packet = task->Serialize(true);

    m_connMgr->Send(packet, clientAddr);

    m_tasksCompleted++;
    m_taskCompletedTrace(task, duration);

    NS_LOG_INFO("Sent response for task "
                << task->GetTaskId() << " (output=" << task->GetOutputSize()
                << " bytes, duration=" << duration.GetMilliSeconds() << "ms)");
}

void
OffloadServer::HandleScalingCommand(Ptr<Packet> buffer)
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
OffloadServer::CleanupClient(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);

    // Clear pending tasks for this client
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

    // Clean up receive buffer for this client
    auto bufferIt = m_rxBuffer.find(clientAddr);
    if (bufferIt != m_rxBuffer.end())
    {
        NS_LOG_DEBUG("Removing rx buffer for disconnected client");
        m_rxBuffer.erase(bufferIt);
    }
}

} // namespace ns3
