/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Abstract base class for transport-layer connection management.
 *
 * ConnectionManager provides a protocol-agnostic interface for managing
 * network connections. It abstracts the differences between connection-oriented
 * (TCP) and connectionless (UDP) transports, allowing applications to switch
 * between protocols without changing their code structure.
 *
 * The interface supports both client and server modes:
 * - **Client mode**: Call Connect() to establish connection(s) to a server
 * - **Server mode**: Call Bind() to accept incoming connections
 *
 * Both modes support bidirectional communication via Send() and ReceiveCallback.
 *
 * Implementations may provide protocol-specific extensions. For example,
 * TcpConnectionManager adds connection pooling control methods that are
 * not part of this base interface.
 *
 * Example client usage:
 * @code
 * Ptr<ConnectionManager> conn = CreateObject<TcpConnectionManager>();
 * conn->SetNode(node);
 * conn->SetReceiveCallback(MakeCallback(&MyApp::OnReceive, this));
 * conn->Connect(serverAddress);
 * conn->Send(packet);
 * @endcode
 *
 * Example server usage:
 * @code
 * Ptr<ConnectionManager> conn = CreateObject<TcpConnectionManager>();
 * conn->SetNode(node);
 * conn->SetReceiveCallback(MakeCallback(&MyApp::OnReceive, this));
 * conn->SetConnectionCallback(MakeCallback(&MyApp::OnNewClient, this));
 * conn->Bind(9000);
 * // Later, in OnReceive:
 * conn->Send(responsePacket, clientAddress);
 * @endcode
 */
class ConnectionManager : public Object
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
    ConnectionManager();

    /**
     * @brief Destructor.
     */
    ~ConnectionManager() override;

    /**
     * @brief Callback signature for receiving data.
     *
     * @param packet The received packet.
     * @param from The address of the sender.
     */
    typedef Callback<void, Ptr<Packet>, const Address&> ReceiveCallback;

    /**
     * @brief Set the node this connection manager operates on.
     *
     * Must be called before Bind() or Connect().
     *
     * @param node The node to use for socket creation.
     */
    virtual void SetNode(Ptr<Node> node) = 0;

    /**
     * @brief Get the node this connection manager operates on.
     *
     * @return The node, or nullptr if not set.
     */
    virtual Ptr<Node> GetNode() const = 0;

    /**
     * @brief Bind to a port and start accepting connections (server mode).
     *
     * For TCP, this creates a listening socket and accepts incoming connections.
     * For UDP, this binds to the port and starts receiving datagrams.
     *
     * @param port The port number to bind to.
     */
    virtual void Bind(uint16_t port) = 0;

    /**
     * @brief Bind to a specific local address and start accepting connections.
     *
     * @param local The local address (IP and port) to bind to.
     */
    virtual void Bind(const Address& local) = 0;

    /**
     * @brief Connect to a remote server (client mode).
     *
     * For TCP, this establishes a connection to the server. If connection
     * pooling is enabled (TcpConnectionManager), multiple connections may
     * be created.
     *
     * For UDP, this sets the default destination address for Send().
     *
     * @param remote The remote server address to connect to.
     */
    virtual void Connect(const Address& remote) = 0;

    /**
     * @brief Send a packet to the default/connected peer.
     *
     * In client mode, sends to the server specified in Connect().
     * In server mode with a single client, sends to that client.
     *
     * @param packet The packet to send.
     */
    virtual void Send(Ptr<Packet> packet) = 0;

    /**
     * @brief Send a packet to a specific peer.
     *
     * In server mode, this routes the packet to the connection for that peer.
     * In client mode with connection pooling, this sends to the server
     * (the 'to' address should match the connected server).
     *
     * @param packet The packet to send.
     * @param to The destination address.
     */
    virtual void Send(Ptr<Packet> packet, const Address& to) = 0;

    /**
     * @brief Set the callback for receiving data.
     *
     * The callback is invoked whenever data is received, with the packet
     * and the sender's address.
     *
     * @param callback The receive callback.
     */
    virtual void SetReceiveCallback(ReceiveCallback callback) = 0;

    /**
     * @brief Close all connections and release resources.
     */
    virtual void Close() = 0;

    /**
     * @brief Close the connection(s) to a specific peer.
     *
     * For TCP in server mode, closes the socket for that specific client.
     * For TCP in client mode with connection pooling, closes ALL pooled
     * connections to the server (since all pool connections share the same peer).
     * For UDP, this is a no-op (connectionless).
     *
     * @param peer The peer address to disconnect.
     */
    virtual void Close(const Address& peer) = 0;

    /**
     * @brief Get the name of this connection manager implementation.
     *
     * @return A string identifying the implementation (e.g., "TCP", "UDP").
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Check if this transport provides reliable delivery.
     *
     * @return True for TCP, false for UDP.
     */
    virtual bool IsReliable() const = 0;

  protected:
    void DoDispose() override;

    /**
     * @brief Traced callback signature for packet transmission/reception.
     *
     * @param packet The packet being sent or received.
     * @param address The remote address (destination for Tx, source for Rx).
     */
    typedef void (*PacketAddressTracedCallback)(Ptr<const Packet> packet, const Address& address);

    /**
     * @brief Trace fired when a packet is successfully sent.
     *
     * Provides the packet and destination address. Useful for measuring
     * throughput and correlating requests with responses via packet UID.
     */
    TracedCallback<Ptr<const Packet>, const Address&> m_txTrace;

    /**
     * @brief Trace fired when a packet is received.
     *
     * Provides the packet and source address. Combined with Tx trace,
     * enables latency measurement for request-response patterns.
     */
    TracedCallback<Ptr<const Packet>, const Address&> m_rxTrace;

    /**
     * @brief Trace fired when a packet send fails.
     *
     * Provides the packet and intended destination. Reasons include:
     * no connection available, socket error, or pool exhausted.
     */
    TracedCallback<Ptr<const Packet>, const Address&> m_txDropTrace;
};

} // namespace ns3

#endif // CONNECTION_MANAGER_H
