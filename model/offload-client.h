/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef OFFLOAD_CLIENT_H
#define OFFLOAD_CLIENT_H

#include "connection-manager.h"
#include "dag-task.h"
#include "orchestrator-header.h"
#include "task.h"

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"

#include <deque>
#include <map>

namespace ns3
{

class Packet;

/**
 * @ingroup distributed
 * @brief Client application for offloading computational tasks via two-phase admission.
 *
 * OffloadClient communicates with an EdgeOrchestrator using the two-phase
 * admission protocol:
 * 1. Client sends ADMISSION_REQUEST with task/DAG metadata
 * 2. Orchestrator responds with ADMISSION_RESPONSE (admit/reject)
 * 3. If admitted, client sends full task data
 * 4. Orchestrator dispatches to backends, collects results
 * 5. Orchestrator sends sink task responses back to client
 *
 * Tasks can be submitted programmatically via SubmitTask(), or auto-generated
 * when MaxTasks > 0.
 *
 * Transport is abstracted via ConnectionManager, defaulting to TCP.
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
     * @brief Submit a task for offloading.
     *
     * Wraps the task as a 1-node DagTask, sends an ADMISSION_REQUEST
     * to the orchestrator, and tracks the pending workload.
     *
     * @param task The task to submit.
     */
    void SubmitTask(Ptr<Task> task);

    /**
     * @brief Get the number of tasks sent.
     * @return Number of tasks sent.
     */
    uint64_t GetTasksSent() const;

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
     * @brief Get the number of responses received.
     * @return Number of responses received.
     */
    uint64_t GetResponsesReceived() const;

    /**
     * @brief TracedCallback signature for task sent events.
     * @param task The task that was submitted.
     */
    typedef void (*TaskSentTracedCallback)(Ptr<const Task> task);

    /**
     * @brief TracedCallback signature for response received events.
     * @param task The completed task from the response.
     * @param rtt Round-trip time from submission to response.
     */
    typedef void (*ResponseReceivedTracedCallback)(Ptr<const Task> task, Time rtt);

    /**
     * @brief TracedCallback signature for task rejected events.
     * @param task The task that was rejected.
     */
    typedef void (*TaskRejectedTracedCallback)(Ptr<const Task> task);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * @brief Handle connection established (TCP-specific).
     * @param serverAddr The server address.
     */
    void HandleConnected(const Address& serverAddr);

    /**
     * @brief Handle connection failure (TCP-specific).
     * @param serverAddr The server address.
     */
    void HandleConnectionFailed(const Address& serverAddr);

    /**
     * @brief Handle data received from the orchestrator.
     * @param packet The received packet.
     * @param from The orchestrator address.
     */
    void HandleReceive(Ptr<Packet> packet, const Address& from);

    /**
     * @brief Generate and submit a task (auto-generation mode).
     */
    void GenerateTask();

    /**
     * @brief Schedule the next task generation.
     */
    void ScheduleNextTask();

    /**
     * @brief Process the receive buffer for complete messages.
     */
    void ProcessBuffer();

    /**
     * @brief Handle an admission response from the orchestrator.
     * @param orchHeader The parsed OrchestratorHeader.
     */
    void HandleAdmissionResponse(const OrchestratorHeader& orchHeader);

    /**
     * @brief Handle a task response (sink task result) from the orchestrator.
     */
    void HandleTaskResponse();

    /**
     * @brief Send full DAG data for an admitted workload.
     * @param dagId The DAG ID.
     */
    void SendFullData(uint64_t dagId);

    // Transport
    Ptr<ConnectionManager> m_connMgr; //!< Connection manager for transport
    Address m_peer;                   //!< Remote orchestrator address

    // Random variable streams (for auto-generation mode)
    Ptr<RandomVariableStream> m_interArrivalTime; //!< Inter-arrival time RNG
    Ptr<RandomVariableStream> m_computeDemand;    //!< Compute demand RNG
    Ptr<RandomVariableStream> m_inputSize;        //!< Input size RNG
    Ptr<RandomVariableStream> m_outputSize;       //!< Output size RNG

    // Configuration
    uint64_t m_maxTasks; //!< Maximum tasks to send (0 = programmatic only)

    // State
    static uint32_t s_nextClientId; //!< Counter for assigning unique client IDs
    uint32_t m_clientId;            //!< Unique ID for this client instance
    EventId m_sendEvent;            //!< Next send event
    uint64_t m_taskCount;           //!< Number of tasks sent (sequence counter)
    uint64_t m_totalTx;             //!< Total bytes transmitted
    uint64_t m_totalRx;             //!< Total bytes received
    uint64_t m_nextDagId;           //!< Next DAG ID for workload tracking

    // Pending workload state
    struct PendingWorkload
    {
        Ptr<DagTask> dag; //!< The DAG wrapping the task
        Time submitTime;  //!< When the admission request was sent
    };

    std::map<uint64_t, PendingWorkload> m_pendingWorkloads; //!< dagId → pending state
    std::deque<uint64_t> m_admittedQueue;                   //!< dagIds admitted, awaiting data send

    // Response handling
    Ptr<Packet> m_rxBuffer;       //!< Receive buffer for stream reassembly
    uint64_t m_responsesReceived; //!< Number of responses received

    // Trace sources
    TracedCallback<Ptr<const Task>> m_taskSentTrace;               //!< Task sent trace
    TracedCallback<Ptr<const Task>, Time> m_responseReceivedTrace; //!< Response received trace
    TracedCallback<Ptr<const Task>> m_taskRejectedTrace;           //!< Task rejected trace
};

} // namespace ns3

#endif // OFFLOAD_CLIENT_H
