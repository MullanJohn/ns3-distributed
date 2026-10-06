/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-client.h"

#include "drop-while-pending-submission-policy.h"
#include "never-retry-admission-policy.h"
#include "simple-task.h"
#include "tcp-connection-manager.h"

#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

#include <vector>

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
            .AddAttribute("AdmissionMode",
                          "SessionMode (default): admission is established once, and "
                          "subsequent workloads are implicitly admitted with no further "
                          "round trip. WorkloadMode: every workload is individually "
                          "admitted via a combined request (data included), which the "
                          "orchestrator dispatches immediately upon acceptance.",
                          EnumValue(OffloadClient::SESSION_MODE),
                          MakeEnumAccessor<OffloadClient::AdmissionMode>(
                              &OffloadClient::m_admissionMode),
                          MakeEnumChecker(OffloadClient::SESSION_MODE,
                                         "SessionMode",
                                         OffloadClient::WORKLOAD_MODE,
                                         "WorkloadMode"))
            .AddAttribute("AdmissionRetryPolicy",
                          "SessionMode-only: decides whether to retry session "
                          "establishment after a rejection (defaults to "
                          "NeverRetryAdmissionPolicy). Ignored in WorkloadMode.",
                          PointerValue(),
                          MakePointerAccessor(&OffloadClient::m_admissionRetryPolicy),
                          MakePointerChecker<AdmissionRetryPolicy>())
            .AddAttribute("DagTemplate",
                          "Template DAG (structure and per-task distributions) that each "
                          "generated workload is instantiated from (see "
                          "DagTask::Instantiate()). Defaults to a single SimpleTask with "
                          "InputSize=OutputSize=1, ComputeDemand=1.0 (i.e. a single task "
                          "with no edges), matching historical behavior.",
                          PointerValue(),
                          MakePointerAccessor(&OffloadClient::m_dagTemplate),
                          MakePointerChecker<DagTask>())
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
            .AddAttribute("GracePeriod",
                          "On stop, generation halts immediately but the connection is kept "
                          "open for up to this long to let any already-pending workload's "
                          "response still arrive (closed early as soon as it resolves). "
                          "Default 0 closes the connection immediately on stop, matching "
                          "historical behavior.",
                          TimeValue(Seconds(0)),
                          MakeTimeAccessor(&OffloadClient::m_gracePeriod),
                          MakeTimeChecker())
            .AddTraceSource("TaskSent",
                            "Trace fired when a task admission request is sent",
                            MakeTraceSourceAccessor(&OffloadClient::m_taskSentTrace),
                            "ns3::OffloadClient::TaskSentTracedCallback")
            .AddTraceSource("WorkloadProcessed",
                            "Trace fired when a completed workload DAG is received",
                            MakeTraceSourceAccessor(&OffloadClient::m_workloadProcessedTrace),
                            "ns3::OffloadClient::WorkloadProcessedTracedCallback")
            .AddTraceSource("TaskRejected",
                            "Trace fired when a task admission is rejected",
                            MakeTraceSourceAccessor(&OffloadClient::m_taskRejectedTrace),
                            "ns3::OffloadClient::TaskRejectedTracedCallback")
            .AddTraceSource("WorkloadViolated",
                            "Trace fired when a completed workload exceeds its deadline budget",
                            MakeTraceSourceAccessor(&OffloadClient::m_workloadViolatedTrace),
                            "ns3::OffloadClient::WorkloadViolatedTracedCallback")
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
      m_admissionMode(SESSION_MODE),
      m_admissionRetryPolicy(nullptr),
      m_dagTemplate(nullptr),
      m_deadlineBudget(Seconds(0)),
      m_commBudget(Seconds(0)),
      m_gracePeriod(Seconds(0)),
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
      m_sessionAdmitted(false),
      m_sessionAdmissionAbandoned(false),
      m_sessionRejectionCount(0),
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
    Simulator::Cancel(m_closeEvent);

    if (m_connMgr)
    {
        m_connMgr->Close();
        m_connMgr = nullptr;
    }

    m_submissionPolicy = nullptr;
    m_admissionRetryPolicy = nullptr;
    m_dagTemplate = nullptr;
    m_rxBuffer = nullptr;
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
OffloadClient::EnsureDagTemplate()
{
    if (m_dagTemplate)
    {
        return;
    }

    NS_LOG_FUNCTION(this);

    Ptr<SimpleTask> task = CreateObject<SimpleTask>();
    task->SetInputSize(1);
    task->SetOutputSize(1);
    task->SetComputeDemand(1.0);

    m_dagTemplate = CreateObject<DagTask>();
    m_dagTemplate->AddTask(task);
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

    if (!m_admissionRetryPolicy)
    {
        m_admissionRetryPolicy = CreateObject<NeverRetryAdmissionPolicy>();
    }

    EnsureDagTemplate();

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

    // Generation always halts immediately, regardless of GracePeriod.
    Simulator::Cancel(m_sendEvent);

    if (m_gracePeriod.IsZero() || m_pendingWorkloads.empty())
    {
        CloseConnection();
        return;
    }

    NS_LOG_INFO("OffloadClient " << m_clientId << " draining " << m_pendingWorkloads.size()
                                 << " pending workload(s) for up to "
                                 << m_gracePeriod.As(Time::MS));
    m_closeEvent = Simulator::Schedule(m_gracePeriod, &OffloadClient::CloseConnection, this);
}

