/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-client.h"

#include "drop-while-pending-submission-policy.h"
#include "simple-task.h"
#include "tcp-connection-manager.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OffloadClient");

NS_OBJECT_ENSURE_REGISTERED(OffloadClient);

uint32_t OffloadClient::s_nextClientId = 0;

TypeId
OffloadClient::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OffloadClient")
            .SetParent<Application>()
            .SetGroupName("Continuum")
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
            .AddAttribute("SubmissionPolicy",
                          "Policy deciding whether to submit each scheduled task "
                          "(defaults to DropWhilePendingSubmissionPolicy)",
                          PointerValue(),
                          MakePointerAccessor(&OffloadClient::m_submissionPolicy),
                          MakePointerChecker<ClientSubmissionPolicy>())
            .AddAttribute("InterArrival",
                          "Random variable for inter-arrival time between tasks in seconds. "
                          "Default ConstantRandomVariable yields 30 Hz periodic arrivals.",
                          StringValue("ns3::ConstantRandomVariable[Constant=0.0333333333333333]"),
                          MakePointerAccessor(&OffloadClient::m_interArrival),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("DeadlineBudget",
                          "End-to-end delay budget per task. "
                          "When zero, defaults to the inter-arrival period sampled at startup.",
                          TimeValue(Seconds(0)),
                          MakeTimeAccessor(&OffloadClient::m_deadlineBudget),
                          MakeTimeChecker())
            .AddAttribute("CommunicationBudget",
                          "Estimated round-trip communication delay (T_ul + T_dl). "
                          "Subtracted from the deadline budget to yield the compute budget.",
                          TimeValue(Seconds(0)),
                          MakeTimeAccessor(&OffloadClient::m_commBudget),
                          MakeTimeChecker())
            .AddAttribute("InputSize",
                          "Random variable for input size per task in bytes",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&OffloadClient::m_inputSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("ComputeDemand",
                          "Random variable for compute demand per task in FLOPS",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&OffloadClient::m_computeDemand),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("OutputSize",
                          "Random variable for result size in bytes",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&OffloadClient::m_outputSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddTraceSource("TaskSent",
                            "Trace fired when a task admission request is sent",
                            MakeTraceSourceAccessor(&OffloadClient::m_taskSentTrace),
                            "ns3::OffloadClient::TaskSentTracedCallback")
            .AddTraceSource("TaskProcessed",
                            "Trace fired when a processed task result is received",
                            MakeTraceSourceAccessor(&OffloadClient::m_taskProcessedTrace),
                            "ns3::OffloadClient::TaskProcessedTracedCallback")
            .AddTraceSource("TaskRejected",
                            "Trace fired when a task admission is rejected",
                            MakeTraceSourceAccessor(&OffloadClient::m_taskRejectedTrace),
                            "ns3::OffloadClient::TaskRejectedTracedCallback")
            .AddTraceSource("TaskViolated",
                            "Trace fired when a processed task exceeds its deadline budget",
                            MakeTraceSourceAccessor(&OffloadClient::m_taskViolatedTrace),
                            "ns3::OffloadClient::TaskViolatedTracedCallback")
            .AddTraceSource(
                "TaskDropped",
                "Trace fired when a task is dropped because a prior task is still pending",
                MakeTraceSourceAccessor(&OffloadClient::m_taskDroppedTrace),
                "ns3::OffloadClient::TaskDroppedTracedCallback");
    return tid;
}

OffloadClient::OffloadClient()
    : m_connMgr(nullptr),
      m_submissionPolicy(nullptr),
      m_deadlineBudget(Seconds(0)),
      m_commBudget(Seconds(0)),
      m_nominalPeriod(Seconds(0)),
      m_clientId(s_nextClientId++),
      m_tasksSent(0),
      m_taskCount(0),
      m_tasksDropped(0),
      m_nextDagId(1),
      m_totalTx(0),
      m_totalRx(0),
      m_tasksRejected(0),
      m_tasksViolated(0),
      m_lastViolationIndex(0),
      m_lastViolationTime(Seconds(0)),
      m_lastRejectionTime(Seconds(0)),
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

    if (m_connMgr)
    {
        m_connMgr->Close();
        m_connMgr = nullptr;
    }

    m_submissionPolicy = nullptr;
    m_rxBuffer = nullptr;
    m_inputSize = nullptr;
    m_computeDemand = nullptr;
    m_outputSize = nullptr;
    m_interArrival = nullptr;
    m_pendingWorkloads.clear();

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
    return m_tasksSent;
}

uint64_t
OffloadClient::GetTasksDropped() const
{
    return m_tasksDropped;
}

uint64_t
OffloadClient::GetResponsesReceived() const
{
    return m_responsesReceived;
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

void
OffloadClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(m_peer.IsInvalid(), "Remote address not set");

    if (!m_connMgr)
    {
        m_connMgr = CreateObject<TcpConnectionManager>();
    }

    if (!m_submissionPolicy)
    {
        m_submissionPolicy = CreateObject<DropWhilePendingSubmissionPolicy>();
    }

    m_nominalPeriod = Seconds(m_interArrival->GetValue());

    m_connMgr->SetNode(GetNode());
    m_connMgr->SetReceiveCallback(MakeCallback(&OffloadClient::HandleReceive, this));

    Ptr<TcpConnectionManager> tcpConnMgr = DynamicCast<TcpConnectionManager>(m_connMgr);
    if (tcpConnMgr)
    {
        tcpConnMgr->SetConnectionCallback(MakeCallback(&OffloadClient::HandleConnected, this));
        tcpConnMgr->SetConnectionFailedCallback(
            MakeCallback(&OffloadClient::HandleConnectionFailed, this));
    }

    m_connMgr->Connect(m_peer);

    if (!tcpConnMgr)
    {
        ScheduleNextTask();
    }
}

void
OffloadClient::StopApplication()
{
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);

    if (m_connMgr)
    {
        m_connMgr->Close();
    }
}

