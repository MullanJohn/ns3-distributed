/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ar-client.h"

#include "simple-task.h"
#include "tcp-connection-manager.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ArClient");

NS_OBJECT_ENSURE_REGISTERED(ArClient);

uint32_t ArClient::s_nextClientId = 0;

TypeId
ArClient::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ArClient")
            .SetParent<Application>()
            .SetGroupName("Distributed")
            .AddConstructor<ArClient>()
            .AddAttribute("Remote",
                          "The address of the remote orchestrator",
                          AddressValue(),
                          MakeAddressAccessor(&ArClient::m_peer),
                          MakeAddressChecker())
            .AddAttribute("ConnectionManager",
                          "Connection manager for transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&ArClient::m_connMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddAttribute("FrameRate",
                          "Frames per second",
                          DoubleValue(30.0),
                          MakeDoubleAccessor(&ArClient::m_frameRate),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("FrameSize",
                          "Random variable for input frame size in bytes",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&ArClient::m_frameSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("ComputeDemand",
                          "Random variable for compute demand per frame in FLOPS",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&ArClient::m_computeDemand),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("OutputSize",
                          "Random variable for result size in bytes",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&ArClient::m_outputSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddTraceSource("FrameSent",
                            "Trace fired when a frame admission request is sent",
                            MakeTraceSourceAccessor(&ArClient::m_frameSentTrace),
                            "ns3::ArClient::FrameSentTracedCallback")
            .AddTraceSource("FrameProcessed",
                            "Trace fired when a processed frame result is received",
                            MakeTraceSourceAccessor(&ArClient::m_frameProcessedTrace),
                            "ns3::ArClient::FrameProcessedTracedCallback")
            .AddTraceSource("FrameRejected",
                            "Trace fired when a frame admission is rejected",
                            MakeTraceSourceAccessor(&ArClient::m_frameRejectedTrace),
                            "ns3::ArClient::FrameRejectedTracedCallback")
            .AddTraceSource(
                "FrameDropped",
                "Trace fired when a frame is dropped because the previous frame is still pending",
                MakeTraceSourceAccessor(&ArClient::m_frameDroppedTrace),
                "ns3::ArClient::FrameDroppedTracedCallback");
    return tid;
}

ArClient::ArClient()
    : m_connMgr(nullptr),
      m_frameRate(30.0),
      m_clientId(s_nextClientId++),
      m_framesSent(0),
      m_frameCount(0),
      m_framesDropped(0),
      m_nextDagId(1),
      m_totalTx(0),
      m_totalRx(0),
      m_rxBuffer(Create<Packet>()),
      m_responsesReceived(0)
{
    NS_LOG_FUNCTION(this);
}

ArClient::~ArClient()
{
    NS_LOG_FUNCTION(this);
}

void
ArClient::DoDispose()
{
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);

    if (m_connMgr)
    {
        m_connMgr->Close();
        m_connMgr = nullptr;
    }

    m_rxBuffer = nullptr;
    m_frameSize = nullptr;
    m_computeDemand = nullptr;
    m_outputSize = nullptr;
    m_pendingWorkloads.clear();

    Application::DoDispose();
}

void
ArClient::SetRemote(const Address& addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peer = addr;
}

Address
ArClient::GetRemote() const
{
    return m_peer;
}

uint64_t
ArClient::GetFramesSent() const
{
    return m_framesSent;
}

uint64_t
ArClient::GetFramesDropped() const
{
    return m_framesDropped;
}

uint64_t
ArClient::GetResponsesReceived() const
{
    return m_responsesReceived;
}

uint64_t
ArClient::GetTotalTx() const
{
    return m_totalTx;
}

uint64_t
ArClient::GetTotalRx() const
{
    return m_totalRx;
}

void
ArClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(m_peer.IsInvalid(), "Remote address not set");

    if (!m_connMgr)
    {
        m_connMgr = CreateObject<TcpConnectionManager>();
    }

    m_connMgr->SetNode(GetNode());
    m_connMgr->SetReceiveCallback(MakeCallback(&ArClient::HandleReceive, this));

    Ptr<TcpConnectionManager> tcpConnMgr = DynamicCast<TcpConnectionManager>(m_connMgr);
    if (tcpConnMgr)
    {
        tcpConnMgr->SetConnectionCallback(MakeCallback(&ArClient::HandleConnected, this));
        tcpConnMgr->SetConnectionFailedCallback(
            MakeCallback(&ArClient::HandleConnectionFailed, this));
    }

    m_connMgr->Connect(m_peer);

    if (!tcpConnMgr)
    {
        ScheduleNextFrame();
    }
}

