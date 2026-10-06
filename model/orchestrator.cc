/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "orchestrator.h"

#include "device-manager.h"
#include "simple-task.h"
#include "tcp-connection-manager.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Orchestrator");

NS_OBJECT_ENSURE_REGISTERED(Orchestrator);

TypeId
Orchestrator::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Orchestrator")
            .SetParent<Application>()
            .SetGroupName("Continuum")
            .AddConstructor<Orchestrator>()
            .AddAttribute("Port",
                          "Port on which to listen for incoming connections",
                          UintegerValue(8080),
                          MakeUintegerAccessor(&Orchestrator::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Scheduler",
                          "Task scheduler for backend selection",
                          PointerValue(),
                          MakePointerAccessor(&Orchestrator::m_scheduler),
                          MakePointerChecker<ClusterScheduler>())
            .AddAttribute("AdmissionPolicy",
                          "Admission policy for workload acceptance (nullptr = always admit)",
                          PointerValue(),
                          MakePointerAccessor(&Orchestrator::m_admissionPolicy),
                          MakePointerChecker<AdmissionPolicy>())
            .AddAttribute("ClientConnectionManager",
                          "Connection manager for client transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&Orchestrator::m_clientConnMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddAttribute("BackendConnectionManager",
                          "Connection manager for backend transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&Orchestrator::m_backendConnMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddAttribute("DeviceManager",
                          "DVFS device manager for backend scaling (optional)",
                          PointerValue(),
                          MakePointerAccessor(&Orchestrator::m_deviceManager),
                          MakePointerChecker<DeviceManager>())
            .AddAttribute("EnableNetworkDelayTracking",
                          "Enable live measurement of per-backend network delay via "
                          "RTT-residual estimation (default: disabled, schedulers fall back "
                          "to the static Cluster::Backend commDelay/commCapacity)",
                          BooleanValue(false),
                          MakeBooleanAccessor(&Orchestrator::m_enableNetworkDelayTracking),
                          MakeBooleanChecker())
            .AddAttribute("NetworkDelaySmoothingFactor",
                          "EWMA smoothing factor in (0, 1] applied to new network-delay "
                          "samples when EnableNetworkDelayTracking is true",
                          DoubleValue(0.3),
                          MakeDoubleAccessor(&Orchestrator::m_networkDelaySmoothingFactor),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddTraceSource("WorkloadAdmitted",
                            "A workload has been admitted for execution",
                            MakeTraceSourceAccessor(&Orchestrator::m_workloadAdmittedTrace),
                            "ns3::Orchestrator::WorkloadAdmittedTracedCallback")
            .AddTraceSource("WorkloadRejected",
                            "A workload has been rejected",
                            MakeTraceSourceAccessor(&Orchestrator::m_workloadRejectedTrace),
                            "ns3::Orchestrator::WorkloadRejectedTracedCallback")
            .AddTraceSource("WorkloadCancelled",
                            "A workload has been cancelled",
                            MakeTraceSourceAccessor(&Orchestrator::m_workloadCancelledTrace),
                            "ns3::Orchestrator::WorkloadCancelledTracedCallback")
            .AddTraceSource("TaskDispatched",
                            "A task has been dispatched to a backend",
                            MakeTraceSourceAccessor(&Orchestrator::m_taskDispatchedTrace),
                            "ns3::Orchestrator::TaskDispatchedTracedCallback")
            .AddTraceSource("TaskCompleted",
                            "A task has been completed by a backend",
                            MakeTraceSourceAccessor(&Orchestrator::m_taskCompletedTrace),
                            "ns3::Orchestrator::TaskCompletedTracedCallback")
            .AddTraceSource("WorkloadCompleted",
                            "A workload has been fully completed",
                            MakeTraceSourceAccessor(&Orchestrator::m_workloadCompletedTrace),
                            "ns3::Orchestrator::WorkloadCompletedTracedCallback");
    return tid;
}

Orchestrator::Orchestrator()
    : m_admissionPolicy(nullptr),
      m_scheduler(nullptr),
      m_deviceManager(nullptr),
      m_port(8080),
      m_clientConnMgr(nullptr),
      m_backendConnMgr(nullptr)
{
    NS_LOG_FUNCTION(this);
}

Orchestrator::~Orchestrator()
{
    NS_LOG_FUNCTION(this);
}

uint64_t
Orchestrator::PeekTaskId(Ptr<Packet> buffer)
{
    uint8_t prefix[9];
    buffer->CopyData(prefix, 9);
    uint64_t taskId = 0;
    for (int j = 1; j < 9; j++)
    {
        taskId = (taskId << 8) | prefix[j];
    }
    return taskId;
}

bool
Orchestrator::CancelWorkload(uint64_t workloadId)
{
    NS_LOG_FUNCTION(this << workloadId);

    auto it = m_workloads.find(workloadId);
    if (it == m_workloads.end())
    {
        return false;
    }

    NS_LOG_WARN("Cancelling workload " << workloadId);

    WorkloadState state = std::move(it->second);
    m_workloads.erase(it);

    for (const auto& [taskId, backendIdx] : state.taskToBackend)
    {
        double computeDemand = 0.0;
        int32_t dagIdx = state.dag ? state.dag->GetTaskIndex(taskId) : -1;
        if (dagIdx >= 0)
        {
            Ptr<Task> task = state.dag->GetTask(static_cast<uint32_t>(dagIdx));
            if (task)
            {
                computeDemand = task->GetComputeDemand();
            }
        }
        m_clusterState.NotifyTaskCompleted(backendIdx, computeDemand);
    }

    m_workloadsCancelled++;
    m_workloadCancelledTrace(workloadId);
    m_clusterState.SetActiveWorkloadCount(static_cast<uint32_t>(m_workloads.size()));

    return true;
}

void
Orchestrator::RejectWorkload(uint32_t taskCount, const std::string& reason)
{
    NS_LOG_FUNCTION(this << taskCount << reason);
    m_workloadsRejected++;
    m_workloadRejectedTrace(taskCount, reason);
}

void
Orchestrator::CleanupConnectionManager(Ptr<ConnectionManager>& connMgr)
{
    NS_LOG_FUNCTION(this);

    if (!connMgr)
    {
        return;
    }

    connMgr->SetReceiveCallback(ConnectionManager::ReceiveCallback());
    Ptr<TcpConnectionManager> tcpMgr = DynamicCast<TcpConnectionManager>(connMgr);
    if (tcpMgr)
    {
        tcpMgr->SetCloseCallback(TcpConnectionManager::ConnectionCallback());
    }
    connMgr->Close();
    connMgr = nullptr;
}

std::map<Address, Ptr<Packet>>::iterator
Orchestrator::AppendToBuffer(std::map<Address, Ptr<Packet>>& bufferMap,
                             const Address& addr,
                             Ptr<Packet> packet)
{
    auto it = bufferMap.find(addr);
    if (it == bufferMap.end())
    {
        it = bufferMap.emplace(addr, packet->Copy()).first;
    }
    else
    {
        it->second->AddAtEnd(packet);
    }
    return it;
}

void
Orchestrator::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_sessionAdmittedClients.clear();
    CleanupConnectionManager(m_clientConnMgr);
    CleanupConnectionManager(m_backendConnMgr);
    m_rxBuffer.clear();
    m_backendRxBuffer.clear();

    m_workloads.clear();
    m_admissionPolicy = nullptr;
    m_scheduler = nullptr;
    m_deviceManager = nullptr;
    m_taskTypeRegistry.clear();
    m_dispatchedTasks.clear();
    m_cluster.Clear();
    m_clusterState.Clear();

    Application::DoDispose();
}

