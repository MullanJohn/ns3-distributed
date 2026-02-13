/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef EDGE_ORCHESTRATOR_H
#define EDGE_ORCHESTRATOR_H

#include "admission-policy.h"
#include "cluster.h"
#include "cluster-scheduler.h"
#include "connection-manager.h"
#include "dag-task.h"
#include "device-metrics-header.h"
#include "orchestrator-header.h"

#include "ns3/application.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <deque>
#include <map>
#include <unordered_map>
#include <utility>

namespace ns3
{

class DeviceManager;

/**
 * @ingroup distributed
 * @brief Orchestrator for edge computing workloads with admission control and scheduling.
 *
 * EdgeOrchestrator manages the execution of computational tasks on a cluster
 * of backend workers. It provides:
 * - Admission control via pluggable AdmissionPolicy
 * - Task scheduling via pluggable Scheduler
 * - Support for DAG workflows (single tasks are wrapped as 1-node DAGs)
 *
 * The orchestrator supports mixed task types through a task type registry.
 * Each task type is registered via RegisterTaskType() with its deserializer
 * callbacks, enabling DAGs containing different task types (e.g., ImageTask
 * and LlmTask in the same workflow).
 *
 * Example usage:
 * @code
 * Ptr<EdgeOrchestrator> orchestrator = CreateObject<EdgeOrchestrator>();
 * orchestrator->SetCluster(cluster);
 * orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
 * orchestrator->SetAttribute("AdmissionPolicy", PointerValue(policy));
 * @endcode
 */
class EdgeOrchestrator : public Application
{
  public:
    /**
     * @brief Callback type for deserializing tasks from packet buffers.
     *
     * The deserializer handles both boundary detection and task creation.
     * It knows its own header format, so it can determine message boundaries.
     *
     * @param packet The packet buffer (may contain multiple messages or partial data).
     * @param consumedBytes Output: bytes consumed from packet (0 if not enough data).
     * @return The deserialized task, or nullptr if not enough data for complete message.
     */
    typedef Callback<Ptr<Task>, Ptr<Packet>, uint64_t&> DeserializerCallback;

    /**
     * @brief TracedCallback signature for workload admitted events.
     * @param workloadId The admitted workload ID.
     * @param taskCount Number of tasks in the workload.
     */
    typedef void (*WorkloadAdmittedTracedCallback)(uint64_t workloadId, uint32_t taskCount);

    /**
     * @brief TracedCallback signature for workload rejected events.
     * @param taskCount Number of tasks in the rejected workload.
     * @param reason Human-readable rejection reason.
     */
    typedef void (*WorkloadRejectedTracedCallback)(uint32_t taskCount, const std::string& reason);

    /**
     * @brief TracedCallback signature for task dispatched events.
     * @param workloadId The workload this task belongs to.
     * @param taskId The dispatched task ID.
     * @param backendIdx The backend index the task was dispatched to.
     */
    typedef void (*TaskDispatchedTracedCallback)(uint64_t workloadId,
                                                 uint64_t taskId,
                                                 uint32_t backendIdx);

    /**
     * @brief TracedCallback signature for task completed events.
     * @param workloadId The workload this task belongs to.
     * @param taskId The completed task ID.
     * @param backendIdx The backend index that processed the task.
     */
    typedef void (*TaskCompletedTracedCallback)(uint64_t workloadId,
                                                uint64_t taskId,
                                                uint32_t backendIdx);

    /**
     * @brief TracedCallback signature for workload cancelled events.
     * @param workloadId The cancelled workload ID.
     */
    typedef void (*WorkloadCancelledTracedCallback)(uint64_t workloadId);

    /**
     * @brief TracedCallback signature for workload completed events.
     * @param workloadId The completed workload ID.
     */
    typedef void (*WorkloadCompletedTracedCallback)(uint64_t workloadId);

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    EdgeOrchestrator();
    ~EdgeOrchestrator() override;

    /**
     * @brief Set the cluster of backend workers.
     * @param cluster The cluster configuration.
     */
    void SetCluster(const Cluster& cluster);

    /**
     * @brief Get the cluster.
     * @return The cluster configuration.
     */
    const Cluster& GetCluster() const;

    /**
     * @brief Entry in the task type registry.
     */
    struct TaskTypeEntry
    {
        DeserializerCallback fullDeserializer;     //!< Full task deserializer (header + payload)
        DeserializerCallback metadataDeserializer; //!< Header-only deserializer
    };

