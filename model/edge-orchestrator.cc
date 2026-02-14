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
#include "ns3/nstime.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
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
            .AddAttribute("WorkerConnectionManager",
                          "Connection manager for worker transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&EdgeOrchestrator::m_workerConnMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddAttribute("AdmissionTimeout",
                          "Timeout for pending admissions (0 = no timeout)",
                          TimeValue(Seconds(0)),
                          MakeTimeAccessor(&EdgeOrchestrator::m_admissionTimeout),
                          MakeTimeChecker())
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
      m_workerConnMgr(nullptr)
{
    NS_LOG_FUNCTION(this);
}

EdgeOrchestrator::~EdgeOrchestrator()
{
    NS_LOG_FUNCTION(this);
}

uint64_t
EdgeOrchestrator::EncodeWireTaskId(uint32_t workloadId, uint32_t dagIdx)
{
    return (static_cast<uint64_t>(workloadId) << 32) | static_cast<uint64_t>(dagIdx);
}

std::pair<uint64_t, uint32_t>
EdgeOrchestrator::DecodeWireTaskId(uint64_t wireId)
{
    return {wireId >> 32, static_cast<uint32_t>(wireId & 0xFFFFFFFF)};
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

    // Move state out before erasing (same pattern as CompleteWorkload)
    WorkloadState state = std::move(it->second);
    m_workloads.erase(it);

    // Clean up wire task type entries and cluster state for dispatched tasks
    for (const auto& tb : state.taskToBackend)
    {
        m_clusterState.NotifyTaskCompleted(tb.second);
        int32_t dagIdx = state.dag->GetTaskIndex(tb.first);
        if (dagIdx >= 0)
        {
            m_wireTaskType.erase(EncodeWireTaskId(workloadId, static_cast<uint32_t>(dagIdx)));
        }
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

    // Clear callbacks first to prevent use-after-dispose
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
    for (auto& queuePair : m_pendingAdmissionQueue)
    {
        for (auto& entry : queuePair.second)
        {
            Simulator::Cancel(entry.timeoutEvent);
        }
    }
    m_pendingAdmissionQueue.clear();
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

    // Idempotent cleanup (safe if StopApplication already ran)
    CancelAllPendingAdmissions();
    CleanupConnectionManager(m_clientConnMgr);
    CleanupConnectionManager(m_workerConnMgr);
    m_rxBuffer.clear();
    m_workerRxBuffer.clear();

    // Release workload state and references
    m_workloads.clear();
    m_admissionPolicy = nullptr;
    m_scheduler = nullptr;
    m_deviceManager = nullptr;
    m_taskTypeRegistry.clear();
    m_wireTaskType.clear();
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

    // Peek the type byte
    uint8_t taskType;
    packet->CopyData(&taskType, 1);

    auto regIt = m_taskTypeRegistry.find(taskType);
    if (regIt == m_taskTypeRegistry.end())
    {
        NS_LOG_ERROR("No deserializer registered for task type " << (int)taskType);
        return nullptr;
    }

    // Create sub-packet without the type byte
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

    // Verify scheduler is set - fail fast if missing
    NS_ABORT_MSG_IF(!m_scheduler, "No Scheduler configured for EdgeOrchestrator");

    // Create default connection managers if not set
    if (!m_clientConnMgr)
    {
        m_clientConnMgr = CreateObject<TcpConnectionManager>();
    }
    if (!m_workerConnMgr)
    {
        m_workerConnMgr = CreateObject<TcpConnectionManager>();
    }

    // Configure client connection manager (listening)
    m_clientConnMgr->SetNode(GetNode());
    m_clientConnMgr->SetReceiveCallback(MakeCallback(&EdgeOrchestrator::HandleReceive, this));

    Ptr<TcpConnectionManager> tcpClientMgr = DynamicCast<TcpConnectionManager>(m_clientConnMgr);
    if (tcpClientMgr)
    {
        tcpClientMgr->SetCloseCallback(MakeCallback(&EdgeOrchestrator::HandleClientClose, this));
    }

    m_clientConnMgr->Bind(m_port);
    NS_LOG_INFO("EdgeOrchestrator listening on port " << m_port);

    // Configure worker connection manager (for outgoing connections)
    m_workerConnMgr->SetNode(GetNode());
    m_workerConnMgr->SetReceiveCallback(
        MakeCallback(&EdgeOrchestrator::HandleBackendResponse, this));

    Ptr<TcpConnectionManager> tcpWorkerMgr = DynamicCast<TcpConnectionManager>(m_workerConnMgr);
    if (tcpWorkerMgr)
    {
        tcpWorkerMgr->SetCloseCallback(MakeCallback(&EdgeOrchestrator::HandleBackendClose, this));
    }

    // Register default SimpleTask type if no types registered
    if (m_taskTypeRegistry.empty())
    {
        RegisterTaskType(SimpleTask::TASK_TYPE,
                         MakeCallback(&SimpleTask::Deserialize),
                         MakeCallback(&SimpleTask::DeserializeHeader));
        NS_LOG_DEBUG("Using default SimpleTask deserializer");
    }

    // Initialize cluster state
    m_clusterState.Resize(m_cluster.GetN());

    // Connect to all cluster backends at startup
    for (uint32_t i = 0; i < m_cluster.GetN(); i++)
    {
        m_workerConnMgr->Connect(m_cluster.Get(i).address);
    }

    // Start device manager if configured
    if (m_deviceManager)
    {
        m_deviceManager->Start(m_cluster, m_workerConnMgr);
    }
}

void
EdgeOrchestrator::StopApplication()
{
    NS_LOG_FUNCTION(this);

    CancelAllPendingAdmissions();

    // Cancel all active workloads before tearing down connections
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

    // Stop active network operations
    CleanupConnectionManager(m_clientConnMgr);
    CleanupConnectionManager(m_workerConnMgr);
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

    // Clear worker receive buffer
    m_workerRxBuffer.erase(backendAddr);

    // Find backend index for the disconnected address
    int32_t backendIdx = m_cluster.GetBackendIndex(backendAddr);

    if (backendIdx < 0)
    {
        NS_LOG_DEBUG("Disconnected backend not found in cluster");
        return;
    }

    // Find and cancel workloads with tasks dispatched to this backend
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

    while (buffer->GetSize() > 0)
    {
        // Peek at first byte to distinguish Phase 1 (OrchestratorHeader, types >= 2)
        // from Phase 2 (DAG data starting with 4-byte task count in network order;
        // first byte is (taskCount >> 24), guaranteed < 2 by DagTask serialization).
        uint8_t firstByte;
        buffer->CopyData(&firstByte, 1);

        if (firstByte >= OrchestratorHeader::ADMISSION_REQUEST)
        {
            // Phase 1: OrchestratorHeader-framed admission message
            if (buffer->GetSize() < orchHeaderSize)
            {
                NS_LOG_DEBUG("Buffer has " << buffer->GetSize() << " bytes, need " << orchHeaderSize
                                           << " for OrchestratorHeader");
                break;
            }

            OrchestratorHeader orchHeader;
            buffer->PeekHeader(orchHeader);

            uint64_t payloadSize = orchHeader.GetPayloadSize();
            uint64_t totalMessageSize = orchHeaderSize + payloadSize;
            if (buffer->GetSize() < totalMessageSize)
            {
                NS_LOG_DEBUG("Buffer has " << buffer->GetSize() << " bytes, need "
                                           << totalMessageSize << " for full message");
                break;
            }

            // Remove OrchestratorHeader from buffer
            buffer->RemoveAtStart(orchHeaderSize);

            // Extract payload
            Ptr<Packet> payload = buffer->CreateFragment(0, payloadSize);
            buffer->RemoveAtStart(payloadSize);

            uint8_t msgType = orchHeader.GetMessageType();
            if (msgType == OrchestratorHeader::ADMISSION_REQUEST)
            {
                HandleAdmissionRequest(orchHeader.GetTaskId(), payload, clientAddr);
            }
            else
            {
                NS_LOG_WARN("Unexpected message type " << (int)msgType << " from client "
                                                       << clientAddr << " - skipping");
            }
        }
        else
        {
            // Phase 2: Raw DAG data upload
            auto queueIt = m_pendingAdmissionQueue.find(clientAddr);
            if (queueIt == m_pendingAdmissionQueue.end() || queueIt->second.empty())
            {
                NS_LOG_ERROR("Received Phase 2 data from client "
                             << clientAddr << " but no pending admissions - clearing buffer");
                m_rxBuffer.erase(it);
                return;
            }

            const PendingAdmission& front = queueIt->second.front();
            uint64_t expectedId = front.id;

            uint64_t consumedBytes = 0;
            Ptr<DagTask> dag = DagTask::DeserializeFullData(
                buffer,
                MakeCallback(&EdgeOrchestrator::DispatchDeserialize, this),
                consumedBytes);

            if (consumedBytes == 0)
            {
                break; // Not enough data yet
            }

            buffer->RemoveAtStart(consumedBytes);
            ConsumePendingAdmission(clientAddr, expectedId);

            if (dag)
            {
                CreateAndDispatchWorkload(dag, clientAddr);
            }
            else
            {
                NS_LOG_WARN("Failed to deserialize data from client " << clientAddr);
                RejectWorkload(0, "deserialization_failed");
            }
        }
    }

    // Update or remove buffer
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

    // Check for duplicate admission from same client
    auto& queue = m_pendingAdmissionQueue[clientAddr];
    for (const auto& entry : queue)
    {
        if (entry.id == id)
        {
            NS_LOG_WARN("Duplicate admission request for id " << id << " from " << clientAddr);
            RejectWorkload(dag->GetTaskCount(), "duplicate_admission");
            SendAdmissionResponse(clientAddr, id, false);
            return false;
        }
    }

    // Store pending admission for Phase 2
    queue.push_back({id, EventId()});

    // Schedule timeout if configured
    if (m_admissionTimeout > Seconds(0))
    {
        queue.back().timeoutEvent = Simulator::Schedule(m_admissionTimeout,
                                                        &EdgeOrchestrator::HandleAdmissionTimeout,
                                                        this,
                                                        clientAddr,
                                                        id);
    }

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
        NS_LOG_WARN("Failed to send admission response to client " << clientAddr);
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

    if (!ProcessDagReadyTasks(workloadId))
    {
        // Dispatch failed — workload already cancelled by ProcessDagReadyTasks.
        // CancelWorkload already updated stats and invoked callbacks.
        return 0;
    }

    m_workloadsAdmitted++;
    m_workloadAdmittedTrace(workloadId, dag->GetTaskCount());

    // Evaluate DVFS scaling when new workload is dispatched
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

    // Schedule the task
    int32_t backendIdx = m_scheduler->ScheduleTask(task, m_cluster, m_clusterState);
    if (backendIdx < 0 || static_cast<uint32_t>(backendIdx) >= m_cluster.GetN())
    {
        NS_LOG_WARN("Scheduler returned invalid backend index " << backendIdx << " for task "
                                                                << task->GetTaskId());
        return -1;
    }

    const Cluster::Backend& backend = m_cluster.Get(backendIdx);

    // Look up workload state to get DAG index for wire encoding
    auto wit = m_workloads.find(workloadId);
    NS_ASSERT_MSG(wit != m_workloads.end(),
                  "DispatchTask: workload " << workloadId << " not found");
    auto& state = wit->second;

    uint64_t originalTaskId = task->GetTaskId();
    int32_t dagIdx = state.dag->GetTaskIndex(originalTaskId);
    NS_ASSERT_MSG(dagIdx >= 0, "Task " << originalTaskId << " not found in DAG");
    uint64_t wireId = EncodeWireTaskId(workloadId, static_cast<uint32_t>(dagIdx));

    // Record task type for backend response deserialization
    m_wireTaskType[wireId] = task->GetTaskType();

    // Track dispatch
    state.taskToBackend[originalTaskId] = backendIdx;
    state.pendingTasks++;

    // Set wire ID for backend routing, serialize, then restore original
    task->SetTaskId(wireId);
    Ptr<Packet> packet = task->Serialize(false);
    task->SetTaskId(originalTaskId);

    bool sent = m_workerConnMgr->Send(packet, backend.address);
    if (!sent)
    {
        NS_LOG_ERROR("Failed to send task to backend " << backendIdx);
        m_wireTaskType.erase(wireId);
        state.taskToBackend.erase(originalTaskId);
        state.pendingTasks--;
        return -1;
    }

    m_taskDispatchedTrace(workloadId, originalTaskId, backendIdx);
    m_clusterState.NotifyTaskDispatched(backendIdx);
    NS_LOG_INFO("Dispatched task " << originalTaskId << " (wire " << wireId << ") to backend "
                                   << backendIdx);

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

    auto bufferIt = AppendToBuffer(m_workerRxBuffer, from, packet);
    Ptr<Packet> buffer = bufferIt->second;

    while (buffer->GetSize() > 0)
    {
        // Need at least messageType(1) + taskId(8) to peek wireId
        if (buffer->GetSize() < 9)
        {
            break;
        }

        // Route device metrics to DeviceManager if present
        if (m_deviceManager && m_deviceManager->TryConsumeMetrics(buffer, from, m_clusterState))
        {
            continue;
        }

        // Peek wireId from the common TaskHeader prefix (byte 0: messageType,
        // bytes 1-8: taskId in network order). All TaskHeader implementations
        // MUST serialize this 9-byte prefix — see TaskHeader class documentation.
        uint8_t prefix[9];
        buffer->CopyData(prefix, 9);
        uint64_t wireId = 0;
        for (int j = 1; j < 9; j++)
        {
            wireId = (wireId << 8) | prefix[j];
        }

        // Look up task type from dispatch-time recording
        auto typeIt = m_wireTaskType.find(wireId);
        if (typeIt == m_wireTaskType.end())
        {
            NS_LOG_ERROR("No task type recorded for wireId " << wireId);
            break;
        }

        auto regIt = m_taskTypeRegistry.find(typeIt->second);
        if (regIt == m_taskTypeRegistry.end())
        {
            NS_LOG_ERROR("No deserializer for task type " << (int)typeIt->second);
            break;
        }

        uint64_t consumedBytes = 0;
        Ptr<Task> task = regIt->second.fullDeserializer(buffer, consumedBytes);

        if (consumedBytes == 0)
        {
            break;
        }

        buffer->RemoveAtStart(consumedBytes);
        m_wireTaskType.erase(wireId);

        if (!task)
        {
            NS_LOG_ERROR("Deserializer consumed " << consumedBytes
                                                  << " bytes but returned null task"
                                                  << " - possible data corruption");
            continue;
        }

        // Decode wire task ID to recover workload and DAG index
        std::pair<uint64_t, uint32_t> decoded = DecodeWireTaskId(wireId);
        uint64_t workloadId = decoded.first;
        uint32_t dagIdx = decoded.second;

        auto workloadIt = m_workloads.find(workloadId);
        if (workloadIt == m_workloads.end())
        {
            NS_LOG_WARN("Workload " << workloadId << " not found for wire task " << wireId);
            continue;
        }

        auto& state = workloadIt->second;

        // Restore original task ID from the DAG
        Ptr<Task> originalDagTask = state.dag->GetTask(dagIdx);
        if (!originalDagTask)
        {
            NS_LOG_WARN("DAG index " << dagIdx << " invalid for workload " << workloadId);
            continue;
        }
        uint64_t originalTaskId = originalDagTask->GetTaskId();
        task->SetTaskId(originalTaskId);

        // Get backend index
        auto backendIt = state.taskToBackend.find(originalTaskId);
        if (backendIt == state.taskToBackend.end())
        {
            NS_LOG_ERROR("Backend index not found for task " << originalTaskId << " in workload "
                                                             << workloadId
                                                             << " - skipping completion");
            continue;
        }
        uint32_t backendIdx = backendIt->second;

        OnTaskCompleted(workloadId, task, backendIdx);
    }

    if (buffer->GetSize() == 0)
    {
        m_workerRxBuffer.erase(from);
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

    // Notify scheduler and update cluster state
    m_scheduler->NotifyTaskCompleted(backendIdx, task);
    m_clusterState.NotifyTaskCompleted(backendIdx);

    // Evaluate DVFS scaling when a task completes
    if (m_deviceManager)
    {
        m_deviceManager->EvaluateScaling(m_clusterState);
    }

    // Fire trace
    m_taskCompletedTrace(workloadId, taskId, backendIdx);

    // Clean up dispatch tracking
    state.taskToBackend.erase(taskId);

    // Decrement pending count
    NS_ASSERT_MSG(state.pendingTasks > 0, "pendingTasks underflow for workload " << workloadId);
    state.pendingTasks--;

    NS_LOG_INFO("Task " << taskId << " completed on backend " << backendIdx);

    // Copy DAG pointer before calls that may invalidate the state reference
    Ptr<DagTask> dag = state.dag;

    int32_t dagIdx = dag->GetTaskIndex(taskId);
    if (dagIdx < 0)
    {
        NS_LOG_ERROR("Task " << taskId << " not found in DAG for workload " << workloadId);
        return;
    }

    // Update DAG node with response task data (for output size propagation)
    dag->SetTask(static_cast<uint32_t>(dagIdx), task);
    dag->MarkCompleted(static_cast<uint32_t>(dagIdx));

    // Note: CompleteWorkload/ProcessDagReadyTasks may erase the workload,
    // invalidating 'state'. Only 'dag' (local Ptr) is used after this point.
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

    // Get ready tasks from DAG
    std::vector<uint32_t> readyIndices = state.dag->GetReadyTasks();

    for (uint32_t idx : readyIndices)
    {
        Ptr<Task> task = state.dag->GetTask(idx);

        // Skip if already dispatched
        if (state.taskToBackend.find(task->GetTaskId()) != state.taskToBackend.end())
        {
            continue;
        }

        // Dispatch the task
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

    NS_LOG_INFO("Workload " << workloadId << " completed");

    WorkloadState state = std::move(it->second);
    m_workloads.erase(it);
    m_clusterState.SetActiveWorkloadCount(static_cast<uint32_t>(m_workloads.size()));

    // Update counters
    m_workloadCompletedTrace(workloadId);
    m_workloadsCompleted++;

    // Send response to client
    if (!state.clientAddr.IsInvalid())
    {
        SendWorkloadResponse(state.clientAddr, state.dag);
    }
}

void
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
            }
        }
    }
}

