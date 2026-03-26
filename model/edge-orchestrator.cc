/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "edge-orchestrator.h"

#include "device-manager.h"
#include "simple-task.h"
#include "tcp-connection-manager.h"

#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("EdgeOrchestrator");

NS_OBJECT_ENSURE_REGISTERED(EdgeOrchestrator);

TypeId
EdgeOrchestrator::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::EdgeOrchestrator")
            .SetParent<Application>()
            .SetGroupName("Distributed")
            .AddConstructor<EdgeOrchestrator>()
            .AddAttribute("Port",
                          "Port on which to listen for incoming connections",
                          UintegerValue(8080),
                          MakeUintegerAccessor(&EdgeOrchestrator::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Scheduler",
                          "Task scheduler for backend selection",
                          PointerValue(),
                          MakePointerAccessor(&EdgeOrchestrator::m_scheduler),
                          MakePointerChecker<ClusterScheduler>())
            .AddAttribute("AdmissionPolicy",
                          "Admission policy for workload acceptance (nullptr = always admit)",
                          PointerValue(),
                          MakePointerAccessor(&EdgeOrchestrator::m_admissionPolicy),
                          MakePointerChecker<AdmissionPolicy>())
            .AddAttribute("ClientConnectionManager",
                          "Connection manager for client transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&EdgeOrchestrator::m_clientConnMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddAttribute("BackendConnectionManager",
                          "Connection manager for backend transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&EdgeOrchestrator::m_backendConnMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddAttribute("DeviceManager",
                          "DVFS device manager for backend scaling (optional)",
                          PointerValue(),
                          MakePointerAccessor(&EdgeOrchestrator::m_deviceManager),
                          MakePointerChecker<DeviceManager>())
            .AddTraceSource("WorkloadAdmitted",
                            "A workload has been admitted for execution",
                            MakeTraceSourceAccessor(&EdgeOrchestrator::m_workloadAdmittedTrace),
                            "ns3::EdgeOrchestrator::WorkloadAdmittedTracedCallback")
            .AddTraceSource("WorkloadRejected",
                            "A workload has been rejected",
                            MakeTraceSourceAccessor(&EdgeOrchestrator::m_workloadRejectedTrace),
                            "ns3::EdgeOrchestrator::WorkloadRejectedTracedCallback")
            .AddTraceSource("WorkloadCancelled",
                            "A workload has been cancelled",
                            MakeTraceSourceAccessor(&EdgeOrchestrator::m_workloadCancelledTrace),
                            "ns3::EdgeOrchestrator::WorkloadCancelledTracedCallback")
            .AddTraceSource("TaskDispatched",
                            "A task has been dispatched to a backend",
                            MakeTraceSourceAccessor(&EdgeOrchestrator::m_taskDispatchedTrace),
                            "ns3::EdgeOrchestrator::TaskDispatchedTracedCallback")
            .AddTraceSource("TaskCompleted",
                            "A task has been completed by a backend",
                            MakeTraceSourceAccessor(&EdgeOrchestrator::m_taskCompletedTrace),
                            "ns3::EdgeOrchestrator::TaskCompletedTracedCallback")
            .AddTraceSource("WorkloadCompleted",
                            "A workload has been fully completed",
                            MakeTraceSourceAccessor(&EdgeOrchestrator::m_workloadCompletedTrace),
                            "ns3::EdgeOrchestrator::WorkloadCompletedTracedCallback");
    return tid;
}

EdgeOrchestrator::EdgeOrchestrator()
    : m_admissionPolicy(nullptr),
      m_scheduler(nullptr),
      m_deviceManager(nullptr),
      m_port(8080),
      m_clientConnMgr(nullptr),
      m_backendConnMgr(nullptr)
{
    NS_LOG_FUNCTION(this);
}

EdgeOrchestrator::~EdgeOrchestrator()
{
    NS_LOG_FUNCTION(this);
}

uint64_t
EdgeOrchestrator::PeekTaskId(Ptr<Packet> buffer)
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
EdgeOrchestrator::CancelWorkload(uint64_t workloadId)
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

    for (const auto& tb : state.taskToBackend)
    {
        m_clusterState.NotifyTaskCompleted(tb.second);
    }

    m_workloadsCancelled++;
    m_workloadCancelledTrace(workloadId);
    m_clusterState.SetActiveWorkloadCount(static_cast<uint32_t>(m_workloads.size()));

    return true;
}

void
EdgeOrchestrator::RejectWorkload(uint32_t taskCount, const std::string& reason)
{
    NS_LOG_FUNCTION(this << taskCount << reason);
    m_workloadsRejected++;
    m_workloadRejectedTrace(taskCount, reason);
}

