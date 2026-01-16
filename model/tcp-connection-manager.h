/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef TCP_CONNECTION_MANAGER_H
#define TCP_CONNECTION_MANAGER_H

#include "connection-manager.h"

#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"

#include <list>
#include <map>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief TCP implementation of ConnectionManager with optional connection pooling.
 *
 * TcpConnectionManager provides reliable, ordered delivery using TCP sockets.
 * It supports both client and server modes with full bidirectional communication.
 *
 * ## Client Mode Features
 *
 * **Multiple Servers**: Connect() can be called multiple times with different
 * addresses to establish connections to multiple servers. Use Send(packet, addr)
 * to route to a specific server:
 * @code
 * conn->Connect(server1Address);
 * conn->Connect(server2Address);
 * conn->Send(packet, server1Address);  // Routes to server1
 * @endcode
 *
 * **Connection Pooling**: When connecting to a single server with PoolSize > 1,
 * multiple TCP connections are created to improve throughput by avoiding
 * head-of-line blocking:
 * @code
 * conn->SetAttribute("PoolSize", UintegerValue(4));
 * conn->Connect(serverAddress);
 * conn->Send(packet);  // Manager picks an idle connection
 * @endcode
 *
 * **Explicit Connection Control** (for streaming or metrics):
 * @code
 * auto connId = conn->AcquireConnection();
 * conn->Send(connId, packet);  // All packets on same connection
 * // ... receive response ...
 * conn->ReleaseConnection(connId);
 * @endcode
 *
 * ## Stream Reassembly
 *
 * TCP is a stream protocol, so received data may be fragmented or coalesced.
 * The ReceiveCallback delivers data as it arrives. Applications that need
 * message framing should handle it at the application layer (e.g., using
 * headers with length fields).
 */
class TcpConnectionManager : public ConnectionManager
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
    TcpConnectionManager();

    /**
     * @brief Destructor.
     */
    ~TcpConnectionManager() override;

    /**
     * @brief Connection identifier for explicit connection control.
     */
    typedef uint64_t ConnectionId;

    /**
     * @brief Invalid connection identifier.
     */
    static const ConnectionId INVALID_CONNECTION = 0;

    /**
     * @brief Callback signature for TCP connection events.
     *
     * Used for new connection notifications (server/client) and
     * connection close notifications.
     *
     * @param peer The address of the remote peer.
     */
    typedef Callback<void, const Address&> ConnectionCallback;

    // ConnectionManager interface implementation
    void SetNode(Ptr<Node> node) override;
    Ptr<Node> GetNode() const override;
    void Bind(uint16_t port) override;
    void Bind(const Address& local) override;
    void Connect(const Address& remote) override;
    void Send(Ptr<Packet> packet) override;
    void Send(Ptr<Packet> packet, const Address& to) override;
    void SetReceiveCallback(ReceiveCallback callback) override;
    void Close() override;
    void Close(const Address& peer) override;
    std::string GetName() const override;
    bool IsReliable() const override;
    bool IsConnected() const override;

    /**
     * @brief Set the callback for new connection events (TCP-specific).
     *
     * For servers, invoked when a new client connects.
     * For clients, invoked when connection is established.
     *
     * @param callback The connection callback.
     */
    void SetConnectionCallback(ConnectionCallback callback);

    /**
     * @brief Set the callback for connection close events (TCP-specific).
     *
     * Invoked when a connection is closed, either by the remote peer
     * or due to an error.
     *
     * @param callback The close callback.
     */
    void SetCloseCallback(ConnectionCallback callback);

    /**
     * @brief Acquire an idle connection from the pool (client mode).
     *
     * Returns a connection identifier that can be used with Send(connId, packet).
     * The connection is marked as busy until ReleaseConnection() is called.
     *
     * @return A connection ID, or INVALID_CONNECTION if no idle connections.
     */
    ConnectionId AcquireConnection();

    /**
     * @brief Acquire the connection to a specific peer (server mode).
     *
     * @param peer The peer address.
     * @return A connection ID, or INVALID_CONNECTION if not connected to that peer.
     */
    ConnectionId AcquireConnection(const Address& peer);

    /**
     * @brief Send a packet on a specific connection.
     *
     * @param connId The connection identifier from AcquireConnection().
     * @param packet The packet to send.
     */
    void Send(ConnectionId connId, Ptr<Packet> packet);

    /**
     * @brief Release a connection back to the pool.
     *
     * @param connId The connection identifier to release.
     */
    void ReleaseConnection(ConnectionId connId);

    /**
     * @brief Get the number of connections in the pool.
     *
     * @return The pool size (client mode) or number of accepted connections (server mode).
     */
    uint32_t GetConnectionCount() const;

    /**
     * @brief Get the number of idle (available) connections.
     *
     * @return The number of connections not currently acquired.
     */
    uint32_t GetIdleConnectionCount() const;

    /**
     * @brief Get the number of active (acquired) connections.
     *
     * @return The number of connections currently in use.
     */
    uint32_t GetActiveConnectionCount() const;

  protected:
    void DoDispose() override;

  private:
    void CreateConnectionTo(const Address& remote);
    void CreatePooledConnections(const Address& remote);
    void HandleConnectionSucceeded(Ptr<Socket> socket);
    void HandleConnectionFailed(Ptr<Socket> socket);
    void HandleAccept(Ptr<Socket> socket, const Address& from);
    void HandleRead(Ptr<Socket> socket);
    void HandlePeerClose(Ptr<Socket> socket);
    void HandlePeerError(Ptr<Socket> socket);
    void CleanupSocket(Ptr<Socket> socket);
    Address GetPeerAddress(Ptr<Socket> socket) const;
    ConnectionId GenerateConnectionId();
    Ptr<Socket> GetIdleSocket();
    Ptr<Socket> GetIdleSocketTo(const Address& peer);
    uint32_t GetUniqueRemoteCount() const;

    Ptr<Node> m_node;
    uint32_t m_poolSize;

    // Listening socket (server mode) - non-null indicates server mode
    Ptr<Socket> m_listenSocket;

    // Connection pool (client mode) or accepted connections (server mode)
    std::list<Ptr<Socket>> m_sockets;
    std::map<Ptr<Socket>, bool> m_socketBusy;
    std::map<Ptr<Socket>, Address> m_socketToPeer;
    std::map<Address, Ptr<Socket>> m_peerToSocket;

    // Connection ID mapping for explicit control
    ConnectionId m_nextConnectionId;
    std::map<ConnectionId, Ptr<Socket>> m_idToSocket;
    std::map<Ptr<Socket>, ConnectionId> m_socketToId;

    // Callbacks
    ReceiveCallback m_receiveCallback;
    ConnectionCallback m_connectionCallback;
    ConnectionCallback m_closeCallback;
};

} // namespace ns3

#endif // TCP_CONNECTION_MANAGER_H