void
Orchestrator::SetCluster(const Cluster& cluster)
{
    NS_LOG_FUNCTION(this);
    m_cluster = cluster;
}

const Cluster&
Orchestrator::GetCluster() const
{
    return m_cluster;
}

const ClusterState&
Orchestrator::GetClusterState() const
{
    return m_clusterState;
}

void
Orchestrator::RegisterTaskType(uint8_t taskType,
                               DeserializerCallback fullDeserializer,
                               DeserializerCallback metadataDeserializer)
{
    NS_LOG_FUNCTION(this << (int)taskType);
    m_taskTypeRegistry[taskType] = {fullDeserializer, metadataDeserializer};
}

Ptr<Task>
Orchestrator::DispatchDeserializeImpl(Ptr<Packet> packet,
                                      uint64_t& consumedBytes,
                                      bool metadataOnly)
{
    consumedBytes = 0;

    if (packet->GetSize() < 1)
    {
        return nullptr;
    }

    uint8_t taskType;
    packet->CopyData(&taskType, 1);

    auto regIt = m_taskTypeRegistry.find(taskType);
    if (regIt == m_taskTypeRegistry.end())
    {
        NS_LOG_ERROR("No deserializer registered for task type " << (int)taskType);
        return nullptr;
    }

    Ptr<Packet> subPacket = packet->CreateFragment(1, packet->GetSize() - 1);

    uint64_t subConsumed = 0;
    const DeserializerCallback& cb =
        metadataOnly ? regIt->second.metadataDeserializer : regIt->second.fullDeserializer;
    Ptr<Task> task = cb(subPacket, subConsumed);

    if (subConsumed > 0)
    {
        consumedBytes = 1 + subConsumed;
    }

    return task;
}