    /**
     * @brief Register deserializers for a task type.
     *
     * Associates a 1-byte task type identifier with full and metadata-only
     * deserializer callbacks. Must be called before StartApplication() for
     * custom task types. If no types are registered, SimpleTask is registered
     * automatically at startup.
     *
     * @param taskType The task type identifier (from Task::GetTaskType()).
     * @param fullDeserializer Callback to deserialize a complete task (header + payload).
     * @param metadataDeserializer Callback to deserialize task metadata only (header).
     */
    void RegisterTaskType(uint8_t taskType,
                          DeserializerCallback fullDeserializer,
                          DeserializerCallback metadataDeserializer);

    /**
     * @brief Get number of workloads admitted.
     * @return Count of admitted workloads.
     */
    uint64_t GetWorkloadsAdmitted() const;

    /**
     * @brief Get number of workloads rejected.
     * @return Count of rejected workloads.
     */
    uint64_t GetWorkloadsRejected() const;

    /**
     * @brief Get number of workloads completed.
     * @return Count of completed workloads.
     */
    uint64_t GetWorkloadsCompleted() const;

    /**
     * @brief Get number of active workloads.
     * @return Count of currently executing workloads.
     */
    uint32_t GetActiveWorkloadCount() const;

    /**
     * @brief Get number of workloads cancelled.
     * @return Count of cancelled workloads (e.g., due to client disconnect).
     */
    uint64_t GetWorkloadsCancelled() const;

    /**
     * @brief Get the configured scheduler.
     * @return The scheduler, or nullptr if not set.
     */
    Ptr<ClusterScheduler> GetScheduler() const;

    /**
     * @brief Get the configured admission policy.
     * @return The admission policy, or nullptr if not set (always admit).
     */
    Ptr<AdmissionPolicy> GetAdmissionPolicy() const;

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * @brief Handle data received from a client via ConnectionManager.
     * @param packet The received packet.
     * @param from The client address.
     */
    void HandleReceive(Ptr<Packet> packet, const Address& from);

    /**
     * @brief Handle client disconnection (TCP-specific).
     * @param clientAddr The address of the disconnected client.
     */
    void HandleClientClose(const Address& clientAddr);

    /**
     * @brief Handle backend disconnection (TCP-specific).
     * @param backendAddr The address of the disconnected backend.
     */
    void HandleBackendClose(const Address& backendAddr);

    /**
     * @brief Process buffered data for a client, extracting complete messages.
     * @param clientAddr The client address.
     */
    void ProcessClientBuffer(const Address& clientAddr);

    /**
     * @brief Handle an admission request from a client (Phase 1).
     *
     * Deserializes DAG metadata, validates structure, checks admission
     * policy, and sends ADMISSION_RESPONSE.
     *
     * @param dagId The DAG ID from the OrchestratorHeader taskId field.
     * @param dagPacket Packet containing serialized DAG metadata.
     * @param clientAddr The client address.
     */
    void HandleAdmissionRequest(uint64_t dagId,
                                 Ptr<Packet> dagPacket,
                                 const Address& clientAddr);

    /**
     * @brief Process an admission decision for a workload.
     *
     * Checks admission policy, detects duplicate requests, queues the
     * pending admission, schedules timeout, and sends the response.
     *
     * @param dag The workload DAG.
     * @param id The DAG ID.
     * @param clientAddr The client address.
     * @return true if admitted, false if rejected.
     */
    bool ProcessAdmissionDecision(Ptr<DagTask> dag,
                                   uint64_t id,
                                   const Address& clientAddr);

    /**
     * @brief Send admission response to client.
     * @param clientAddr The client address.
     * @param taskId The task ID.
     * @param admitted Whether the task was admitted.
     */
    void SendAdmissionResponse(const Address& clientAddr,
                                uint64_t taskId,
                                bool admitted);

    /**
     * @brief Check admission for a workload.
     * @param dag The workload DAG.
     * @return true if admitted.
     */
    bool CheckAdmission(Ptr<DagTask> dag);

    /**
     * @brief Create and dispatch a workload.
     * @param dag The DAG to execute.
     * @param clientAddr Client address.
     * @return Workload ID on success, 0 on failure.
     */
    uint64_t CreateAndDispatchWorkload(Ptr<DagTask> dag,
                                        const Address& clientAddr);

