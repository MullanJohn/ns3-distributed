/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef OFFLOAD_CLIENT_H
#define OFFLOAD_CLIENT_H

#include "admission-retry-policy.h"
#include "client-submission-policy.h"
#include "connection-manager.h"
#include "dag-task.h"
#include "orchestrator-header.h"
#include "task.h"

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"

#include <map>

namespace ns3
{

class Packet;

/**
 * @ingroup continuum
 * @brief Configurable client that generates tasks for remote processing.
 *
 * OffloadClient models a device that generates tasks according to a
 * pluggable inter-arrival distribution and offloads them to a backend.
 * The default inter-arrival is constant at 1/30 s; pass any other
 * RandomVariableStream (e.g. ExponentialRandomVariable) for stochastic
 * arrivals.
 *
 * Each generation tick instantiates a fresh DagTask from the client's
 * assigned DagTemplate attribute (structure and per-task distributions;
 * see Task::Sample() and DagTask::Instantiate()).
 *
 * Example usage (helper convenience):
 * @code
 * OffloadClientHelper helper(orchAddress);
 * helper.SetTaskRate(30.0);
 * helper.SetLinearChainDagTemplate(1, inputSizeRv, computeDemandRv, outputSizeRv);
 * helper.Install(node);
 * @endcode
 */
class OffloadClient : public Application
{
  public:
    /**
     * @brief How workloads are admitted by the orchestrator.
     */
    enum AdmissionMode : uint8_t
    {
        SESSION_MODE = 0, //!< Default: admit once, subsequent workloads implicit
        WORKLOAD_MODE = 1 //!< Every workload individually admitted (combined request)
    };

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    OffloadClient();
    ~OffloadClient() override;

    /**
     * @brief Set the remote orchestrator address.
     * @param addr Orchestrator address (InetSocketAddress or Inet6SocketAddress).
     */
    void SetRemote(const Address& addr);

    /**
     * @brief Get the remote orchestrator address.
     * @return The orchestrator address.
     */
    Address GetRemote() const;

    /**
     * @brief Get the number of tasks sent.
     * @return Number of tasks submitted for admission.
     */
    uint64_t GetTasksSent() const;

    /**
     * @brief Get the number of tasks dropped.
     * @return Number of tasks dropped because a prior task was still pending.
     */
    uint64_t GetTasksDropped() const;

    /**
     * @brief Get the number of responses received.
     * @return Number of processed task results received.
     */
    uint64_t GetResponsesReceived() const;

    /**
     * @brief Get the total bytes transmitted.
     * @return Total bytes sent.
     */
    uint64_t GetTotalTx() const;

    /**
     * @brief Get the total bytes received.
     * @return Total bytes received.
     */
    uint64_t GetTotalRx() const;

    /**
     * @brief Assign a fixed random variable stream number to the random variables used by this
     * application.
     * @param stream First stream index to use.
     * @return The number of stream indices assigned.
     */
    int64_t AssignStreams(int64_t stream) override;

    /**
     * @brief TracedCallback signature for task sent events.
     * @param task The task that was submitted for admission.
     */
    typedef void (*TaskSentTracedCallback)(Ptr<const Task> task);

    /**
     * @brief TracedCallback signature for workload processed events.
     *
     * Fires exactly once per completed workload, regardless of how many tasks
     * (or sink tasks) the DAG contains. The DAG carries every executed task
     * with its recorded backend and compute times.
     *
     * @param dag The completed workload DAG as returned by the orchestrator.
     * @param latency End-to-end latency from submission to response.
     */
    typedef void (*WorkloadProcessedTracedCallback)(Ptr<const DagTask> dag, Time latency);

    /**
     * @brief TracedCallback signature for task rejected events.
     * @param task The rejected task.
     */
    typedef void (*TaskRejectedTracedCallback)(Ptr<const Task> task);

    /**
     * @brief TracedCallback signature for workload deadline violation events.
     * @param dag The completed workload whose latency exceeded the deadline budget.
     * @param latency End-to-end latency from submission to response.
     */
    typedef void (*WorkloadViolatedTracedCallback)(Ptr<const DagTask> dag, Time latency);

    /**
     * @brief TracedCallback signature for task dropped events.
     * @param taskIndex The sequence number of the dropped task.
     */
    typedef void (*TaskDroppedTracedCallback)(uint64_t taskIndex);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    void HandleConnected(const Address& serverAddr);
    void HandleConnectionFailed(const Address& serverAddr);
    void HandleReceive(Ptr<Packet> packet, const Address& from);

    /**
     * @brief Ensure a DagTemplate is set, constructing the historical
     * single-task default (InputSize=OutputSize=1, ComputeDemand=1.0) if
     * none has been assigned.
     */
    void EnsureDagTemplate();

    /**
     * @brief Close the connection manager, if one is set.
     *
     * Shared by StopApplication()'s immediate-close path (GracePeriod == 0
     * or nothing pending) and its delayed-close path (scheduled after
     * GracePeriod, or triggered early once the last pending workload
     * resolves).
     */
    void CloseConnection();

    /**
     * @brief Generate and submit the next task.
     */
    void GenerateTask();

    /**
     * @brief Schedule the next task generation.
     */
    void ScheduleNextTask();

    void ProcessBuffer();