void
EdgeOrchestrator::CleanupConnectionManager(Ptr<ConnectionManager>& connMgr)
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

void
EdgeOrchestrator::CancelAllPendingAdmissions()
{
    NS_LOG_FUNCTION(this);
    m_pendingAdmissions.clear();
}

std::map<Address, Ptr<Packet>>::iterator
EdgeOrchestrator::AppendToBuffer(std::map<Address, Ptr<Packet>>& bufferMap,
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
EdgeOrchestrator::DoDispose()
{
    NS_LOG_FUNCTION(this);

    CancelAllPendingAdmissions();
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
EdgeOrchestrator::SetCluster(const Cluster& cluster)
{
    NS_LOG_FUNCTION(this);
    m_cluster = cluster;
}

const Cluster&
EdgeOrchestrator::GetCluster() const
{
    return m_cluster;
}

void
EdgeOrchestrator::RegisterTaskType(uint8_t taskType,
                                   DeserializerCallback fullDeserializer,
                                   DeserializerCallback metadataDeserializer)
{
    NS_LOG_FUNCTION(this << (int)taskType);
    m_taskTypeRegistry[taskType] = {fullDeserializer, metadataDeserializer};
}

Ptr<Task>
EdgeOrchestrator::DispatchDeserializeImpl(Ptr<Packet> packet,
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
EdgeOrchestrator::DispatchDeserialize(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    NS_LOG_FUNCTION(this << packet);
    return DispatchDeserializeImpl(packet, consumedBytes, false);
}

Ptr<Task>
EdgeOrchestrator::DispatchDeserializeMetadata(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    NS_LOG_FUNCTION(this << packet);
    return DispatchDeserializeImpl(packet, consumedBytes, true);
}

uint64_t
EdgeOrchestrator::GetWorkloadsAdmitted() const
{
    return m_workloadsAdmitted;
}

uint64_t
EdgeOrchestrator::GetWorkloadsRejected() const
{
    return m_workloadsRejected;
}

uint64_t
EdgeOrchestrator::GetWorkloadsCompleted() const
{
    return m_workloadsCompleted;
}

uint32_t
EdgeOrchestrator::GetActiveWorkloadCount() const
{
    return static_cast<uint32_t>(m_workloads.size());
}

uint64_t
EdgeOrchestrator::GetWorkloadsCancelled() const
{
    return m_workloadsCancelled;
}

Ptr<ClusterScheduler>
EdgeOrchestrator::GetScheduler() const
{
    return m_scheduler;
}

Ptr<AdmissionPolicy>
EdgeOrchestrator::GetAdmissionPolicy() const
{
    return m_admissionPolicy;
}

void
EdgeOrchestrator::StartApplication()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(!m_scheduler, "No Scheduler configured for EdgeOrchestrator");

    if (!m_clientConnMgr)
    {
        m_clientConnMgr = CreateObject<TcpConnectionManager>();
    }
    if (!m_backendConnMgr)
    {
        m_backendConnMgr = CreateObject<TcpConnectionManager>();
    }

    m_clientConnMgr->SetNode(GetNode());
    m_clientConnMgr->SetReceiveCallback(MakeCallback(&EdgeOrchestrator::HandleReceive, this));

    Ptr<TcpConnectionManager> tcpClientMgr = DynamicCast<TcpConnectionManager>(m_clientConnMgr);
    if (tcpClientMgr)
    {
        tcpClientMgr->SetCloseCallback(MakeCallback(&EdgeOrchestrator::HandleClientClose, this));
    }

    m_clientConnMgr->Bind(m_port);
    NS_LOG_INFO("EdgeOrchestrator listening on port " << m_port);

    m_backendConnMgr->SetNode(GetNode());
    m_backendConnMgr->SetReceiveCallback(
        MakeCallback(&EdgeOrchestrator::HandleBackendResponse, this));

    Ptr<TcpConnectionManager> tcpBackendMgr = DynamicCast<TcpConnectionManager>(m_backendConnMgr);
    if (tcpBackendMgr)
    {
        tcpBackendMgr->SetCloseCallback(MakeCallback(&EdgeOrchestrator::HandleBackendClose, this));
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
EdgeOrchestrator::StopApplication()
{
    NS_LOG_FUNCTION(this);

    CancelAllPendingAdmissions();

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
EdgeOrchestrator::HandleReceive(Ptr<Packet> packet, const Address& from)
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
EdgeOrchestrator::HandleClientClose(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);
    NS_LOG_INFO("Client disconnected: " << clientAddr);
    CleanupClient(clientAddr);
}

void
EdgeOrchestrator::HandleBackendClose(const Address& backendAddr)
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
EdgeOrchestrator::ProcessClientBuffer(const Address& clientAddr)
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
        case OrchestratorHeader::ADMISSION_REQUEST:
            HandleAdmissionRequest(orchHeader.GetTaskId(), payload, clientAddr);
            break;
        case OrchestratorHeader::DATA_UPLOAD:
            HandleDataUpload(orchHeader.GetTaskId(), payload, clientAddr);
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

bool
EdgeOrchestrator::ProcessAdmissionDecision(Ptr<DagTask> dag, uint64_t id, const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << id << clientAddr);

    if (!CheckAdmission(dag))
    {
        NS_LOG_INFO("Workload " << id << " rejected by admission policy");
        RejectWorkload(dag->GetTaskCount(), "admission_rejected");
        SendAdmissionResponse(clientAddr, id, false);
        return false;
    }

    auto& pending = m_pendingAdmissions[clientAddr];
    if (pending.count(id))
    {
        NS_LOG_WARN("Duplicate admission request for id " << id << " from " << clientAddr);
        RejectWorkload(dag->GetTaskCount(), "duplicate_admission");
        SendAdmissionResponse(clientAddr, id, false);
        return false;
    }

    pending.insert(id);

    NS_LOG_INFO("Workload " << id << " admitted, awaiting data upload");
    SendAdmissionResponse(clientAddr, id, true);
    return true;
}

void
EdgeOrchestrator::SendAdmissionResponse(const Address& clientAddr, uint64_t taskId, bool admitted)
{
    NS_LOG_FUNCTION(this << clientAddr << taskId << admitted);

    OrchestratorHeader response;
    response.SetMessageType(OrchestratorHeader::ADMISSION_RESPONSE);
    response.SetTaskId(taskId);
    response.SetAdmitted(admitted);
    response.SetPayloadSize(0);

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(response);

    if (!m_clientConnMgr->Send(packet, clientAddr))
    {
        NS_LOG_WARN("Failed to send admission response for id " << taskId << " to " << clientAddr);

        if (admitted)
        {
            auto mapIt = m_pendingAdmissions.find(clientAddr);
            if (mapIt != m_pendingAdmissions.end())
            {
                mapIt->second.erase(taskId);
                if (mapIt->second.empty())
                {
                    m_pendingAdmissions.erase(mapIt);
                }
            }
            RejectWorkload(0, "admission_response_send_failed");
        }
        return;
    }

    NS_LOG_DEBUG("Sent ADMISSION_RESPONSE for id " << taskId << ": "
                                                   << (admitted ? "admitted" : "rejected"));
}

bool
EdgeOrchestrator::CheckAdmission(Ptr<DagTask> dag)
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
EdgeOrchestrator::CreateAndDispatchWorkload(Ptr<DagTask> dag, const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << dag);

    uint64_t workloadId = m_nextWorkloadId++;
    WorkloadState state;
    state.dag = dag;
    state.clientAddr = clientAddr;
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

    if (m_deviceManager)
    {
        m_deviceManager->EvaluateScaling(m_clusterState);
    }

    NS_LOG_INFO("Workload " << workloadId << " admitted (" << dag->GetTaskCount() << " tasks)");

    return workloadId;
}

int32_t
EdgeOrchestrator::DispatchTask(uint64_t workloadId, Ptr<Task> task)
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

    m_dispatchedTasks[taskId] = {workloadId, static_cast<uint32_t>(dagIdx), task->GetTaskType()};

    state.taskToBackend[taskId] = backendIdx;
    state.pendingTasks++;

    Ptr<Packet> packet = task->Serialize(false);

    bool sent = m_backendConnMgr->Send(packet, backend.address);
    if (!sent)
    {
        NS_LOG_ERROR("Failed to send task to backend " << backendIdx);
        m_dispatchedTasks.erase(taskId);
        state.taskToBackend.erase(taskId);
        state.pendingTasks--;
        return -1;
    }

    task->SetState(TASK_DISPATCHED);
    m_taskDispatchedTrace(workloadId, taskId, backendIdx);
    m_clusterState.NotifyTaskDispatched(backendIdx);
    NS_LOG_INFO("Dispatched task " << taskId << " to backend " << backendIdx);

    return backendIdx;
}

void
EdgeOrchestrator::HandleBackendResponse(Ptr<Packet> packet, const Address& from)
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
            NS_LOG_ERROR("Deserializer consumed "
                         << consumedBytes << " bytes but returned null task " << taskId
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

        OnTaskCompleted(info.workloadId, task, backendIdx);
    }

    if (buffer->GetSize() == 0)
    {
        m_backendRxBuffer.erase(from);
    }
}

