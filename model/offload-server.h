/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef OFFLOAD_SERVER_H
#define OFFLOAD_SERVER_H

#include "accelerator.h"
#include "connection-manager.h"
#include "offload-header.h"
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
 * @ingroup distributed
 * @brief Server application for receiving and processing offloaded tasks.
 *
 * OffloadServer listens for connections from OffloadClient applications,
 * receives task requests, submits them to the Accelerator aggregated to
 * the node, and sends responses back when tasks complete.
 *
 * Transport is abstracted via ConnectionManager, defaulting to TCP.
 * Users can inject a custom ConnectionManager (e.g., UDP) via the
 * "ConnectionManager" attribute.
 */
class OffloadServer : public Application
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    OffloadServer();
    ~OffloadServer() override;

    /**
     * @brief Get the number of tasks received.
     * @return Number of tasks received.
     */
    uint64_t GetTasksReceived() const;

    /**
     * @brief Get the number of tasks completed.
     * @return Number of tasks completed.
     */
    uint64_t GetTasksCompleted() const;

    /**
     * @brief Get the total bytes received.
     * @return Total bytes received.
     */
    uint64_t GetTotalRx() const;

    /**
     * @brief Get the port number the server is listening on.
     * @return The port number.
     */
    uint16_t GetPort() const;

    /**
     * @brief TracedCallback signature for task received events.
     * @param header The offload header that was received.
     */
    typedef void (*TaskReceivedTracedCallback)(const OffloadHeader& header);

    /**
     * @brief TracedCallback signature for task completed events.
     * @param header The offload header for the response.
     * @param duration The processing duration.
     */
    typedef void (*TaskCompletedTracedCallback)(const OffloadHeader& header, Time duration);

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
     * @brief Process buffered data for a client, extracting complete messages.
     * @param clientAddr The client address.
     */
    void ProcessBuffer(const Address& clientAddr);

    /**
     * @brief Process a complete task request.
     * @param header The offload header.
     * @param clientAddr The client address for response routing.
     */
    void ProcessTask(const OffloadHeader& header, const Address& clientAddr);

    /**
     * @brief Called when a task completes on the accelerator.
     * @param task The completed task.
     * @param duration The processing duration.
     */
    void OnTaskCompleted(Ptr<const Task> task, Time duration);

    /**
     * @brief Send a response to the client.
     * @param clientAddr The client address.
     * @param task The completed compute task.
     * @param duration The processing duration.
     */
    void SendResponse(const Address& clientAddr, Ptr<const Task> task, Time duration);

    /**
     * @brief Clean up state for a disconnected client.
     * @param clientAddr The client address.
     */
    void CleanupClient(const Address& clientAddr);

    // Configuration
    uint16_t m_port; //!< Port to listen on

    // Transport
    Ptr<ConnectionManager> m_connMgr; //!< Connection manager for transport

    // Accelerator
    Ptr<Accelerator> m_accelerator; //!< Cached accelerator reference

    // Per-client receive buffers (keyed by client address)
    std::map<Address, Ptr<Packet>> m_rxBuffer;

    // Pending tasks: taskId -> pending info
    struct PendingTask
    {
        Address clientAddr; //!< Client address for response routing
        Ptr<Task> task; //!< The task being processed
    };

    std::unordered_map<uint64_t, PendingTask> m_pendingTasks;

    // Statistics
    uint64_t m_tasksReceived;  //!< Number of tasks received
    uint64_t m_tasksCompleted; //!< Number of tasks completed
    uint64_t m_totalRx;        //!< Total bytes received

    // Trace sources
    TracedCallback<const OffloadHeader&> m_taskReceivedTrace;        //!< Task received
    TracedCallback<const OffloadHeader&, Time> m_taskCompletedTrace; //!< Task completed
};

} // namespace ns3

#endif // OFFLOAD_SERVER_H