    /**
     * @brief Dispatch a task to a backend.
     * @param workloadId The workload this task belongs to.
     * @param task The task to dispatch.
     * @return Backend index, or -1 if scheduling failed.
     */
    int32_t DispatchTask(uint64_t workloadId, Ptr<Task> task);

    /**
     * @brief Handle response from a backend worker.
     * @param packet The response packet.
     * @param from The backend address.
     */
    void HandleBackendResponse(Ptr<Packet> packet, const Address& from);

    /**
     * @brief Handle task completion.
     * @param workloadId The workload this task belongs to.
     * @param task The completed task.
     * @param backendIdx The backend that processed the task.
     */
    void OnTaskCompleted(uint64_t workloadId,
                         Ptr<Task> task,
                         uint32_t backendIdx);

    /**
     * @brief Process ready tasks for a DAG workload.
     * @param workloadId The workload to process.
     * @return true if all ready tasks dispatched successfully, false if a dispatch
     *         failure occurred and the workload was cancelled.
     */
    bool ProcessDagReadyTasks(uint64_t workloadId);

    /**
     * @brief Complete a workload and invoke callback.
     */
    void CompleteWorkload(uint64_t workloadId);

    /**
     * @brief Send workload results to a network client.
     *
     * Sends the output from sink tasks (tasks with no successors) to the client.
     *
     * @param clientAddr The client address.
     * @param dag The completed DAG.
     */
    void SendWorkloadResponse(const Address& clientAddr, Ptr<DagTask> dag);

    /**
     * @brief Clean up state for a disconnected client.
     * @param clientAddr The client address.
     */
    void CleanupClient(const Address& clientAddr);

    /**
     * @brief Cancel timeout and remove the front pending admission for a client.
     * @param clientAddr The client address.
     * @param id The admission ID to clean up.
     */
    void ConsumePendingAdmission(const Address& clientAddr, uint64_t id);

    /**
     * @brief Cancel an active workload, cleaning up all associated state.
     *
     * Erases dispatch tracking, decrements active count, increments cancelled
     * count, fires the WorkloadCancelled trace, and removes the workload
     * from m_workloads.
     *
     * @param workloadId The workload to cancel.
     * @return true if the workload was found and cancelled.
     */
    bool CancelWorkload(uint64_t workloadId);

    /**
     * @brief Helper to increment rejected counter and fire trace.
     * @param taskCount Number of tasks in the rejected workload.
     * @param reason Rejection reason.
     */
    void RejectWorkload(uint32_t taskCount, const std::string& reason);

    /**
     * @brief Helper to clean up a connection manager.
     * @param connMgr The connection manager to clean up (will be set to nullptr).
     */
    void CleanupConnectionManager(Ptr<ConnectionManager>& connMgr);

    /**
     * @brief Cancel all pending admission timeout events and clear the queue.
     */
    void CancelAllPendingAdmissions();

    /**
     * @brief Append packet data to a per-address receive buffer.
     * @param bufferMap The buffer map (client or worker).
     * @param addr The source address.
     * @param packet The received packet.
     * @return Iterator to the buffer entry.
     */
    std::map<Address, Ptr<Packet>>::iterator AppendToBuffer(
        std::map<Address, Ptr<Packet>>& bufferMap,
        const Address& addr,
        Ptr<Packet> packet);

    /**
     * @brief Handle timeout of a pending admission.
     * @param clientAddr The client address.
     * @param id The admission ID that timed out.
     */
    void HandleAdmissionTimeout(Address clientAddr, uint64_t id);

    /**
     * @brief Encode workload ID and DAG index into a wire task ID.
     * @param workloadId The workload ID (upper 32 bits).
     * @param dagIdx The DAG task index (lower 32 bits).
     * @return The encoded wire task ID.
     */
    static uint64_t EncodeWireTaskId(uint32_t workloadId, uint32_t dagIdx);

    /**
     * @brief Decode a wire task ID into workload ID and DAG index.
     * @param wireId The encoded wire task ID.
     * @return Pair of (workloadId, dagIdx).
     */
    static std::pair<uint64_t, uint32_t> DecodeWireTaskId(uint64_t wireId);

    /**
     * @brief Dispatch deserialization of a type-prefixed task buffer.
     *
     * Peeks the 1-byte type tag, looks up the full deserializer in the
     * registry, strips the type byte, and delegates.
     *
     * @param packet The packet buffer (type byte + task data).
     * @param consumedBytes Output: bytes consumed (including type byte), 0 if insufficient data.
     * @return The deserialized task, or nullptr on failure.
     */
    Ptr<Task> DispatchDeserialize(Ptr<Packet> packet, uint64_t& consumedBytes);