void
ArClient::StopApplication()
{
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);

    if (m_connMgr)
    {
        m_connMgr->Close();
    }
}

void
ArClient::HandleConnected(const Address& serverAddr)
{
    NS_LOG_FUNCTION(this << serverAddr);
    NS_LOG_INFO("ArClient " << m_clientId << " connected to orchestrator " << serverAddr);

    ScheduleNextFrame();
}

void
ArClient::HandleConnectionFailed(const Address& serverAddr)
{
    NS_LOG_FUNCTION(this << serverAddr);
    NS_LOG_ERROR("ArClient " << m_clientId << " failed to connect to " << serverAddr);
}

void
ArClient::HandleReceive(Ptr<Packet> packet, const Address& from)
{
    NS_LOG_FUNCTION(this << packet << from);

    if (packet->GetSize() == 0)
    {
        return;
    }

    m_totalRx += packet->GetSize();
    NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from " << from);
    m_rxBuffer->AddAtEnd(packet);
    ProcessBuffer();
}

void
ArClient::GenerateFrame()
{
    NS_LOG_FUNCTION(this);

    if (!m_connMgr || !m_connMgr->IsConnected())
    {
        NS_LOG_DEBUG("Not connected, cannot submit frame");
        return;
    }

    m_frameCount++;

    if (!m_pendingWorkloads.empty())
    {
        m_framesDropped++;
        NS_LOG_INFO("ArClient " << m_clientId << " dropped frame " << m_frameCount
                                << " (previous frame still pending)");
        m_frameDroppedTrace(m_frameCount);
        ScheduleNextFrame();
        return;
    }

    uint64_t frameSize = static_cast<uint64_t>(m_frameSize->GetValue());
    double computeDemand = m_computeDemand->GetValue();
    uint64_t outputSize = static_cast<uint64_t>(m_outputSize->GetValue());

    Ptr<SimpleTask> task = CreateObject<SimpleTask>();
    task->SetComputeDemand(computeDemand);
    task->SetInputSize(frameSize);
    task->SetOutputSize(outputSize);

    uint64_t taskId = (static_cast<uint64_t>(m_clientId) << 32) | m_framesSent;
    task->SetTaskId(taskId);

    Ptr<DagTask> dag = CreateObject<DagTask>();
    dag->AddTask(task);

    uint64_t dagId = (static_cast<uint64_t>(m_clientId) << 32) | m_nextDagId++;

    Ptr<Packet> metadata = dag->SerializeMetadata();

    OrchestratorHeader orchHeader;
    orchHeader.SetMessageType(OrchestratorHeader::ADMISSION_REQUEST);
    orchHeader.SetTaskId(dagId);
    orchHeader.SetPayloadSize(metadata->GetSize());

    Ptr<Packet> packet = Create<Packet>();
    packet->AddAtEnd(metadata);
    packet->AddHeader(orchHeader);

    PendingWorkload pw;
    pw.dag = dag;
    pw.submitTime = Simulator::Now();
    m_pendingWorkloads[dagId] = pw;

    m_connMgr->Send(packet);

    m_framesSent++;
    m_totalTx += packet->GetSize();

    NS_LOG_INFO("ArClient " << m_clientId << " sent frame " << m_framesSent << " (dagId " << dagId
                            << ", " << frameSize << " bytes, " << computeDemand << " FLOPS)");

    m_frameSentTrace(task);

    ScheduleNextFrame();
}

void
ArClient::ScheduleNextFrame()
{
    NS_LOG_FUNCTION(this);

    if (m_sendEvent.IsPending())
    {
        return;
    }

    Time interval = Seconds(1.0 / m_frameRate);
    m_sendEvent = Simulator::Schedule(interval, &ArClient::GenerateFrame, this);

    NS_LOG_DEBUG("Next frame scheduled in " << interval.GetMilliSeconds() << " ms");
}

