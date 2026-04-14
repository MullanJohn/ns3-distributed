/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef BACKEND_SERVER_H
#define BACKEND_SERVER_H

#include "accelerator.h"
#include "connection-manager.h"
#include "task.h"

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <map>
#include <unordered_map>

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Backend server that processes tasks on an accelerator.
 *
 * BackendServer receives tasks from an Orchestrator, submits them to the
 * Accelerator aggregated on the node, and sends processing results back when
 * tasks complete.
 *
 * Example usage:
 * @code
 * Ptr<BackendServer> server = CreateObject<BackendServer>();
 * server->SetAttribute("Port", UintegerValue(9000));
 * serverNode->AddApplication(server);
 * // Requires a GpuAccelerator aggregated on serverNode
 * @endcode
 */
class BackendServer : public Application
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    BackendServer();
    ~BackendServer() override;

    /**
     * @brief Get the number of tasks received.
     * @return Number of tasks received.
     */
    uint64_t GetTasksReceived() const;

    /**
     * @brief Get the number of tasks processed.
     * @return Number of tasks completed and response sent.
     */
    uint64_t GetTasksProcessed() const;

    /**
     * @brief Get the total bytes received.
     * @return Total bytes received.
     */
    uint64_t GetTotalRx() const;

    /**
     * @brief Get the port number.
     * @return The listening port.
     */
    uint16_t GetPort() const;

    /**
     * @brief TracedCallback signature for task received events.
     * @param task The task that was received.
     */
    typedef void (*TaskReceivedTracedCallback)(Ptr<const Task> task);

    /**
     * @brief TracedCallback signature for task processed events.
     * @param task The completed task.
     * @param duration The processing duration on the accelerator.
     */
    typedef void (*TaskProcessedTracedCallback)(Ptr<const Task> task, Time duration);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    void HandleReceive(Ptr<Packet> packet, const Address& from);
    void HandleClientClose(const Address& clientAddr);
    void ProcessBuffer(const Address& clientAddr);
    void ProcessTask(Ptr<Task> task, const Address& clientAddr);
    void OnTaskCompleted(Ptr<const Task> task, Time duration);
    void SendResponse(const Address& clientAddr, Ptr<const Task> task, Time duration);
    void HandleScalingCommand(Ptr<Packet> buffer);
    void CleanupClient(const Address& clientAddr);

    // Configuration
    uint16_t m_port; //!< Port to listen on

    // Transport
    Ptr<ConnectionManager> m_connMgr; //!< Connection manager for transport

    // Accelerator
    Ptr<Accelerator> m_accelerator; //!< Cached accelerator reference

    // Per-client receive buffers
    std::map<Address, Ptr<Packet>> m_rxBuffer;

    // Pending tasks: taskId -> pending info
    struct PendingTask
    {
        Address clientAddr; //!< Client address for response routing
        Ptr<Task> task;     //!< The task being processed
    };

    std::unordered_map<uint64_t, PendingTask> m_pendingTasks;

    // Statistics
    uint64_t m_tasksReceived;  //!< Number of tasks received
    uint64_t m_tasksProcessed; //!< Number of tasks processed
    uint64_t m_totalRx;        //!< Total bytes received

    // Trace sources
    TracedCallback<Ptr<const Task>> m_taskReceivedTrace;        //!< Task received
    TracedCallback<Ptr<const Task>, Time> m_taskProcessedTrace; //!< Task processed
};

} // namespace ns3

#endif // BACKEND_SERVER_H