Ptr<Task>
Orchestrator::DispatchDeserialize(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    NS_LOG_FUNCTION(this << packet);
    return DispatchDeserializeImpl(packet, consumedBytes, false);
}

uint64_t
Orchestrator::GetWorkloadsAdmitted() const
{
    return m_workloadsAdmitted;
}

uint64_t
Orchestrator::GetWorkloadsRejected() const
{
    return m_workloadsRejected;
}

uint64_t
Orchestrator::GetWorkloadsCompleted() const
{
    return m_workloadsCompleted;
}

uint32_t
Orchestrator::GetActiveWorkloadCount() const
{
    return static_cast<uint32_t>(m_workloads.size());
}

uint64_t
Orchestrator::GetWorkloadsCancelled() const
{
    return m_workloadsCancelled;
}

Ptr<ClusterScheduler>
Orchestrator::GetScheduler() const
{
    return m_scheduler;
}

Ptr<AdmissionPolicy>
Orchestrator::GetAdmissionPolicy() const
{
    return m_admissionPolicy;
}

void
Orchestrator::StartApplication()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(!m_scheduler, "No Scheduler configured for Orchestrator");

    if (!m_clientConnMgr)
    {
        m_clientConnMgr = CreateObject<TcpConnectionManager>();
    }
    if (!m_backendConnMgr)
    {
        m_backendConnMgr = CreateObject<TcpConnectionManager>();
    }

    m_clientConnMgr->SetNode(GetNode());
    m_clientConnMgr->SetReceiveCallback(MakeCallback(&Orchestrator::HandleReceive, this));

    Ptr<TcpConnectionManager> tcpClientMgr = DynamicCast<TcpConnectionManager>(m_clientConnMgr);
    if (tcpClientMgr)
    {
        tcpClientMgr->SetCloseCallback(MakeCallback(&Orchestrator::HandleClientClose, this));
    }

    m_clientConnMgr->Bind(m_port);
    NS_LOG_INFO("Orchestrator listening on port " << m_port);

    m_backendConnMgr->SetNode(GetNode());
    m_backendConnMgr->SetReceiveCallback(MakeCallback(&Orchestrator::HandleBackendResponse, this));

    Ptr<TcpConnectionManager> tcpBackendMgr = DynamicCast<TcpConnectionManager>(m_backendConnMgr);
    if (tcpBackendMgr)
    {
        tcpBackendMgr->SetCloseCallback(MakeCallback(&Orchestrator::HandleBackendClose, this));
    }

    if (m_taskTypeRegistry.empty())
    {
        RegisterTaskType(SimpleTask::TASK_TYPE,
                         MakeCallback(&SimpleTask::Deserialize),
                         MakeCallback(&SimpleTask::DeserializeHeader));
        NS_LOG_DEBUG("Using default SimpleTask deserializer");
    }

    m_clusterState.Resize(m_cluster.GetN());

    for (uint32_t i = 0; i < m_cluster.GetN(); i++)
    {
        m_backendConnMgr->Connect(m_cluster.Get(i).address);
    }

    if (m_deviceManager)
    {
        m_deviceManager->Start(m_cluster, m_backendConnMgr, m_clusterState);
    }
}