void
ArClient::ProcessBuffer()
{
    NS_LOG_FUNCTION(this);

    while (m_rxBuffer->GetSize() > 0)
    {
        uint32_t sizeBefore = m_rxBuffer->GetSize();

        uint8_t firstByte;
        m_rxBuffer->CopyData(&firstByte, 1);

        if (firstByte >= OrchestratorHeader::ADMISSION_REQUEST)
        {
            if (m_rxBuffer->GetSize() < OrchestratorHeader::SERIALIZED_SIZE)
            {
                break;
            }

            OrchestratorHeader orchHeader;
            m_rxBuffer->PeekHeader(orchHeader);

            uint64_t totalSize = OrchestratorHeader::SERIALIZED_SIZE + orchHeader.GetPayloadSize();
            if (m_rxBuffer->GetSize() < totalSize)
            {
                break;
            }

            m_rxBuffer->RemoveAtStart(totalSize);
            HandleAdmissionResponse(orchHeader);
        }
        else
        {
            HandleTaskResponse();
        }

        if (m_rxBuffer->GetSize() == sizeBefore)
        {
            break;
        }
    }
}

void
ArClient::HandleAdmissionResponse(const OrchestratorHeader& orchHeader)
{
    NS_LOG_FUNCTION(this << orchHeader.GetTaskId() << orchHeader.IsAdmitted());

    uint64_t dagId = orchHeader.GetTaskId();

    auto it = m_pendingWorkloads.find(dagId);
    if (it == m_pendingWorkloads.end())
    {
        NS_LOG_WARN("Received admission response for unknown dagId " << dagId);
        return;
    }

    if (orchHeader.IsAdmitted())
    {
        NS_LOG_INFO("ArClient " << m_clientId << " admission ACCEPTED for dagId " << dagId);
        SendFullData(dagId);
    }
    else
    {
        NS_LOG_INFO("ArClient " << m_clientId << " admission REJECTED for dagId " << dagId);

        Ptr<DagTask> dag = it->second.dag;
        for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
        {
            Ptr<Task> task = dag->GetTask(i);
            if (task)
            {
                m_frameRejectedTrace(task);
            }
        }

        m_pendingWorkloads.erase(it);
    }
}

void
ArClient::HandleTaskResponse()
{
    NS_LOG_FUNCTION(this);

    uint64_t consumedBytes = 0;
    Ptr<Task> task = SimpleTask::Deserialize(m_rxBuffer, consumedBytes);

    if (consumedBytes == 0)
    {
        return;
    }

    m_rxBuffer->RemoveAtStart(consumedBytes);

    if (!task)
    {
        NS_LOG_WARN("Deserializer consumed " << consumedBytes << " bytes but returned null task");
        return;
    }

    uint64_t taskId = task->GetTaskId();

    for (auto it = m_pendingWorkloads.begin(); it != m_pendingWorkloads.end(); ++it)
    {
        Ptr<DagTask> dag = it->second.dag;
        int32_t dagIdx = dag->GetTaskIndex(taskId);
        if (dagIdx >= 0)
        {
            Time latency = Simulator::Now() - it->second.submitTime;
            m_responsesReceived++;

            NS_LOG_INFO("ArClient " << m_clientId << " received result for frame (task " << taskId
                                    << ", latency=" << latency.GetMilliSeconds() << "ms)");

            m_frameProcessedTrace(task, latency);

            dag->MarkCompleted(static_cast<uint32_t>(dagIdx));
            if (dag->IsComplete())
            {
                m_pendingWorkloads.erase(it);
            }
            return;
        }
    }

    NS_LOG_WARN("Received response for unknown task " << taskId);
}

void
ArClient::SendFullData(uint64_t dagId)
{
    NS_LOG_FUNCTION(this << dagId);

    auto it = m_pendingWorkloads.find(dagId);
    if (it == m_pendingWorkloads.end())
    {
        NS_LOG_WARN("Cannot send full data for unknown dagId " << dagId);
        return;
    }

    Ptr<DagTask> dag = it->second.dag;
    Ptr<Packet> packet = dag->SerializeFullData();

    m_connMgr->Send(packet);
    m_totalTx += packet->GetSize();

    NS_LOG_INFO("ArClient " << m_clientId << " sent full frame data for dagId " << dagId << " ("
                            << packet->GetSize() << " bytes)");
}

int64_t
ArClient::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    auto currentStream = stream;
    m_frameSize->SetStream(currentStream++);
    m_computeDemand->SetStream(currentStream++);
    m_outputSize->SetStream(currentStream++);
    currentStream += Application::AssignStreams(currentStream);
    return (currentStream - stream);
}

} // namespace ns3