void
EdgeOrchestrator::OnTaskCompleted(uint64_t workloadId, Ptr<Task> task, uint32_t backendIdx)
{
    NS_LOG_FUNCTION(this << workloadId << task->GetTaskId() << backendIdx);

    auto it = m_workloads.find(workloadId);
    if (it == m_workloads.end())
    {
        NS_LOG_WARN("OnTaskCompleted: workload " << workloadId << " not found");
        return;
    }

    WorkloadState& state = it->second;
    uint64_t taskId = task->GetTaskId();

    m_scheduler->NotifyTaskCompleted(backendIdx, task);
    m_clusterState.NotifyTaskCompleted(backendIdx);

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
EdgeOrchestrator::ProcessDagReadyTasks(uint64_t workloadId)
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
EdgeOrchestrator::CompleteWorkload(uint64_t workloadId)
{
    NS_LOG_FUNCTION(this << workloadId);

    auto it = m_workloads.find(workloadId);
    if (it == m_workloads.end())
    {
        return;
    }
    if (!it->second.clientAddr.IsInvalid())
    {
        if (!SendWorkloadResponse(it->second.clientAddr, it->second.dag))
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
EdgeOrchestrator::SendWorkloadResponse(const Address& clientAddr, Ptr<DagTask> dag)
{
    NS_LOG_FUNCTION(this << clientAddr);

    std::vector<uint32_t> sinkIndices = dag->GetSinkTasks();

    NS_LOG_INFO("Sending " << sinkIndices.size() << " sink task response(s) to client "
                           << clientAddr);

    for (uint32_t idx : sinkIndices)
    {
        Ptr<Task> sinkTask = dag->GetTask(idx);
        if (sinkTask)
        {
            Ptr<Packet> packet = sinkTask->Serialize(true);
            if (!m_clientConnMgr->Send(packet, clientAddr))
            {
                NS_LOG_WARN("Failed to send response to client " << clientAddr);
                return false;
            }
        }
    }

    return true;
}

void
EdgeOrchestrator::HandleDataUpload(uint64_t dagId, Ptr<Packet> payload, const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << dagId << clientAddr);

    auto mapIt = m_pendingAdmissions.find(clientAddr);
    if (mapIt == m_pendingAdmissions.end() || mapIt->second.find(dagId) == mapIt->second.end())
    {
        NS_LOG_WARN("Received DATA_UPLOAD for unknown dagId " << dagId << " from " << clientAddr
                                                              << " — discarding");
        return;
    }

    mapIt->second.erase(dagId);
    if (mapIt->second.empty())
    {
        m_pendingAdmissions.erase(mapIt);
    }

    uint64_t consumedBytes = 0;
    Ptr<DagTask> dag =
        DagTask::DeserializeFullData(payload,
                                     MakeCallback(&EdgeOrchestrator::DispatchDeserialize, this),
                                     consumedBytes);

    if (dag)
    {
        CreateAndDispatchWorkload(dag, clientAddr);
    }
    else
    {
        NS_LOG_WARN("Failed to deserialize DAG data for dagId " << dagId << " from " << clientAddr);
        RejectWorkload(0, "deserialization_failed");
    }
}

void
EdgeOrchestrator::CleanupClient(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);

    m_rxBuffer.erase(clientAddr);

    m_pendingAdmissions.erase(clientAddr);

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

void
EdgeOrchestrator::HandleAdmissionRequest(uint64_t dagId,
                                         Ptr<Packet> dagPacket,
                                         const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << dagId << clientAddr);

    uint64_t consumedBytes = 0;
    Ptr<DagTask> dag = DagTask::DeserializeMetadata(
        dagPacket,
        MakeCallback(&EdgeOrchestrator::DispatchDeserializeMetadata, this),
        consumedBytes);
    if (!dag)
    {
        NS_LOG_WARN("Failed to deserialize DAG metadata for dagId " << dagId);
        RejectWorkload(0, "deserialization_failed");
        SendAdmissionResponse(clientAddr, dagId, false);
        return;
    }

    if (dag->GetTaskCount() == 0)
    {
        NS_LOG_WARN("DAG admission request for empty DAG " << dagId);
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

    ProcessAdmissionDecision(dag, dagId, clientAddr);
}

} // namespace ns3
