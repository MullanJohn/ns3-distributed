/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

#include "cluster.h"
#include "connection-manager.h"
#include "node-scheduler.h"
#include "offload-header.h"

#include "ns3/application.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <map>
#include <unordered_map>

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
 * The LoadBalancer uses ConnectionManagers for transport abstraction:
 * - Frontend ConnectionManager: accepts client connections, routes responses
 * - Backend ConnectionManager: maintains connections to all backend servers
 * - A mapping from task IDs to client addresses for response routing
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
    typedef void (*TaskForwardedCallback)(const TaskHeader& header, uint32_t backendIndex);

    /**
     * @brief Trace callback signature for response routed events.
     * @param header The response header.
     * @param totalLatency Time from task receipt to response forwarded.
     */
    typedef void (*ResponseRoutedCallback)(const TaskHeader& header, Time totalLatency);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    // ========== ConnectionManager callbacks ==========

    /**
     * @brief Handle data received from a client via frontend ConnectionManager.
     * @param packet The received packet.
     * @param from The client address.
     */
    void HandleFrontendReceive(Ptr<Packet> packet, const Address& from);

    /**
     * @brief Handle data received from a backend via backend ConnectionManager.
     * @param packet The received packet.
     * @param from The backend address.
     */
    void HandleBackendReceive(Ptr<Packet> packet, const Address& from);

    // ========== Task forwarding and response routing ==========

    /**
     * @brief Forward a task to a backend.
     * @param header The task header.
     * @param payload The task payload.
     * @param clientAddr The originating client address for response routing.
     */
    void ForwardTask(const OffloadHeader& header, Ptr<Packet> payload, const Address& clientAddr);

    /**
     * @brief Route a response back to the client.
     * @param header The response header.
     * @param payload The response payload.
     * @param backendAddr The backend that sent the response.
     */
    void RouteResponse(const OffloadHeader& header, Ptr<Packet> payload, const Address& backendAddr);

    // ========== Configuration ==========
    uint16_t m_port;                          //!< Port to listen on
    Ptr<NodeScheduler> m_scheduler;           //!< The scheduling policy
    Ptr<ConnectionManager> m_frontendConnMgr; //!< ConnectionManager for client connections
    Ptr<ConnectionManager> m_backendConnMgr;  //!< ConnectionManager for backend connections

    // ========== Runtime state ==========
    Cluster m_cluster; //!< Backend servers

    // Per-client receive buffers (keyed by client address)
    std::map<Address, Ptr<Packet>> m_clientBuffers;

    // Per-backend receive buffers (keyed by backend address)
    std::map<Address, Ptr<Packet>> m_backendBuffers;

    // Response routing: taskId -> pending info
    struct PendingResponse
    {
        Address clientAddr;    //!< The client address to send response to
        Time arrivalTime;      //!< When task was received from client
        uint32_t backendIndex; //!< Which backend is processing this task
    };

    std::unordered_map<uint64_t, PendingResponse> m_pendingResponses;

    // ========== Statistics ==========
    uint64_t m_tasksForwarded;  //!< Number of tasks forwarded
    uint64_t m_responsesRouted; //!< Number of responses routed
    uint64_t m_clientRx;        //!< Bytes received from clients
    uint64_t m_backendRx;       //!< Bytes received from backends

    // ========== Trace sources ==========
    TracedCallback<const TaskHeader&, uint32_t> m_taskForwardedTrace;
    TracedCallback<const TaskHeader&, Time> m_responseRoutedTrace;
};

} // namespace ns3

#endif // LOAD_BALANCER_H
