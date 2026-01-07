/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef OFFLOAD_SERVER_H
#define OFFLOAD_SERVER_H

#include "gpu-accelerator.h"
#include "offload-header.h"
#include "task.h"

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <list>
#include <map>

namespace ns3
{

class Socket;
class Packet;

/**
 * @ingroup distributed
 * @brief TCP server application for receiving and processing offloaded tasks.
 *
 * OffloadServer listens for TCP connections from OffloadClient applications,
 * receives task requests, submits them to the GpuAccelerator aggregated to
 * the node, and sends responses back when tasks complete.
 *
 * Since TCP is a stream protocol, the server buffers incoming data per-client
 * and extracts complete OffloadHeader messages as they arrive.
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
     * @brief Get the local address the server is bound to.
     * @return The local address.
     */
    Address GetLocalAddress() const;

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
     * @brief Handle a packet received on a socket.
     * @param socket The receiving socket.
     */
    void HandleRead(Ptr<Socket> socket);

    /**
     * @brief Handle an incoming connection.
     * @param socket The new connection socket.
     * @param from The address of the client.
     */
    void HandleAccept(Ptr<Socket> socket, const Address& from);

    /**
     * @brief Handle a connection close.
     * @param socket The closed socket.
     */
    void HandlePeerClose(Ptr<Socket> socket);

    /**
     * @brief Handle a connection error.
     * @param socket The socket with error.
     */
    void HandlePeerError(Ptr<Socket> socket);

    /**
     * @brief Process buffered data for a client, extracting complete messages.
     * @param socket The client socket.
     */
    void ProcessBuffer(Ptr<Socket> socket);

    /**
     * @brief Process a complete task request.
     * @param header The offload header.
     * @param socket The client socket.
     */
    void ProcessTask(const OffloadHeader& header, Ptr<Socket> socket);

    /**
     * @brief Called when a task completes on the GPU.
     * @param task The completed task.
     * @param duration The processing duration.
     */
    void OnTaskCompleted(Ptr<const Task> task, Time duration);

    /**
     * @brief Send a response to the client.
     * @param socket The client socket.
     * @param task The completed task.
     * @param duration The processing duration.
     */
    void SendResponse(Ptr<Socket> socket, Ptr<const Task> task, Time duration);

    /**
     * @brief Clean up pending tasks for a closed socket.
     * @param socket The closed socket.
     */
    void CleanupSocket(Ptr<Socket> socket);

    // Configuration
    Address m_local; //!< Local address to bind to
    uint16_t m_port; //!< Port to listen on

    // Sockets
    Ptr<Socket> m_socket;                //!< IPv4 listening socket
    Ptr<Socket> m_socket6;               //!< IPv6 listening socket (used if only port is specified)
    std::list<Ptr<Socket>> m_socketList; //!< Accepted client sockets

    // Per-client receive buffers keyed by socket (for TCP stream reassembly)
    std::map<Ptr<Socket>, Ptr<Packet>> m_rxBuffer;

    // GPU accelerator
    Ptr<GpuAccelerator> m_accelerator; //!< Cached accelerator reference

    // Pending tasks: taskId -> (socket, task)
    struct PendingTask
    {
        Ptr<Socket> socket;
        Ptr<Task> task;
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