    /**
     * @brief Handle the orchestrator's non-blocking admission acknowledgment.
     *
     * On acceptance: in SessionMode, marks the session admitted (first success
     * only); otherwise (WorkloadMode, or an already-admitted session) there is
     * nothing further to do since the workload's data was already sent.
     * On rejection: marks the workload's tasks rejected; in SessionMode, if
     * the session was not yet admitted, consults the AdmissionRetryPolicy to
     * decide whether to keep attempting session establishment.
     *
     * @param orchHeader The ADMISSION_RESPONSE header.
     */
    void HandleAdmissionResponse(const OrchestratorHeader& orchHeader);

    /**
     * @brief Handle a completed workload returned by the orchestrator.
     *
     * Receiving this message is itself the completion signal for the workload:
     * the payload carries the entire executed DAG, so the pending entry is
     * cleared and the completion traces fire exactly once.
     *
     * @param dagId The DAG ID echoed back by the orchestrator.
     * @param payload Packet containing the serialized completed DAG.
     */
    void HandleWorkloadResponse(uint64_t dagId, Ptr<Packet> payload);

    /**
     * @brief Deserialize one task from a DAG payload, stripping its type prefix.
     *
     * DagTask prepends a 1-byte task type tag to every serialized task. The
     * client currently only submits SimpleTask, so this delegates directly
     * rather than keeping a task type registry as the Orchestrator does.
     *
     * @param packet The type-prefixed task buffer.
     * @param consumedBytes Output: bytes consumed including the type byte, 0 on failure.
     * @return The deserialized task, or nullptr on failure.
     */
    static Ptr<Task> DeserializeResponseTask(Ptr<Packet> packet, uint64_t& consumedBytes);

    // Transport
    Ptr<ConnectionManager> m_connMgr;               //!< Connection manager for transport
    Address m_peer;                                 //!< Remote orchestrator address
    Ptr<ClientSubmissionPolicy> m_submissionPolicy; //!< Policy deciding whether to submit each tick
    AdmissionMode m_admissionMode;                  //!< SessionMode (default) or WorkloadMode
    Ptr<AdmissionRetryPolicy> m_admissionRetryPolicy; //!< SessionMode-only: retry-after-rejection
    Ptr<DagTask> m_dagTemplate; //!< Template DAG (structure + per-task distributions)

    // Configuration
    Ptr<RandomVariableStream> m_interArrival; //!< Inter-arrival time between tasks in seconds
    Time m_deadlineBudget; //!< End-to-end delay budget (0 = use sampled inter-arrival period)
    Time m_commBudget;     //!< Communication budget (T_ul + T_dl estimate)
    Time m_gracePeriod;    //!< Drain window on stop before closing (0 = close immediately)
    Time m_nominalPeriod;                      //!< Inter-arrival period sampled once at startup

    // State
    static uint32_t s_nextClientId; //!< Counter for assigning unique client IDs
    uint32_t m_clientId;            //!< Unique ID for this client instance
    EventId m_sendEvent;            //!< Next task event
    EventId m_closeEvent;           //!< Scheduled end-of-GracePeriod connection close
    uint64_t m_tasksSent;           //!< Tasks successfully submitted for admission
    uint64_t m_taskCount;           //!< Total task generation events (sent + dropped)
    uint64_t m_tasksDropped;        //!< Tasks dropped due to pending workload
    uint64_t m_nextDagId;           //!< Next DAG ID
    uint64_t m_totalTx;             //!< Total bytes transmitted
    uint64_t m_totalRx;             //!< Total bytes received
    uint64_t m_tasksRejected;       //!< Tasks rejected by the orchestrator
    uint64_t m_tasksViolated;       //!< Completed tasks whose latency exceeded the deadline budget
    uint64_t m_lastViolationIndex;  //!< Task index of the most recent violation (0 = never)
    Time m_lastViolationTime;       //!< Simulation time of the most recent violation
    Time m_lastRejectionTime;       //!< Simulation time of the most recent rejection

    // SessionMode state (unused in WorkloadMode)
    bool m_sessionAdmitted;          //!< True once the one-time session admission succeeds
    bool m_sessionAdmissionAbandoned; //!< True once AdmissionRetryPolicy says to give up
    uint32_t m_sessionRejectionCount; //!< Consecutive session-admission rejections so far

    // Pending workload state
    struct PendingWorkload
    {
        Ptr<DagTask> dag; //!< The DAG wrapping the submitted task
        Time submitTime;  //!< When the admission request was sent
    };

    std::map<uint64_t, PendingWorkload> m_pendingWorkloads; //!< dagId -> pending state

    // Response handling
    Ptr<Packet> m_rxBuffer;       //!< Receive buffer for stream reassembly
    uint64_t m_responsesReceived; //!< Number of responses received

    // Trace sources
    TracedCallback<Ptr<const Task>> m_taskSentTrace;                   //!< Task sent
    TracedCallback<Ptr<const DagTask>, Time> m_workloadProcessedTrace; //!< Workload processed
    TracedCallback<Ptr<const Task>> m_taskRejectedTrace;               //!< Task rejected
    TracedCallback<Ptr<const DagTask>, Time> m_workloadViolatedTrace;  //!< Deadline violated
    TracedCallback<uint64_t> m_taskDroppedTrace;                       //!< Task dropped
};

} // namespace ns3

#endif // OFFLOAD_CLIENT_H