void
OffloadClient::HandleConnected(const Address& serverAddr)
{
    NS_LOG_FUNCTION(this << serverAddr);
    NS_LOG_INFO("OffloadClient " << m_clientId << " connected to orchestrator " << serverAddr);

    ScheduleNextTask();
}

void
OffloadClient::HandleConnectionFailed(const Address& serverAddr)
{
    NS_LOG_FUNCTION(this << serverAddr);
    NS_LOG_ERROR("OffloadClient " << m_clientId << " failed to connect to " << serverAddr);
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
    NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from " << from);
    m_rxBuffer->AddAtEnd(packet);
    ProcessBuffer();
}

void
OffloadClient::GenerateTask()
{
    NS_LOG_FUNCTION(this);

    if (!m_connMgr || !m_connMgr->IsConnected())
    {
        NS_LOG_DEBUG("Not connected, cannot submit task");
        return;
    }

    m_taskCount++;

    ClientSubmissionPolicy::ClientState st;
    st.pendingCount = static_cast<uint32_t>(m_pendingWorkloads.size());
    st.submissionCount = m_taskCount;
    st.submissionsSent = m_tasksSent;
    st.submissionsDropped = m_tasksDropped;
    st.submissionsViolated = m_tasksViolated;
    st.submissionsRejected = m_tasksRejected;
    st.lastViolationIndex = m_lastViolationIndex;
    st.lastViolationTime = m_lastViolationTime;
    st.lastRejectionTime = m_lastRejectionTime;
    st.now = Simulator::Now();
    st.submissionRate = 1.0 / m_nominalPeriod.GetSeconds();

    if (!m_submissionPolicy->ShouldSubmit(st))
    {
        m_tasksDropped++;
        NS_LOG_INFO("OffloadClient " << m_clientId << " dropped task " << m_taskCount
                                     << " (policy=" << m_submissionPolicy->GetName() << ")");
        m_taskDroppedTrace(m_taskCount);
        ScheduleNextTask();
        return;
    }

    uint64_t inputSize = static_cast<uint64_t>(m_inputSize->GetValue());
    double computeDemand = m_computeDemand->GetValue();
    uint64_t outputSize = static_cast<uint64_t>(m_outputSize->GetValue());

    Ptr<SimpleTask> task = CreateObject<SimpleTask>();
    task->SetComputeDemand(computeDemand);
    task->SetInputSize(inputSize);
    task->SetOutputSize(outputSize);

    Time budget = m_deadlineBudget.IsStrictlyPositive() ? m_deadlineBudget : m_nominalPeriod;
    Time computeBudget = budget - m_commBudget;
    task->SetDeadline(Simulator::Now() + computeBudget);

    uint64_t taskId = (static_cast<uint64_t>(m_clientId) << 32) | m_tasksSent;
    task->SetTaskId(taskId);

    Ptr<DagTask> dag = CreateObject<DagTask>();
    dag->AddTask(task);

    uint64_t dagId = (static_cast<uint64_t>(m_clientId) << 32) | m_nextDagId++;

    Ptr<Packet> metadata = dag->SerializeMetadata();

    OrchestratorHeader orchHeader;
    orchHeader.SetMessageType(OrchestratorHeader::ADMISSION_REQUEST);
    orchHeader.SetDagId(dagId);
    orchHeader.SetPayloadSize(metadata->GetSize());

    Ptr<Packet> packet = Create<Packet>();
    packet->AddAtEnd(metadata);
    packet->AddHeader(orchHeader);

    if (!m_connMgr->Send(packet))
    {
        NS_LOG_WARN("OffloadClient " << m_clientId << " failed to send admission request for dagId "
                                     << dagId);
        m_tasksDropped++;
        m_taskDroppedTrace(m_taskCount);
        ScheduleNextTask();
        return;
    }

    PendingWorkload pw;
    pw.dag = dag;
    pw.submitTime = Simulator::Now();
    m_pendingWorkloads[dagId] = pw;

    m_tasksSent++;
    m_totalTx += packet->GetSize();

    NS_LOG_INFO("OffloadClient " << m_clientId << " sent task " << m_tasksSent << " (dagId "
                                 << dagId << ", " << inputSize << " bytes, " << computeDemand
                                 << " FLOPS)");

    task->SetState(TASK_SUBMITTED);
    m_taskSentTrace(task);

    ScheduleNextTask();
}

