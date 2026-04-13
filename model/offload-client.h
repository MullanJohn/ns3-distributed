/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef OFFLOAD_CLIENT_H
#define OFFLOAD_CLIENT_H

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
 * Example usage (helper convenience):
 * @code
 * OffloadClientHelper helper(orchAddress);
 * helper.SetTaskRate(30.0);
 * helper.SetMeanInputSize(60000);
 * helper.SetComputeDemand(5e9);
 * helper.Install(node);
 * @endcode
 */
class OffloadClient : public Application
{
  public:
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
     * @brief TracedCallback signature for task processed events.
     * @param task The completed task.
     * @param latency End-to-end latency from submission to response.
     */
    typedef void (*TaskProcessedTracedCallback)(Ptr<const Task> task, Time latency);

    /**
     * @brief TracedCallback signature for task rejected events.
     * @param task The rejected task.
     */
    typedef void (*TaskRejectedTracedCallback)(Ptr<const Task> task);

    /**
     * @brief TracedCallback signature for task deadline violation events.
     * @param task The completed task whose latency exceeded the deadline budget.
     * @param latency End-to-end latency from submission to response.
     */
    typedef void (*TaskViolatedTracedCallback)(Ptr<const Task> task, Time latency);

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
     * @brief Generate and submit the next task.
     */
    void GenerateTask();

    /**
     * @brief Schedule the next task generation.
     */
    void ScheduleNextTask();

    void ProcessBuffer();
    void HandleAdmissionResponse(const OrchestratorHeader& orchHeader);
    void HandleTaskResponse();
    void SendFullData(uint64_t dagId);

    // Transport
    Ptr<ConnectionManager> m_connMgr; //!< Connection manager for transport
    Address m_peer;                   //!< Remote orchestrator address
    Ptr<ClientSubmissionPolicy> m_submissionPolicy; //!< Policy deciding whether to submit each tick

    // Configuration
    Ptr<RandomVariableStream> m_interArrival;  //!< Inter-arrival time between tasks in seconds
    Time m_deadlineBudget;                     //!< End-to-end delay budget (0 = use sampled inter-arrival period)
    Time m_commBudget;                         //!< Communication budget (T_ul + T_dl estimate)
    Ptr<RandomVariableStream> m_inputSize;     //!< Input size per task in bytes
    Ptr<RandomVariableStream> m_computeDemand; //!< FLOPS per task
    Ptr<RandomVariableStream> m_outputSize;    //!< Result size in bytes
    Time m_nominalPeriod;                      //!< Inter-arrival period sampled once at startup

    // State
    static uint32_t s_nextClientId; //!< Counter for assigning unique client IDs
    uint32_t m_clientId;            //!< Unique ID for this client instance
    EventId m_sendEvent;            //!< Next task event
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
    TracedCallback<Ptr<const Task>> m_taskSentTrace;            //!< Task sent
    TracedCallback<Ptr<const Task>, Time> m_taskProcessedTrace; //!< Task processed
    TracedCallback<Ptr<const Task>> m_taskRejectedTrace;        //!< Task rejected
    TracedCallback<Ptr<const Task>, Time> m_taskViolatedTrace;  //!< Deadline violated
    TracedCallback<uint64_t> m_taskDroppedTrace;                //!< Task dropped
};

} // namespace ns3

#endif // OFFLOAD_CLIENT_H