void
Orchestrator::StopApplication()
{
    NS_LOG_FUNCTION(this);

    std::vector<uint64_t> activeIds;
    activeIds.reserve(m_workloads.size());
    for (const auto& pair : m_workloads)
    {
        activeIds.push_back(pair.first);
    }
    for (uint64_t wid : activeIds)
    {
        CancelWorkload(wid);
    }

    CleanupConnectionManager(m_clientConnMgr);
    CleanupConnectionManager(m_backendConnMgr);
}

void
Orchestrator::HandleReceive(Ptr<Packet> packet, const Address& from)
{
    NS_LOG_FUNCTION(this << packet << from);

    if (packet->GetSize() == 0)
    {
        return;
    }

    NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from client " << from);

    AppendToBuffer(m_rxBuffer, from, packet);
    ProcessClientBuffer(from);
}

void
Orchestrator::HandleClientClose(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);
    NS_LOG_INFO("Client disconnected: " << clientAddr);
    CleanupClient(clientAddr);
}

void
Orchestrator::HandleBackendClose(const Address& backendAddr)
{
    NS_LOG_FUNCTION(this << backendAddr);
    NS_LOG_WARN("Backend disconnected: " << backendAddr);

    m_backendRxBuffer.erase(backendAddr);

    int32_t backendIdx = m_cluster.GetBackendIndex(backendAddr);

    if (backendIdx < 0)
    {
        NS_LOG_DEBUG("Disconnected backend not found in cluster");
        return;
    }

    std::vector<uint64_t> affectedWorkloads;
    for (const auto& pair : m_workloads)
    {
        for (const auto& tb : pair.second.taskToBackend)
        {
            if (tb.second == static_cast<uint32_t>(backendIdx))
            {
                affectedWorkloads.push_back(pair.first);
                break;
            }
        }
    }

    for (uint64_t wid : affectedWorkloads)
    {
        NS_LOG_WARN("Cancelling workload " << wid << " due to backend " << backendIdx
                                           << " disconnect");
        CancelWorkload(wid);
    }
}

void
Orchestrator::ProcessClientBuffer(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);

    auto it = m_rxBuffer.find(clientAddr);
    if (it == m_rxBuffer.end())
    {
        return;
    }

    Ptr<Packet> buffer = it->second;
    static const uint32_t orchHeaderSize = OrchestratorHeader::SERIALIZED_SIZE;

    while (buffer->GetSize() >= orchHeaderSize)
    {
        OrchestratorHeader orchHeader;
        buffer->PeekHeader(orchHeader);

        uint64_t payloadSize = orchHeader.GetPayloadSize();
        uint64_t totalMessageSize = orchHeaderSize + payloadSize;
        if (buffer->GetSize() < totalMessageSize)
        {
            NS_LOG_DEBUG("Buffer has " << buffer->GetSize() << " bytes, need " << totalMessageSize
                                       << " for full message");
            break;
        }

        buffer->RemoveAtStart(orchHeaderSize);

        Ptr<Packet> payload = buffer->CreateFragment(0, payloadSize);
        buffer->RemoveAtStart(payloadSize);

        switch (orchHeader.GetMessageType())
        {
        case OrchestratorHeader::WORKLOAD_REQUEST:
            HandleWorkloadRequest(orchHeader.GetDagId(), payload, clientAddr);
            break;
        default:
            NS_LOG_WARN("Unknown message type " << static_cast<int>(orchHeader.GetMessageType())
                                                << " from client " << clientAddr << " — skipping");
            break;
        }
    }

    if (buffer->GetSize() == 0)
    {
        m_rxBuffer.erase(it);
    }
}