void
OffloadClient::ScheduleNextTask()
{
    NS_LOG_FUNCTION(this);

    if (m_sendEvent.IsPending())
    {
        return;
    }

    Time interval = Seconds(m_interArrival->GetValue());
    m_sendEvent = Simulator::Schedule(interval, &OffloadClient::GenerateTask, this);

    NS_LOG_DEBUG("Next task scheduled in " << interval.GetMilliSeconds() << " ms");
}

void
OffloadClient::ProcessBuffer()
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
OffloadClient::HandleAdmissionResponse(const OrchestratorHeader& orchHeader)
{
    NS_LOG_FUNCTION(this << orchHeader.GetDagId() << orchHeader.IsAdmitted());

    uint64_t dagId = orchHeader.GetDagId();

    auto it = m_pendingWorkloads.find(dagId);
    if (it == m_pendingWorkloads.end())
    {
        NS_LOG_WARN("Received admission response for unknown dagId " << dagId);
        return;
    }

    if (orchHeader.IsAdmitted())
    {
        NS_LOG_INFO("OffloadClient " << m_clientId << " admission ACCEPTED for dagId " << dagId);
        SendFullData(dagId);
    }
    else
    {
        NS_LOG_INFO("OffloadClient " << m_clientId << " admission REJECTED for dagId " << dagId);

        Ptr<DagTask> dag = it->second.dag;
        for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
        {
            Ptr<Task> task = dag->GetTask(i);
            if (task)
            {
                task->SetState(TASK_REJECTED);
                m_tasksRejected++;
                m_lastRejectionTime = Simulator::Now();
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

            Time budget =
                m_deadlineBudget.IsStrictlyPositive() ? m_deadlineBudget : m_nominalPeriod;

            if (latency > budget)
            {
                NS_LOG_INFO("OffloadClient " << m_clientId << " deadline violated for task "
                                             << taskId << " (latency=" << latency.GetMilliSeconds()
                                             << "ms, budget=" << budget.GetMilliSeconds() << "ms)");
                m_tasksViolated++;
                m_lastViolationIndex = m_taskCount;
                m_lastViolationTime = Simulator::Now();
                m_taskViolatedTrace(task, latency);
            }
            else
            {
                NS_LOG_INFO("OffloadClient " << m_clientId << " received result for task " << taskId
                                             << " (latency=" << latency.GetMilliSeconds() << "ms)");
            }

            m_taskProcessedTrace(task, latency);

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
    Ptr<Packet> dagData = dag->SerializeFullData();

    OrchestratorHeader uploadHeader;
    uploadHeader.SetMessageType(OrchestratorHeader::DATA_UPLOAD);
    uploadHeader.SetDagId(dagId);
    uploadHeader.SetPayloadSize(dagData->GetSize());

    Ptr<Packet> packet = Create<Packet>();
    packet->AddAtEnd(dagData);
    packet->AddHeader(uploadHeader);

    if (!m_connMgr->Send(packet))
    {
        NS_LOG_WARN("OffloadClient " << m_clientId << " failed to send full data for dagId "
                                     << dagId);
        for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
        {
            Ptr<Task> task = dag->GetTask(i);
            if (task)
            {
                m_tasksRejected++;
                m_lastRejectionTime = Simulator::Now();
                m_taskRejectedTrace(task);
            }
        }
        m_pendingWorkloads.erase(it);
        return;
    }

    m_totalTx += packet->GetSize();

    NS_LOG_INFO("OffloadClient " << m_clientId << " sent full task data for dagId " << dagId << " ("
                                 << packet->GetSize() << " bytes)");
}

int64_t
OffloadClient::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    auto currentStream = stream;
    m_inputSize->SetStream(currentStream++);
    m_computeDemand->SetStream(currentStream++);
    m_outputSize->SetStream(currentStream++);
    m_interArrival->SetStream(currentStream++);
    currentStream += Application::AssignStreams(currentStream);
    return (currentStream - stream);
}

} // namespace ns3