void
OffloadClient::CloseConnection()
{
    NS_LOG_FUNCTION(this);

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

    if (m_admissionMode == SESSION_MODE && m_sessionAdmissionAbandoned)
    {
        // Gave up on establishing a session (AdmissionRetryPolicy said not to
        // retry) -- stay permanently quiet, do not reschedule.
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

    Time budget = m_deadlineBudget.IsStrictlyPositive() ? m_deadlineBudget : m_nominalPeriod;
    Time computeBudget = budget - m_commBudget;
    Time deadline = Simulator::Now() + computeBudget;

    Ptr<DagTask> dag = m_dagTemplate->Instantiate();
    uint32_t taskCount = dag->GetTaskCount();
    std::vector<Ptr<Task>> tasks;
    tasks.reserve(taskCount);
    uint64_t totalInputSize = 0;
    double totalComputeDemand = 0;

    for (uint32_t i = 0; i < taskCount; i++)
    {
        Ptr<Task> task = dag->GetTask(i);
        task->SetDeadline(deadline);

        // Task-local index (bits 8..15) keeps IDs unique within a multi-task
        // DAG; m_tasksSent (bits 32+) keeps them unique across ticks.
        uint64_t taskId =
            (static_cast<uint64_t>(m_clientId) << 32) | (m_tasksSent << 8) | i;
        task->SetTaskId(taskId);

        tasks.push_back(task);

        totalInputSize += task->GetInputSize();
        totalComputeDemand += task->GetComputeDemand();
    }

    uint64_t dagId = (static_cast<uint64_t>(m_clientId) << 32) | m_nextDagId++;

    // WorkloadMode always declares 0 (each workload is independently admitted,
    // so there is no session-level rate to declare). SessionMode declares its
    // nominal generation rate every time; the orchestrator only acts on it
    // (remembering the session as admitted) the first time it sees this client.
    double declaredRate =
        (m_admissionMode == SESSION_MODE) ? 1.0 / m_nominalPeriod.GetSeconds() : 0.0;
    dag->SetDeclaredRate(declaredRate);

    Ptr<Packet> dagData = dag->SerializeFullData();

    OrchestratorHeader orchHeader;
    orchHeader.SetMessageType(OrchestratorHeader::WORKLOAD_REQUEST);
    orchHeader.SetDagId(dagId);
    orchHeader.SetPayloadSize(dagData->GetSize());

    Ptr<Packet> packet = Create<Packet>();
    packet->AddAtEnd(dagData);
    packet->AddHeader(orchHeader);

    if (!m_connMgr->Send(packet))
    {
        NS_LOG_WARN("OffloadClient " << m_clientId << " failed to send workload request for dagId "
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

    NS_LOG_INFO("OffloadClient " << m_clientId << " sent workload " << m_tasksSent << " (dagId "
                                 << dagId << ", " << taskCount << " tasks, " << totalInputSize
                                 << " bytes, " << totalComputeDemand << " FLOPS)");

    for (const auto& task : tasks)
    {
        task->SetState(TASK_SUBMITTED);
        m_taskSentTrace(task);
    }

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

        if (m_rxBuffer->GetSize() < OrchestratorHeader::SERIALIZED_SIZE)
        {
            break;
        }

        OrchestratorHeader orchHeader;
        m_rxBuffer->PeekHeader(orchHeader);

        uint64_t payloadSize = orchHeader.GetPayloadSize();
        uint64_t totalSize = OrchestratorHeader::SERIALIZED_SIZE + payloadSize;
        if (m_rxBuffer->GetSize() < totalSize)
        {
            break;
        }

        // Capture the payload before consuming the message from the buffer.
        Ptr<Packet> payload =
            payloadSize > 0
                ? m_rxBuffer->CreateFragment(OrchestratorHeader::SERIALIZED_SIZE, payloadSize)
                : Create<Packet>();

        m_rxBuffer->RemoveAtStart(totalSize);

        if (orchHeader.IsWorkloadResponse())
        {
            HandleWorkloadResponse(orchHeader.GetDagId(), payload);
        }
        else
        {
            HandleAdmissionResponse(orchHeader);
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

        if (m_admissionMode == SESSION_MODE && !m_sessionAdmitted)
        {
            m_sessionAdmitted = true;
            m_sessionRejectionCount = 0;
            NS_LOG_INFO("OffloadClient " << m_clientId << " session admitted; subsequent "
                                         "workloads will be implicitly admitted");
        }

        // Data was already sent with the request in both modes -- nothing
        // further to do here; just wait for the eventual WorkloadResponse.
        return;
    }

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

    if (m_closeEvent.IsPending() && m_pendingWorkloads.empty())
    {
        Simulator::Cancel(m_closeEvent);
        CloseConnection();
    }

    if (m_admissionMode == SESSION_MODE && !m_sessionAdmitted)
    {
        m_sessionRejectionCount++;
        if (!m_admissionRetryPolicy->ShouldRetry(m_sessionRejectionCount))
        {
            m_sessionAdmissionAbandoned = true;
            NS_LOG_INFO("OffloadClient " << m_clientId << " giving up on session admission "
                                         "after " << m_sessionRejectionCount << " rejection(s)");
        }
        else
        {
            NS_LOG_INFO("OffloadClient " << m_clientId << " will retry session admission "
                                         "(rejection " << m_sessionRejectionCount << ")");
        }
    }
}

Ptr<Task>
OffloadClient::DeserializeResponseTask(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    consumedBytes = 0;

    if (packet->GetSize() < 1)
    {
        return nullptr;
    }

    // Skip the 1-byte task type tag DagTask prepends to each task.
    Ptr<Packet> taskPacket = packet->CreateFragment(1, packet->GetSize() - 1);
    uint64_t taskConsumed = 0;
    Ptr<Task> task = SimpleTask::Deserialize(taskPacket, taskConsumed);

    if (taskConsumed > 0)
    {
        consumedBytes = 1 + taskConsumed;
    }

    return task;
}

void
OffloadClient::HandleWorkloadResponse(uint64_t dagId, Ptr<Packet> payload)
{
    NS_LOG_FUNCTION(this << dagId);

    auto it = m_pendingWorkloads.find(dagId);
    if (it == m_pendingWorkloads.end())
    {
        NS_LOG_WARN("Received workload response for unknown dagId " << dagId);
        return;
    }

    uint64_t consumedBytes = 0;
    Ptr<DagTask> responseDag =
        DagTask::DeserializeFullData(payload,
                                     MakeCallback(&OffloadClient::DeserializeResponseTask),
                                     consumedBytes);

    if (!responseDag)
    {
        NS_LOG_WARN("Failed to deserialize workload response for dagId " << dagId);
        return;
    }

    Time latency = Simulator::Now() - it->second.submitTime;
    m_responsesReceived++;

    Time budget = m_deadlineBudget.IsStrictlyPositive() ? m_deadlineBudget : m_nominalPeriod;

    if (latency > budget)
    {
        NS_LOG_INFO("OffloadClient " << m_clientId << " deadline violated for dagId " << dagId
                                     << " (latency=" << latency.GetMilliSeconds()
                                     << "ms, budget=" << budget.GetMilliSeconds() << "ms)");
        m_tasksViolated++;
        m_lastViolationIndex = m_taskCount;
        m_lastViolationTime = Simulator::Now();
        m_workloadViolatedTrace(responseDag, latency);
    }
    else
    {
        NS_LOG_INFO("OffloadClient " << m_clientId << " received result for dagId " << dagId
                                     << " (latency=" << latency.GetMilliSeconds() << "ms)");
    }

    m_workloadProcessedTrace(responseDag, latency);

    // Arrival of this message is itself the completion signal: the orchestrator
    // only sends it once the whole DAG has finished executing.
    m_pendingWorkloads.erase(it);

    if (m_closeEvent.IsPending() && m_pendingWorkloads.empty())
    {
        Simulator::Cancel(m_closeEvent);
        CloseConnection();
    }
}

int64_t
OffloadClient::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    EnsureDagTemplate();
    auto currentStream = stream;
    m_interArrival->SetStream(currentStream++);
    currentStream += m_dagTemplate->AssignStreams(currentStream);
    currentStream += Application::AssignStreams(currentStream);
    return (currentStream - stream);
}

} // namespace ns3