void
Orchestrator::SendAdmissionResponse(const Address& clientAddr, uint64_t dagId, bool admitted)
{
    NS_LOG_FUNCTION(this << clientAddr << dagId << admitted);

    OrchestratorHeader response;
    response.SetMessageType(OrchestratorHeader::ADMISSION_RESPONSE);
    response.SetDagId(dagId);
    response.SetAdmitted(admitted);
    response.SetPayloadSize(0);

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(response);

    // Nothing to roll back on a send failure here: dispatch (if admitted) has
    // already happened by the time this is called, since the confirmation is
    // a trailing, non-blocking acknowledgment, not a gate on processing.
    if (!m_clientConnMgr->Send(packet, clientAddr))
    {
        NS_LOG_WARN("Failed to send admission response for dagId " << dagId << " to "
                                                                    << clientAddr);
        return;
    }

    NS_LOG_DEBUG("Sent ADMISSION_RESPONSE for dagId " << dagId << ": "
                                                       << (admitted ? "admitted" : "rejected"));
}

bool
Orchestrator::CheckAdmission(Ptr<DagTask> dag)
{
    NS_LOG_FUNCTION(this);

    if (!m_admissionPolicy)
    {
        NS_LOG_DEBUG("No admission policy - admitting by default");
        return true;
    }

    return m_admissionPolicy->ShouldAdmit(dag, m_cluster, m_clusterState);
}

uint64_t
Orchestrator::CreateAndDispatchWorkload(Ptr<DagTask> dag,
                                        const Address& clientAddr,
                                        uint64_t dagId)
{
    NS_LOG_FUNCTION(this << dag << dagId);

    uint64_t workloadId = m_nextWorkloadId++;
    WorkloadState state;
    state.dag = dag;
    state.clientAddr = clientAddr;
    state.dagId = dagId;
    state.pendingTasks = 0;

    m_workloads[workloadId] = state;
    m_clusterState.SetActiveWorkloadCount(static_cast<uint32_t>(m_workloads.size()));

    for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
    {
        Ptr<Task> task = dag->GetTask(i);
        if (task)
        {
            task->SetState(TASK_ADMITTED);
        }
    }

    m_workloadsAdmitted++;
    m_workloadAdmittedTrace(workloadId, dag->GetTaskCount());

    if (!ProcessDagReadyTasks(workloadId))
    {
        return 0;
    }

    NS_LOG_INFO("Workload " << workloadId << " admitted (" << dag->GetTaskCount() << " tasks)");

    return workloadId;
}

int32_t
Orchestrator::DispatchTask(uint64_t workloadId, Ptr<Task> task)
{
    NS_LOG_FUNCTION(this << workloadId << task->GetTaskId());

    int32_t backendIdx = m_scheduler->ScheduleTask(task, m_cluster, m_clusterState);
    if (backendIdx < 0 || static_cast<uint32_t>(backendIdx) >= m_cluster.GetN())
    {
        NS_LOG_WARN("Scheduler returned invalid backend index " << backendIdx << " for task "
                                                                << task->GetTaskId());
        return -1;
    }

    const Cluster::Backend& backend = m_cluster.Get(backendIdx);

    auto wit = m_workloads.find(workloadId);
    NS_ASSERT_MSG(wit != m_workloads.end(),
                  "DispatchTask: workload " << workloadId << " not found");
    auto& state = wit->second;

    uint64_t taskId = task->GetTaskId();
    int32_t dagIdx = state.dag->GetTaskIndex(taskId);
    NS_ASSERT_MSG(dagIdx >= 0, "Task " << taskId << " not found in DAG");

    m_dispatchedTasks[taskId] = {workloadId,
                                 static_cast<uint32_t>(dagIdx),
                                 task->GetTaskType(),
                                 Simulator::Now()};

    state.taskToBackend[taskId] = backendIdx;
    state.pendingTasks++;

    m_clusterState.NotifyTaskDispatched(backendIdx, task->GetComputeDemand());

    if (m_deviceManager)
    {
        m_deviceManager->EvaluateScaling(m_clusterState);
    }

    Ptr<Packet> packet = task->Serialize(false);

    bool sent = m_backendConnMgr->Send(packet, backend.address);
    if (!sent)
    {
        NS_LOG_ERROR("Failed to send task to backend " << backendIdx);
        m_clusterState.UndoTaskDispatched(backendIdx, task->GetComputeDemand());
        m_dispatchedTasks.erase(taskId);
        state.taskToBackend.erase(taskId);
        state.pendingTasks--;
        return -1;
    }

    task->SetState(TASK_DISPATCHED);
    m_taskDispatchedTrace(workloadId, taskId, backendIdx);
    NS_LOG_INFO("Dispatched task " << taskId << " to backend " << backendIdx);

    return backendIdx;
}