    /**
     * @brief Dispatch metadata-only deserialization of a type-prefixed task buffer.
     *
     * Same as DispatchDeserialize but uses the metadata deserializer.
     *
     * @param packet The packet buffer (type byte + task header data).
     * @param consumedBytes Output: bytes consumed (including type byte), 0 if insufficient data.
     * @return The deserialized task, or nullptr on failure.
     */
    Ptr<Task> DispatchDeserializeMetadata(Ptr<Packet> packet, uint64_t& consumedBytes);

    /**
     * @brief Shared implementation for dispatch deserialization.
     *
     * @param packet The packet buffer (type byte + task data).
     * @param consumedBytes Output: bytes consumed (including type byte), 0 if insufficient data.
     * @param metadataOnly If true, use the metadata deserializer; otherwise use the full deserializer.
     * @return The deserialized task, or nullptr on failure.
     */
    Ptr<Task> DispatchDeserializeImpl(Ptr<Packet> packet, uint64_t& consumedBytes, bool metadataOnly);

    // Configuration
    Ptr<AdmissionPolicy> m_admissionPolicy; //!< Admission policy (nullptr = always admit)
    Ptr<ClusterScheduler> m_scheduler;      //!< Task scheduler (required)
    Ptr<DeviceManager> m_deviceManager;     //!< DVFS device manager (optional)
    std::map<uint8_t, TaskTypeEntry> m_taskTypeRegistry; //!< taskType → deserializers
    std::unordered_map<uint64_t, uint8_t> m_wireTaskType; //!< wireId → taskType (for backend responses)
    Cluster m_cluster;                      //!< Backend cluster
    uint16_t m_port;                        //!< Listen port

    // Connection management
    Ptr<ConnectionManager> m_clientConnMgr;            //!< For client connections (listening)
    Ptr<ConnectionManager> m_workerConnMgr;            //!< For worker connections (outgoing)
    std::map<Address, Ptr<Packet>> m_rxBuffer;         //!< Per-client receive buffers
    std::map<Address, Ptr<Packet>> m_workerRxBuffer;   //!< Per-worker receive buffers

    // Workload state
    struct WorkloadState
    {
        Ptr<DagTask> dag;                            //!< The DAG workflow
        Address clientAddr;                          //!< Client address for response routing
        std::map<uint64_t, uint32_t> taskToBackend;  //!< originalTaskId → backendIdx
        uint32_t pendingTasks{0};                    //!< Tasks dispatched but not completed
    };

    std::map<uint64_t, WorkloadState> m_workloads; //!< Active workloads
    uint32_t m_nextWorkloadId{1};                  //!< Next workload ID

    // Pending admissions — per-client ordered queue
    // TCP ordering guarantees Phase 2 data arrives in the same order as admissions,
    // so the queue front tells the orchestrator what format to expect.
    struct PendingAdmission
    {
        uint64_t id;           //!< DAG ID from Phase 1
        EventId timeoutEvent;  //!< Timeout event (default: not running)
    };

    std::map<Address, std::deque<PendingAdmission>> m_pendingAdmissionQueue;

    // Admission timeout tracking
    Time m_admissionTimeout;  //!< Timeout for pending admissions (0 = no timeout)

    // Statistics
    uint64_t m_workloadsAdmitted{0};  //!< Total admitted
    uint64_t m_workloadsRejected{0};  //!< Total rejected
    uint64_t m_workloadsCompleted{0}; //!< Total completed
    uint64_t m_workloadsCancelled{0}; //!< Total cancelled (client disconnect)

    // Traces
    TracedCallback<uint64_t, uint32_t> m_workloadAdmittedTrace;  //!< (workloadId, taskCount)
    TracedCallback<uint32_t, const std::string&> m_workloadRejectedTrace; //!< (taskCount, reason)
    TracedCallback<uint64_t> m_workloadCancelledTrace; //!< (workloadId)
    TracedCallback<uint64_t, uint64_t, uint32_t> m_taskDispatchedTrace; //!< (workloadId, taskId, backendIdx)
    TracedCallback<uint64_t, uint64_t, uint32_t> m_taskCompletedTrace;  //!< (workloadId, taskId, backendIdx)
    TracedCallback<uint64_t> m_workloadCompletedTrace; //!< (workloadId)
};

} // namespace ns3

#endif // EDGE_ORCHESTRATOR_H