void
EdgeOrchestrator::ConsumePendingAdmission(const Address& clientAddr, uint64_t id)
{
    NS_LOG_FUNCTION(this << clientAddr << id);

    auto queueIt = m_pendingAdmissionQueue.find(clientAddr);
    if (queueIt != m_pendingAdmissionQueue.end() && !queueIt->second.empty())
    {
        NS_ASSERT_MSG(queueIt->second.front().id == id,
                      "ConsumePendingAdmission: front id " << queueIt->second.front().id
                                                           << " != expected " << id);
        Simulator::Cancel(queueIt->second.front().timeoutEvent);
        queueIt->second.pop_front();
        if (queueIt->second.empty())
        {
            m_pendingAdmissionQueue.erase(queueIt);
        }
    }
}

void
EdgeOrchestrator::CleanupClient(const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << clientAddr);

    // Clear receive buffer
    m_rxBuffer.erase(clientAddr);

    // Clear pending admissions and their timeout events for this client
    auto queueIt = m_pendingAdmissionQueue.find(clientAddr);
    if (queueIt != m_pendingAdmissionQueue.end())
    {
        for (const auto& entry : queueIt->second)
        {
            NS_LOG_DEBUG("Removing pending admission " << entry.id << " - client disconnected");
            Simulator::Cancel(entry.timeoutEvent);
        }
        m_pendingAdmissionQueue.erase(queueIt);
    }

    // Cancel pending workloads from this client
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

    // Deserialize DAG metadata (task headers only, no payloads)
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

    // Validate DAG structure
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

void
EdgeOrchestrator::HandleAdmissionTimeout(Address clientAddr, uint64_t id)
{
    NS_LOG_FUNCTION(this << clientAddr << id);

    auto queueIt = m_pendingAdmissionQueue.find(clientAddr);
    if (queueIt == m_pendingAdmissionQueue.end() || queueIt->second.empty())
    {
        return;
    }

    auto& queue = queueIt->second;
    NS_ASSERT_MSG(queue.front().id == id, "Admission timeout for non-front id " << id);

    NS_LOG_WARN("Admission timeout for id " << id << " from client " << clientAddr);

    // Cancel all pending admissions to preserve TCP stream ordering —
    // data for later admissions cannot be parsed once a preceding one is removed.
    for (auto& entry : queue)
    {
        Simulator::Cancel(entry.timeoutEvent);
        RejectWorkload(0, "admission_timeout");
    }
    queue.clear();
    m_pendingAdmissionQueue.erase(queueIt);
}

} // namespace ns3