void
Orchestrator::HandleBackendResponse(Ptr<Packet> packet, const Address& from)
{
    NS_LOG_FUNCTION(this << packet << from);

    if (packet->GetSize() == 0)
    {
        return;
    }

    NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from backend " << from);

    auto bufferIt = AppendToBuffer(m_backendRxBuffer, from, packet);
    Ptr<Packet> buffer = bufferIt->second;

    while (buffer->GetSize() > 0)
    {
        if (buffer->GetSize() < 9)
        {
            break;
        }

        if (m_deviceManager && m_deviceManager->TryConsumeMetrics(buffer, from, m_clusterState))
        {
            continue;
        }

        uint64_t taskId = PeekTaskId(buffer);

        auto dispIt = m_dispatchedTasks.find(taskId);
        if (dispIt == m_dispatchedTasks.end())
        {
            NS_LOG_ERROR("No dispatch info for task " << taskId);
            break;
        }

        DispatchedTaskInfo info = dispIt->second;

        auto regIt = m_taskTypeRegistry.find(info.taskType);
        if (regIt == m_taskTypeRegistry.end())
        {
            NS_LOG_ERROR("No deserializer for task type " << (int)info.taskType);
            break;
        }

        uint64_t consumedBytes = 0;
        Ptr<Task> task = regIt->second.fullDeserializer(buffer, consumedBytes);

        if (consumedBytes == 0)
        {
            break;
        }

        buffer->RemoveAtStart(consumedBytes);

        if (!task)
        {
            NS_LOG_ERROR("Deserializer consumed " << consumedBytes
                                                  << " bytes but returned null task " << taskId
                                                  << " — cancelling workload");
            m_dispatchedTasks.erase(dispIt);
            CancelWorkload(info.workloadId);
            continue;
        }

        m_dispatchedTasks.erase(dispIt);

        auto workloadIt = m_workloads.find(info.workloadId);
        if (workloadIt == m_workloads.end())
        {
            NS_LOG_WARN("Workload " << info.workloadId << " not found for task " << taskId);
            continue;
        }

        auto& state = workloadIt->second;
        auto backendIt = state.taskToBackend.find(taskId);
        if (backendIt == state.taskToBackend.end())
        {
            NS_LOG_ERROR("Backend index not found for task " << taskId << " in workload "
                                                             << info.workloadId
                                                             << " - skipping completion");
            continue;
        }
        uint32_t backendIdx = backendIt->second;

        OnTaskCompleted(info.workloadId, task, backendIdx, info.dispatchTime);
    }

    if (buffer->GetSize() == 0)
    {
        m_backendRxBuffer.erase(from);
    }
}

