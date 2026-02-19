/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-client.h"

#include "simple-task.h"
#include "tcp-connection-manager.h"

#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OffloadClient");

NS_OBJECT_ENSURE_REGISTERED(OffloadClient);

// Static counter for assigning unique client IDs
uint32_t OffloadClient::s_nextClientId = 0;

TypeId
OffloadClient::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OffloadClient")
            .SetParent<Application>()
            .SetGroupName("Distributed")
            .AddConstructor<OffloadClient>()
            .AddAttribute("Remote",
                          "The address of the remote orchestrator",
                          AddressValue(),
                          MakeAddressAccessor(&OffloadClient::m_peer),
                          MakeAddressChecker())
            .AddAttribute("ConnectionManager",
                          "Connection manager for transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&OffloadClient::m_connMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddAttribute("MaxTasks",
                          "Maximum number of tasks to auto-generate (0 = programmatic only)",
                          UintegerValue(0),
                          MakeUintegerAccessor(&OffloadClient::m_maxTasks),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("InterArrivalTime",
                          "Random variable for inter-arrival time between tasks",
                          PointerValue(CreateObject<ExponentialRandomVariable>()),
                          MakePointerAccessor(&OffloadClient::m_interArrivalTime),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("ComputeDemand",
                          "Random variable for task compute demand in FLOPS",
                          PointerValue(CreateObject<ExponentialRandomVariable>()),
                          MakePointerAccessor(&OffloadClient::m_computeDemand),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("InputSize",
                          "Random variable for task input size in bytes",
                          PointerValue(CreateObject<ExponentialRandomVariable>()),
                          MakePointerAccessor(&OffloadClient::m_inputSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("OutputSize",
                          "Random variable for task output size in bytes",
                          PointerValue(CreateObject<ExponentialRandomVariable>()),
                          MakePointerAccessor(&OffloadClient::m_outputSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddTraceSource("TaskSent",
                            "Trace fired when an admission request is sent",
                            MakeTraceSourceAccessor(&OffloadClient::m_taskSentTrace),
                            "ns3::OffloadClient::TaskSentTracedCallback")
            .AddTraceSource("ResponseReceived",
                            "Trace fired when a task response is received",
                            MakeTraceSourceAccessor(&OffloadClient::m_responseReceivedTrace),
                            "ns3::OffloadClient::ResponseReceivedTracedCallback")
            .AddTraceSource("TaskRejected",
                            "Trace fired when an admission is rejected",
                            MakeTraceSourceAccessor(&OffloadClient::m_taskRejectedTrace),
                            "ns3::OffloadClient::TaskRejectedTracedCallback");
    return tid;
}

OffloadClient::OffloadClient()
    : m_connMgr(nullptr),
      m_maxTasks(0),
      m_clientId(s_nextClientId++),
      m_taskCount(0),
      m_totalTx(0),
      m_totalRx(0),
      m_nextDagId(1),
      m_rxBuffer(Create<Packet>()),
      m_responsesReceived(0)
{
    NS_LOG_FUNCTION(this);
}

OffloadClient::~OffloadClient()
{
    NS_LOG_FUNCTION(this);
}

void
OffloadClient::DoDispose()
{
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);

    // Clean up ConnectionManager
    if (m_connMgr)
    {
        m_connMgr->Close();
        m_connMgr = nullptr;
    }

    m_rxBuffer = nullptr;
    m_pendingWorkloads.clear();
    m_interArrivalTime = nullptr;
    m_computeDemand = nullptr;
    m_inputSize = nullptr;
    m_outputSize = nullptr;

    Application::DoDispose();
}

void
OffloadClient::SetRemote(const Address& addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peer = addr;
}

Address
OffloadClient::GetRemote() const
{
    return m_peer;
}

uint64_t
OffloadClient::GetTasksSent() const
{
    return m_taskCount;
}

uint64_t
OffloadClient::GetTotalTx() const
{
    return m_totalTx;
}

uint64_t
OffloadClient::GetTotalRx() const
{
    return m_totalRx;
}

uint64_t
OffloadClient::GetResponsesReceived() const
{
    return m_responsesReceived;
}

void
OffloadClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(m_peer.IsInvalid(), "Remote address not set");

    // Create default ConnectionManager if not set
    if (!m_connMgr)
    {
        m_connMgr = CreateObject<TcpConnectionManager>();
    }

    // Configure ConnectionManager
    m_connMgr->SetNode(GetNode());
    m_connMgr->SetReceiveCallback(MakeCallback(&OffloadClient::HandleReceive, this));

    // Set TCP-specific callbacks
    Ptr<TcpConnectionManager> tcpConnMgr = DynamicCast<TcpConnectionManager>(m_connMgr);
    if (tcpConnMgr)
    {
        tcpConnMgr->SetConnectionCallback(MakeCallback(&OffloadClient::HandleConnected, this));
        tcpConnMgr->SetConnectionFailedCallback(
            MakeCallback(&OffloadClient::HandleConnectionFailed, this));
    }

    // Connect to orchestrator
    m_connMgr->Connect(m_peer);

    // For connectionless transports (UDP), start sending immediately
    if (!tcpConnMgr && m_maxTasks > 0)
    {
        ScheduleNextTask();
    }
}

void
OffloadClient::StopApplication()
{
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);

    // Close ConnectionManager
    if (m_connMgr)
    {
        m_connMgr->Close();
    }
}

void
OffloadClient::HandleConnected(const Address& serverAddr)
{
    NS_LOG_FUNCTION(this << serverAddr);
    NS_LOG_INFO("Client " << m_clientId << " connected to orchestrator " << serverAddr);

    // Start auto-generating tasks if configured
    if (m_maxTasks > 0)
    {
        ScheduleNextTask();
    }
}

void
OffloadClient::HandleConnectionFailed(const Address& serverAddr)
{
    NS_LOG_FUNCTION(this << serverAddr);
    NS_LOG_ERROR("Client " << m_clientId << " failed to connect to " << serverAddr);
}

void
OffloadClient::HandleReceive(Ptr<Packet> packet, const Address& from)
{
    NS_LOG_FUNCTION(this << packet << from);

    if (packet->GetSize() == 0)
    {
        return;
    }

    m_totalRx += packet->GetSize();

    // Append to receive buffer
    m_rxBuffer->AddAtEnd(packet);

    // Process complete messages
    ProcessBuffer();
}

void
OffloadClient::SubmitTask(Ptr<Task> task)
{
    NS_LOG_FUNCTION(this << task->GetTaskId());

    if (!m_connMgr || !m_connMgr->IsConnected())
    {
        NS_LOG_DEBUG("Not connected, cannot submit task");
        return;
    }

    // Assign task ID if zero: (clientId << 32) | seqNum
    if (task->GetTaskId() == 0)
    {
        uint64_t taskId = (static_cast<uint64_t>(m_clientId) << 32) | m_taskCount;
        task->SetTaskId(taskId);
    }

    // Wrap as 1-node DagTask
    Ptr<DagTask> dag = CreateObject<DagTask>();
    dag->AddTask(task);

    // Assign a dagId for tracking
    uint64_t dagId = (static_cast<uint64_t>(m_clientId) << 32) | m_nextDagId++;

    // Serialize DAG metadata for admission request
    Ptr<Packet> metadata = dag->SerializeMetadata();

    // Create OrchestratorHeader
    OrchestratorHeader orchHeader;
    orchHeader.SetMessageType(OrchestratorHeader::ADMISSION_REQUEST);
    orchHeader.SetTaskId(dagId);
    orchHeader.SetPayloadSize(metadata->GetSize());

    // Build packet: OrchestratorHeader + metadata
    Ptr<Packet> packet = Create<Packet>();
    packet->AddAtEnd(metadata);
    packet->AddHeader(orchHeader);

    // Track the pending workload
    PendingWorkload pw;
    pw.dag = dag;
    pw.submitTime = Simulator::Now();
    m_pendingWorkloads[dagId] = pw;

    // Send to orchestrator
    m_connMgr->Send(packet);

    m_taskCount++;
    m_totalTx += packet->GetSize();

    NS_LOG_INFO("Client " << m_clientId << " sent ADMISSION_REQUEST for dagId " << dagId
                          << " (task " << task->GetTaskId() << ")");

    m_taskSentTrace(task);
}

void
OffloadClient::GenerateTask()
{
    NS_LOG_FUNCTION(this);

    // Generate task parameters from random distributions
    uint64_t inputSize = static_cast<uint64_t>(m_inputSize->GetValue());
    uint64_t outputSize = static_cast<uint64_t>(m_outputSize->GetValue());
    double computeDemand = m_computeDemand->GetValue();

    // Create a SimpleTask
    Ptr<SimpleTask> task = CreateObject<SimpleTask>();
    task->SetComputeDemand(computeDemand);
    task->SetInputSize(inputSize);
    task->SetOutputSize(outputSize);

    // Submit via the admission protocol
    SubmitTask(task);

    // Schedule next task if not at limit
    if (m_maxTasks == 0 || m_taskCount < m_maxTasks)
    {
        ScheduleNextTask();
    }
}

void
OffloadClient::ScheduleNextTask()
{
    NS_LOG_FUNCTION(this);

    if (m_sendEvent.IsPending())
    {
        return;
    }

    Time nextTime = Seconds(m_interArrivalTime->GetValue());
    m_sendEvent = Simulator::Schedule(nextTime, &OffloadClient::GenerateTask, this);

    NS_LOG_DEBUG("Next task scheduled in " << nextTime.GetSeconds() << " seconds");
}

void
OffloadClient::ProcessBuffer()
{
    NS_LOG_FUNCTION(this);

    while (m_rxBuffer->GetSize() > 0)
    {
        uint32_t sizeBefore = m_rxBuffer->GetSize();

        // Peek first byte for message type demux
        uint8_t firstByte;
        m_rxBuffer->CopyData(&firstByte, 1);

        if (firstByte >= OrchestratorHeader::ADMISSION_REQUEST)
        {
            // Admission response (OrchestratorHeader)
            if (m_rxBuffer->GetSize() < OrchestratorHeader::SERIALIZED_SIZE)
            {
                break; // Not enough data
            }

            OrchestratorHeader orchHeader;
            m_rxBuffer->PeekHeader(orchHeader);

            uint64_t totalSize = OrchestratorHeader::SERIALIZED_SIZE + orchHeader.GetPayloadSize();
            if (m_rxBuffer->GetSize() < totalSize)
            {
                break; // Not enough data
            }

            m_rxBuffer->RemoveAtStart(totalSize);
            HandleAdmissionResponse(orchHeader);
        }
        else
        {
            // Task response (SimpleTaskHeader-framed)
            HandleTaskResponse();
        }

        // If nothing was consumed, stop to avoid infinite loop
        if (m_rxBuffer->GetSize() == sizeBefore)
        {
            break;
        }
    }
}

void
OffloadClient::HandleAdmissionResponse(const OrchestratorHeader& orchHeader)
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
        NS_LOG_INFO("Client " << m_clientId << " admission ACCEPTED for dagId " << dagId);
        SendFullData(dagId);
    }
    else
    {
        NS_LOG_INFO("Client " << m_clientId << " admission REJECTED for dagId " << dagId);

        // Fire rejected trace for the task(s) in the DAG
        Ptr<DagTask> dag = it->second.dag;
        for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
        {
            Ptr<Task> task = dag->GetTask(i);
            if (task)
            {
                m_taskRejectedTrace(task);
            }
        }

        m_pendingWorkloads.erase(it);
    }
}

void
OffloadClient::HandleTaskResponse()
{
    NS_LOG_FUNCTION(this);

    // Use SimpleTask::Deserialize to parse the response
    uint64_t consumedBytes = 0;
    Ptr<Task> task = SimpleTask::Deserialize(m_rxBuffer, consumedBytes);

    if (consumedBytes == 0)
    {
        return; // Not enough data
    }

    m_rxBuffer->RemoveAtStart(consumedBytes);

    if (!task)
    {
        NS_LOG_WARN("Deserializer consumed " << consumedBytes << " bytes but returned null task");
        return;
    }

    uint64_t taskId = task->GetTaskId();

    // Find the pending workload that contains this task
    // The orchestrator restores original task IDs, so we match by task ID
    for (auto it = m_pendingWorkloads.begin(); it != m_pendingWorkloads.end(); ++it)
    {
        Ptr<DagTask> dag = it->second.dag;
        int32_t dagIdx = dag->GetTaskIndex(taskId);
        if (dagIdx >= 0)
        {
            Time rtt = Simulator::Now() - it->second.submitTime;
            m_responsesReceived++;

            NS_LOG_INFO("Client " << m_clientId << " received response for task " << taskId
                                  << " (RTT=" << rtt.GetMilliSeconds() << "ms)");

            m_responseReceivedTrace(task, rtt);

            // Mark completed and check if workload is done
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
OffloadClient::SendFullData(uint64_t dagId)
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

    NS_LOG_INFO("Client " << m_clientId << " sent full data for dagId " << dagId << " ("
                          << packet->GetSize() << " bytes)");
}

int64_t
OffloadClient::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    auto currentStream = stream;
    m_interArrivalTime->SetStream(currentStream++);
    m_computeDemand->SetStream(currentStream++);
    m_inputSize->SetStream(currentStream++);
    m_outputSize->SetStream(currentStream++);
    currentStream += Application::AssignStreams(currentStream);
    return (currentStream - stream);
}

} // namespace ns3
