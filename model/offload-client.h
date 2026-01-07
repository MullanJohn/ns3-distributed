/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef OFFLOAD_CLIENT_H
#define OFFLOAD_CLIENT_H

#include "offload-header.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"

#include <map>

namespace ns3
{

class Socket;
class Packet;

/**
 * @ingroup distributed
 * @brief TCP client application for offloading computational tasks.
 *
 * OffloadClient generates computational tasks and sends them to a remote
 * server over TCP. Tasks are generated with configurable random distributions
 * for inter-arrival time, compute demand, and input/output sizes.
 *
 * Each task is sent as a packet with an OffloadHeader followed by payload
 * data sized to match the task's input size, simulating realistic data
 * transfer for computation offloading scenarios.
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
     * @brief Set the remote server address.
     * @param addr Server address (InetSocketAddress or Inet6SocketAddress).
     */
    void SetRemote(const Address& addr);

    /**
     * @brief Get the remote server address.
     * @return The server address.
     */
    Address GetRemote() const;

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
     * @brief TracedCallback signature for task sent events.
     * @param header The offload header that was sent.
     */
    typedef void (*TaskSentTracedCallback)(const OffloadHeader& header);

    /**
     * @brief TracedCallback signature for response received events.
     * @param header The offload header from the response.
     * @param rtt Round-trip time from task sent to response received.
     */
    typedef void (*ResponseReceivedTracedCallback)(const OffloadHeader& header, Time rtt);

    /**
     * @brief Get the number of responses received.
     * @return Number of responses received.
     */
    uint64_t GetResponsesReceived() const;

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * @brief Callback for successful connection.
     * @param socket The connected socket.
     */
    void ConnectionSucceeded(Ptr<Socket> socket);

    /**
     * @brief Callback for failed connection.
     * @param socket The socket.
     */
    void ConnectionFailed(Ptr<Socket> socket);

    /**
     * @brief Generate and send a task.
     */
    void SendTask();

    /**
     * @brief Schedule the next task generation.
     */
    void ScheduleNextTask();

    /**
     * @brief Handle data received from the server.
     * @param socket The socket with incoming data.
     */
    void HandleRead(Ptr<Socket> socket);

    /**
     * @brief Process the receive buffer for complete messages.
     */
    void ProcessBuffer();

    // Socket
    Ptr<Socket> m_socket; //!< TCP socket
    Address m_peer;       //!< Remote server address
    Address m_local;      //!< Local bind address
    bool m_connected;     //!< Connection state

    // Random variable streams
    Ptr<RandomVariableStream> m_interArrivalTime; //!< Inter-arrival time RNG
    Ptr<RandomVariableStream> m_computeDemand;    //!< Compute demand RNG
    Ptr<RandomVariableStream> m_inputSize;        //!< Input size RNG
    Ptr<RandomVariableStream> m_outputSize;       //!< Output size RNG

    // Configuration
    uint64_t m_maxTasks; //!< Maximum tasks to send (0 = unlimited)

    // State
    static uint32_t s_nextClientId; //!< Counter for assigning unique client IDs
    uint32_t m_clientId;            //!< Unique ID for this client instance
    EventId m_sendEvent;            //!< Next send event
    uint64_t m_taskCount;           //!< Number of tasks sent
    uint64_t m_totalTx;             //!< Total bytes transmitted
    uint64_t m_totalRx;             //!< Total bytes received

    // Response handling
    Ptr<Packet> m_rxBuffer;               //!< Receive buffer for TCP stream
    uint64_t m_responsesReceived;         //!< Number of responses received
    std::map<uint64_t, Time> m_sendTimes; //!< Map of task ID to send time

    // Trace sources
    TracedCallback<const OffloadHeader&> m_taskSentTrace;               //!< Task sent trace
    TracedCallback<const OffloadHeader&, Time> m_responseReceivedTrace; //!< Response received trace
};

} // namespace ns3

#endif // OFFLOAD_CLIENT_H