void
Orchestrator::OnTaskCompleted(uint64_t workloadId, Ptr<Task> task, uint32_t backendIdx, Time dispatchTime)
{
    NS_LOG_FUNCTION(this << workloadId << task->GetTaskId() << backendIdx << dispatchTime);

    auto it = m_workloads.find(workloadId);
    if (it == m_workloads.end())
    {
        NS_LOG_WARN("OnTaskCompleted: workload " << workloadId << " not found");
        return;
    }

    WorkloadState& state = it->second;
    uint64_t taskId = task->GetTaskId();

    m_scheduler->NotifyTaskCompleted(backendIdx, task);
    m_clusterState.NotifyTaskCompleted(backendIdx, task->GetComputeDemand());

    if (m_enableNetworkDelayTracking)
    {
        Time rtt = Simulator::Now() - dispatchTime;

        const ClusterState::BackendState& backendState = m_clusterState.Get(backendIdx);
        double effectiveRate = backendState.device.effectiveComputeRate > 0.0
                                   ? backendState.device.effectiveComputeRate
                                   : backendState.device.nominalComputeRate;
        Time estimatedComputeTime =
            effectiveRate > 0.0 ? Seconds(task->GetComputeDemand() / effectiveRate) : Seconds(0);

        // residual approximates round-trip network overhead beyond estimated compute
        // time; it also absorbs any backend-side queueing ahead of compute, so it is
        // not a pure propagation-delay measurement, just a useful placement signal.
        Time residual = rtt - estimatedComputeTime;
        Time oneWaySample = residual.IsNegative() ? Seconds(0) : residual / 2;

        m_clusterState.UpdateMeasuredNetworkDelay(backendIdx,
                                                  oneWaySample,
                                                  m_networkDelaySmoothingFactor);
    }

    if (m_deviceManager)
    {
        m_deviceManager->EvaluateScaling(m_clusterState);
    }

    m_taskCompletedTrace(workloadId, taskId, backendIdx);

    state.taskToBackend.erase(taskId);

    NS_ASSERT_MSG(state.pendingTasks > 0, "pendingTasks underflow for workload " << workloadId);
    state.pendingTasks--;

    NS_LOG_INFO("Task " << taskId << " completed on backend " << backendIdx);

    Ptr<DagTask> dag = state.dag;

    int32_t dagIdx = dag->GetTaskIndex(taskId);
    if (dagIdx < 0)
    {
        NS_LOG_ERROR("Task " << taskId << " not found in DAG for workload " << workloadId);
        return;
    }

    dag->SetTask(static_cast<uint32_t>(dagIdx), task);
    dag->MarkCompleted(static_cast<uint32_t>(dagIdx));

    if (dag->IsComplete())
    {
        CompleteWorkload(workloadId);
    }
    else
    {
        ProcessDagReadyTasks(workloadId);
    }
}

bool
Orchestrator::ProcessDagReadyTasks(uint64_t workloadId)
{
    NS_LOG_FUNCTION(this << workloadId);

    auto it = m_workloads.find(workloadId);
    NS_ASSERT_MSG(it != m_workloads.end(),
                  "ProcessDagReadyTasks: workload " << workloadId << " not found");

    WorkloadState& state = it->second;

    std::vector<uint32_t> readyIndices = state.dag->GetReadyTasks();

    for (uint32_t idx : readyIndices)
    {
        Ptr<Task> task = state.dag->GetTask(idx);

        if (state.taskToBackend.find(task->GetTaskId()) != state.taskToBackend.end())
        {
            continue;
        }

        int32_t backendIdx = DispatchTask(workloadId, task);
        if (backendIdx < 0)
        {
            NS_LOG_ERROR("Failed to dispatch DAG task " << task->GetTaskId() << " in workload "
                                                        << workloadId << " - failing workload");
            CancelWorkload(workloadId);
            return false;
        }
    }

    return true;
}

void
Orchestrator::CompleteWorkload(uint64_t workloadId)
{
    NS_LOG_FUNCTION(this << workloadId);

    auto it = m_workloads.find(workloadId);
    if (it == m_workloads.end())
    {
        return;
    }
    if (!it->second.clientAddr.IsInvalid())
    {
        if (!SendWorkloadResponse(it->second.clientAddr, it->second.dagId, it->second.dag))
        {
            NS_LOG_WARN("Failed to deliver result for workload " << workloadId << " — cancelling");
            CancelWorkload(workloadId);
            return;
        }
    }

    NS_LOG_INFO("Workload " << workloadId << " completed");

    m_workloads.erase(it);
    m_clusterState.SetActiveWorkloadCount(static_cast<uint32_t>(m_workloads.size()));

    m_workloadCompletedTrace(workloadId);
    m_workloadsCompleted++;
}

