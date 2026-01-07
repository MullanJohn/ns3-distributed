/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

#include "address-hash.h"
#include "cluster.h"
#include "node-scheduler.h"
#include "offload-header.h"

#include "ns3/application.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/traced-callback.h"

#include <list>
#include <map>
#include <unordered_map>
#include <vector>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Load balancer application for distributing tasks across a cluster.
 *
 * LoadBalancer sits between OffloadClients and a cluster of OffloadServers.
 * It receives task requests from clients, selects a backend server using the
 * configured NodeScheduler, forwards the task, and routes responses back to
 * the originating client.
 *
 * Architecture:
 * @code
 * OffloadClient(s) ---> LoadBalancer ---> OffloadServer(s)
 *                           |                    |
 *                           | NodeScheduler      | GpuAccelerator
 *                           | selects backend    | processes task
 *                           |                    |
 *                      <--- routes response <----
 * @endcode
 *
 * The LoadBalancer maintains:
 * - A listening socket for incoming client connections
 * - Persistent TCP connections to all backend servers
 * - A mapping from task IDs to client sockets for response routing
 */
class LoadBalancer : public Application
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Default constructor.
     */
    LoadBalancer();

    /**
     * @brief Destructor.
     */
    ~LoadBalancer() override;

    /**
     * @brief Set the cluster of backend servers.
     *
     * Must be called before the application starts.
     *
     * @param cluster The cluster to load balance across.
     */
    void SetCluster(const Cluster& cluster);

    /**
     * @brief Get the number of tasks forwarded to backends.
     * @return Number of tasks forwarded.
     */
    uint64_t GetTasksForwarded() const;

    /**
     * @brief Get the number of responses routed back to clients.
     * @return Number of responses routed.
     */
    uint64_t GetResponsesRouted() const;

    /**
     * @brief Get the total bytes received from clients.
     * @return Total bytes received from clients.
     */
    uint64_t GetClientRx() const;

    /**
     * @brief Get the total bytes received from backends.
     * @return Total bytes received from backends.
     */
    uint64_t GetBackendRx() const;

    /**
     * @brief Trace callback signature for task forwarded events.
     * @param header The task header.
     * @param backendIndex The backend index selected.
     */
    typedef void (*TaskForwardedCallback)(const OffloadHeader& header, uint32_t backendIndex);

    /**
     * @brief Trace callback signature for response routed events.
     * @param header The response header.
     * @param totalLatency Time from task receipt to response forwarded.
     */
    typedef void (*ResponseRoutedCallback)(const OffloadHeader& header, Time totalLatency);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    // ========== Client-facing (incoming) ==========

    /**
     * @brief Handle a new client connection.
     */
    void HandleClientAccept(Ptr<Socket> socket, const Address& from);

    /**
     * @brief Handle data received from a client.
     */
    void HandleClientRead(Ptr<Socket> socket);

    /**
     * @brief Process buffered data from a client.
     */
    void ProcessClientBuffer(Ptr<Socket> socket, const Address& from);

    /**
     * @brief Handle client socket close.
     */
    void HandleClientClose(Ptr<Socket> socket);

    /**
     * @brief Handle client socket error.
     */
    void HandleClientError(Ptr<Socket> socket);

    /**
     * @brief Clean up state for a closed client socket.
     */
    void CleanupClientSocket(Ptr<Socket> socket);

    // ========== Backend-facing (outgoing) ==========

    /**
     * @brief Connect to all backend servers.
     */
    void ConnectToBackends();

    /**
     * @brief Handle successful backend connection.
     */
    void HandleBackendConnected(Ptr<Socket> socket);

    /**
     * @brief Handle failed backend connection.
     */
    void HandleBackendFailed(Ptr<Socket> socket);

    /**
     * @brief Handle data received from a backend.
     */
    void HandleBackendRead(Ptr<Socket> socket);

    /**
     * @brief Process buffered data from a backend.
     */
    void ProcessBackendBuffer(uint32_t backendIndex);

    // ========== Task forwarding and response routing ==========

    /**
     * @brief Forward a task to a backend.
     */
    void ForwardTask(const OffloadHeader& header,
                     Ptr<Packet> payload,
                     Ptr<Socket> clientSocket,
                     const Address& clientAddr);

    /**
     * @brief Route a response back to the client.
     */
    void RouteResponse(const OffloadHeader& header, Ptr<Packet> payload, uint32_t backendIndex);

    // ========== Configuration ==========
    uint16_t m_port;                //!< Port to listen on
    Ptr<NodeScheduler> m_scheduler; //!< The scheduling policy

    // ========== Runtime state ==========
    Cluster m_cluster;          //!< Backend servers
    Ptr<Socket> m_listenSocket; //!< Listening socket for clients

    // Client connections
    std::list<Ptr<Socket>> m_clientSockets; //!< Connected client sockets
    std::unordered_map<Address, Ptr<Packet>, AddressHash>
        m_clientRxBuffers; //!< Per-client receive buffers
    std::map<Ptr<Socket>, Address>
        m_clientSocketAddresses; //!< Socket-to-address for buffer cleanup

    // Backend connections
    std::vector<Ptr<Socket>> m_backendSockets;         //!< Sockets to backends
    std::vector<bool> m_backendConnected;              //!< Connection status per backend
    std::vector<Ptr<Packet>> m_backendRxBuffers;       //!< Per-backend receive buffers
    std::map<Ptr<Socket>, uint32_t> m_socketToBackend; //!< Map socket to backend index

    // Response routing: taskId -> pending info
    struct PendingResponse
    {
        Ptr<Socket> clientSocket; //!< The client socket to send response to
        Address clientAddr;       //!< The client address
        Time arrivalTime;         //!< When task was received from client
        uint32_t backendIndex;    //!< Which backend is processing this task
    };

    std::unordered_map<uint64_t, PendingResponse> m_pendingResponses;

    // ========== Statistics ==========
    uint64_t m_tasksForwarded;  //!< Number of tasks forwarded
    uint64_t m_responsesRouted; //!< Number of responses routed
    uint64_t m_clientRx;        //!< Bytes received from clients
    uint64_t m_backendRx;       //!< Bytes received from backends

    // ========== Trace sources ==========
    TracedCallback<const OffloadHeader&, uint32_t> m_taskForwardedTrace;
    TracedCallback<const OffloadHeader&, Time> m_responseRoutedTrace;
};

} // namespace ns3

#endif // LOAD_BALANCER_H