bool
Orchestrator::SendWorkloadResponse(const Address& clientAddr, uint64_t dagId, Ptr<DagTask> dag)
{
    NS_LOG_FUNCTION(this << clientAddr << dagId);

    // The whole executed DAG travels back in one message, so the client sees
    // exactly one completion event per workload no matter how many sink tasks
    // the DAG has, and gets every task's recorded backend/compute time.
    Ptr<Packet> dagPayload = dag->SerializeFullData(true);

    OrchestratorHeader header;
    header.SetMessageType(OrchestratorHeader::WORKLOAD_RESPONSE);
    header.SetDagId(dagId);
    header.SetPayloadSize(dagPayload->GetSize());

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(header);
    packet->AddAtEnd(dagPayload);

    NS_LOG_INFO("Sending workload response for dagId " << dagId << " (" << dag->GetTaskCount()
                                                       << " tasks) to client " << clientAddr);

    if (!m_clientConnMgr->Send(packet, clientAddr))
    {
        NS_LOG_WARN("Failed to send workload response to client " << clientAddr);
        return false;
    }

    return true;
}

void
Orchestrator::HandleWorkloadRequest(uint64_t dagId, Ptr<Packet> payload, const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << dagId << clientAddr);

    uint64_t consumedBytes = 0;
    Ptr<DagTask> dag =
        DagTask::DeserializeFullData(payload,
                                     MakeCallback(&Orchestrator::DispatchDeserialize, this),
                                     consumedBytes);

    if (!dag)
    {
        NS_LOG_WARN("Failed to deserialize workload request for dagId " << dagId << " from "
                                                                        << clientAddr);
        RejectWorkload(0, "deserialization_failed");
        SendAdmissionResponse(clientAddr, dagId, false);
        return;
    }

    if (dag->GetTaskCount() == 0)
    {
        NS_LOG_WARN("Workload request for empty DAG " << dagId);
        RejectWorkload(0, "empty_dag");
        SendAdmissionResponse(clientAddr, dagId, false);
        return;
    }

    if (!dag->Validate())
    {
        NS_LOG_WARN("DAG validation failed for dagId " << dagId);
        RejectWorkload(dag->GetTaskCount(), "invalid_dag");
        SendAdmissionResponse(clientAddr, dagId, false);
        return;
    }

    if (m_sessionAdmittedClients.count(clientAddr))
    {
        // Session already established: implicitly admitted, no check, no confirmation.
        CreateAndDispatchWorkload(dag, clientAddr, dagId);
        return;
    }

    if (!CheckAdmission(dag))
    {
        NS_LOG_INFO("Workload " << dagId << " rejected by admission policy");
        RejectWorkload(dag->GetTaskCount(), "admission_rejected");
        SendAdmissionResponse(clientAddr, dagId, false);
        return;
    }

    if (dag->GetDeclaredRate() > 0.0)
    {
        // A positive declared rate is SessionMode's signal to establish a
        // session: remember this client so future requests skip admission
        // entirely. WorkloadMode always declares 0, so it never does this.
        m_sessionAdmittedClients.insert(clientAddr);
        NS_LOG_INFO("Client " << clientAddr << " session admitted (declared rate="
                              << dag->GetDeclaredRate() << " Hz)");
    }

    CreateAndDispatchWorkload(dag, clientAddr, dagId); // proceeds immediately
    SendAdmissionResponse(clientAddr, dagId, true);    // non-blocking acknowledgment
}

void
Orchestrator::CleanupClient(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);

    m_rxBuffer.erase(clientAddr);
    m_sessionAdmittedClients.erase(clientAddr);

    std::vector<uint64_t> clientWorkloads;
    for (const auto& pair : m_workloads)
    {
        if (pair.second.clientAddr == clientAddr)
        {
            clientWorkloads.push_back(pair.first);
        }
    }

    for (uint64_t wid : clientWorkloads)
    {
        NS_LOG_DEBUG("Cancelling workload " << wid << " - client disconnected");
        CancelWorkload(wid);
    }
}

} // namespace ns3
