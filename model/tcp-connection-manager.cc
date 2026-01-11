/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "tcp-connection-manager.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpConnectionManager");

NS_OBJECT_ENSURE_REGISTERED(TcpConnectionManager);

TypeId
TcpConnectionManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::distributed::TcpConnectionManager")
                            .SetParent<ConnectionManager>()
                            .SetGroupName("Distributed")
                            .AddConstructor<TcpConnectionManager>()
                            .AddAttribute("PoolSize",
                                          "Number of TCP connections in the pool (client mode). "
                                          "Default 1 = single connection.",
                                          UintegerValue(1),
                                          MakeUintegerAccessor(&TcpConnectionManager::m_poolSize),
                                          MakeUintegerChecker<uint32_t>(1));
    return tid;
}

TcpConnectionManager::TcpConnectionManager()
    : m_node(nullptr),
      m_poolSize(1),
      m_isServer(false),
      m_listenSocket(nullptr),
      m_nextConnectionId(1)
{
    NS_LOG_FUNCTION(this);
}

TcpConnectionManager::~TcpConnectionManager()
{
    NS_LOG_FUNCTION(this);
}

void
TcpConnectionManager::DoDispose()
{
    NS_LOG_FUNCTION(this);

    // Close listening socket
    if (m_listenSocket)
    {
        m_listenSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                          MakeNullCallback<void, Ptr<Socket>, const Address&>());
        m_listenSocket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                          MakeNullCallback<void, Ptr<Socket>>());
        m_listenSocket->Close();
        m_listenSocket = nullptr;
    }

    // Close all connection sockets
    for (auto& socket : m_sockets)
    {
        if (socket)
        {
            socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
            socket->SetConnectCallback(MakeNullCallback<void, Ptr<Socket>>(),
                                       MakeNullCallback<void, Ptr<Socket>>());
            socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                      MakeNullCallback<void, Ptr<Socket>>());
            socket->Close();
        }
    }
    m_sockets.clear();

    // Clear all maps
    m_socketBusy.clear();
    m_socketToPeer.clear();
    m_peerToSocket.clear();
    m_idToSocket.clear();
    m_socketToId.clear();

    // Clear callbacks
    m_receiveCallback = ReceiveCallback();
    m_connectionCallback = ConnectionCallback();
    m_closeCallback = ConnectionCallback();

    m_node = nullptr;

    ConnectionManager::DoDispose();
}

void
TcpConnectionManager::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

Ptr<Node>
TcpConnectionManager::GetNode() const
{
    return m_node;
}

void
TcpConnectionManager::Bind(uint16_t port)
{
    NS_LOG_FUNCTION(this << port);
    Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
}

void
TcpConnectionManager::Bind(const Address& local)
{
    NS_LOG_FUNCTION(this << local);

    if (!m_node)
    {
        NS_LOG_ERROR("Node not set. Call SetNode() before Bind().");
        return;
    }

    m_isServer = true;

    // Create listening socket
    m_listenSocket = Socket::CreateSocket(m_node, TcpSocketFactory::GetTypeId());

    if (m_listenSocket->Bind(local) == -1)
    {
        NS_LOG_ERROR("Failed to bind listening socket");
        return;
    }

    m_listenSocket->Listen();
    m_listenSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                      MakeCallback(&TcpConnectionManager::HandleAccept, this));
    m_listenSocket->SetCloseCallbacks(MakeCallback(&TcpConnectionManager::HandlePeerClose, this),
                                      MakeCallback(&TcpConnectionManager::HandlePeerError, this));

    NS_LOG_INFO("TCP server listening on " << local);
}

void
TcpConnectionManager::Connect(const Address& remote)
{
    NS_LOG_FUNCTION(this << remote);

    if (!m_node)
    {
        NS_LOG_ERROR("Node not set. Call SetNode() before Connect().");
        return;
    }

    if (m_isServer)
    {
        NS_LOG_ERROR("Cannot Connect() after Bind(). Already in server mode.");
        return;
    }

    m_serverAddress = remote;
    CreatePooledConnections();
}

void
TcpConnectionManager::CreatePooledConnections()
{
    NS_LOG_FUNCTION(this);

    for (uint32_t i = 0; i < m_poolSize; ++i)
    {
        Ptr<Socket> socket = Socket::CreateSocket(m_node, TcpSocketFactory::GetTypeId());

        // Bind to ephemeral port
        if (InetSocketAddress::IsMatchingType(m_serverAddress))
        {
            socket->Bind();
        }
        else if (Inet6SocketAddress::IsMatchingType(m_serverAddress))
        {
            socket->Bind6();
        }

        // Set up callbacks
        socket->SetConnectCallback(
            MakeCallback(&TcpConnectionManager::HandleConnectionSucceeded, this),
            MakeCallback(&TcpConnectionManager::HandleConnectionFailed, this));
        socket->SetRecvCallback(MakeCallback(&TcpConnectionManager::HandleRead, this));
        socket->SetCloseCallbacks(MakeCallback(&TcpConnectionManager::HandlePeerClose, this),
                                  MakeCallback(&TcpConnectionManager::HandlePeerError, this));

        // Connect to server
        socket->Connect(m_serverAddress);

        // Add to pool
        m_sockets.push_back(socket);
        m_socketBusy[socket] = false;
        m_socketToPeer[socket] = m_serverAddress;
        // Note: m_peerToSocket maps to the first socket for this peer (for Close(peer) support)
        if (m_peerToSocket.find(m_serverAddress) == m_peerToSocket.end())
        {
            m_peerToSocket[m_serverAddress] = socket;
        }

        NS_LOG_DEBUG("Created connection " << (i + 1) << "/" << m_poolSize << " to "
                                           << m_serverAddress);
    }
}

void
TcpConnectionManager::HandleConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_INFO("Connection established to " << m_serverAddress);

    if (!m_connectionCallback.IsNull())
    {
        m_connectionCallback(m_serverAddress);
    }
}

void
TcpConnectionManager::HandleConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_ERROR("Connection failed to " << m_serverAddress);

    CleanupSocket(socket);
}

void
TcpConnectionManager::HandleAccept(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);

    Address peerAddr;
    if (socket->GetPeerName(peerAddr) != 0)
    {
        // Fallback to the address from the callback if GetPeerName fails
        peerAddr = from;
    }

    NS_LOG_INFO("Accepted connection from " << peerAddr);

    // Set up callbacks for the accepted socket
    socket->SetRecvCallback(MakeCallback(&TcpConnectionManager::HandleRead, this));
    socket->SetCloseCallbacks(MakeCallback(&TcpConnectionManager::HandlePeerClose, this),
                              MakeCallback(&TcpConnectionManager::HandlePeerError, this));

    // Track the connection
    m_sockets.push_back(socket);
    m_socketBusy[socket] = false;
    m_socketToPeer[socket] = peerAddr;
    m_peerToSocket[peerAddr] = socket;

    if (!m_connectionCallback.IsNull())
    {
        m_connectionCallback(peerAddr);
    }
}

void
TcpConnectionManager::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Ptr<Packet> packet;
    Address from;

    while ((packet = socket->RecvFrom(from)))
    {
        if (packet->GetSize() == 0)
        {
            break;
        }

        // Use the stored peer address for consistency with our address maps
        // This ensures the address in the callback matches what's in m_peerToSocket
        Address peerAddr = GetPeerAddress(socket);
        if (peerAddr.IsInvalid())
        {
            peerAddr = from;
        }

        NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from " << peerAddr);

        m_rxTrace(packet, peerAddr);

        if (!m_receiveCallback.IsNull())
        {
            m_receiveCallback(packet, peerAddr);
        }
    }
}

void
TcpConnectionManager::HandlePeerClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address peer = GetPeerAddress(socket);
    NS_LOG_INFO("Peer " << peer << " closed connection");

    if (!m_closeCallback.IsNull() && !peer.IsInvalid())
    {
        m_closeCallback(peer);
    }

    CleanupSocket(socket);
}

void
TcpConnectionManager::HandlePeerError(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address peer = GetPeerAddress(socket);
    NS_LOG_ERROR("Connection error with " << peer);

    if (!m_closeCallback.IsNull() && !peer.IsInvalid())
    {
        m_closeCallback(peer);
    }

    CleanupSocket(socket);
}

void
TcpConnectionManager::CleanupSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    if (!socket)
    {
        return;
    }

    // Clear callbacks before closing to prevent re-entrant calls
    socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    socket->SetConnectCallback(MakeNullCallback<void, Ptr<Socket>>(),
                               MakeNullCallback<void, Ptr<Socket>>());
    socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                              MakeNullCallback<void, Ptr<Socket>>());

    // Close the socket
    socket->Close();

    // Remove from peer mapping
    auto peerIt = m_socketToPeer.find(socket);
    if (peerIt != m_socketToPeer.end())
    {
        m_peerToSocket.erase(peerIt->second);
        m_socketToPeer.erase(peerIt);
    }

    // Remove from connection ID mapping
    auto idIt = m_socketToId.find(socket);
    if (idIt != m_socketToId.end())
    {
        m_idToSocket.erase(idIt->second);
        m_socketToId.erase(idIt);
    }

    // Remove from busy map and socket list
    m_socketBusy.erase(socket);
    m_sockets.remove(socket);
}

Address
TcpConnectionManager::GetPeerAddress(Ptr<Socket> socket) const
{
    auto it = m_socketToPeer.find(socket);
    if (it != m_socketToPeer.end())
    {
        return it->second;
    }
    return Address();
}

void
TcpConnectionManager::Send(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    Ptr<Socket> socket = GetIdleSocket();
    if (!socket)
    {
        NS_LOG_ERROR("No idle connection available for Send()");
        m_txDropTrace(packet, m_serverAddress);
        return;
    }

    Address peerAddr = GetPeerAddress(socket);
    int sent = socket->Send(packet);
    if (sent > 0)
    {
        NS_LOG_DEBUG("Sent " << sent << " bytes");
        m_txTrace(packet, peerAddr);
    }
    else
    {
        NS_LOG_ERROR("Failed to send packet");
        m_txDropTrace(packet, peerAddr);
    }
}

void
TcpConnectionManager::Send(Ptr<Packet> packet, const Address& to)
{
    NS_LOG_FUNCTION(this << packet << to);

    Ptr<Socket> socket = nullptr;

    if (m_isServer)
    {
        // Server mode: find the socket for this peer
        auto it = m_peerToSocket.find(to);
        if (it != m_peerToSocket.end())
        {
            socket = it->second;
        }
        else
        {
            NS_LOG_ERROR("No connection to peer " << to);
            m_txDropTrace(packet, to);
            return;
        }
    }
    else
    {
        // Client mode: use any idle connection to the server
        socket = GetIdleSocket();
        if (!socket)
        {
            NS_LOG_ERROR("No idle connection available");
            m_txDropTrace(packet, to);
            return;
        }
    }

    int sent = socket->Send(packet);
    if (sent > 0)
    {
        NS_LOG_DEBUG("Sent " << sent << " bytes to " << to);
        m_txTrace(packet, to);
    }
    else
    {
        NS_LOG_ERROR("Failed to send packet to " << to);
        m_txDropTrace(packet, to);
    }
}

Ptr<Socket>
TcpConnectionManager::GetIdleSocket()
{
    for (auto& socket : m_sockets)
    {
        auto it = m_socketBusy.find(socket);
        if (it != m_socketBusy.end() && !it->second)
        {
            return socket;
        }
    }
    return nullptr;
}

void
TcpConnectionManager::SetReceiveCallback(ReceiveCallback callback)
{
    NS_LOG_FUNCTION(this);
    m_receiveCallback = callback;
}

void
TcpConnectionManager::SetConnectionCallback(ConnectionCallback callback)
{
    NS_LOG_FUNCTION(this);
    m_connectionCallback = callback;
}

void
TcpConnectionManager::SetCloseCallback(ConnectionCallback callback)
{
    NS_LOG_FUNCTION(this);
    m_closeCallback = callback;
}

void
TcpConnectionManager::Close()
{
    NS_LOG_FUNCTION(this);

    // Close listening socket
    if (m_listenSocket)
    {
        m_listenSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                          MakeNullCallback<void, Ptr<Socket>, const Address&>());
        m_listenSocket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                          MakeNullCallback<void, Ptr<Socket>>());
        m_listenSocket->Close();
        m_listenSocket = nullptr;
    }

    // Close all connection sockets
    for (auto& socket : m_sockets)
    {
        if (socket)
        {
            socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
            socket->SetConnectCallback(MakeNullCallback<void, Ptr<Socket>>(),
                                       MakeNullCallback<void, Ptr<Socket>>());
            socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                      MakeNullCallback<void, Ptr<Socket>>());
            socket->Close();
        }
    }

    m_sockets.clear();
    m_socketBusy.clear();
    m_socketToPeer.clear();
    m_peerToSocket.clear();
    m_idToSocket.clear();
    m_socketToId.clear();
}

void
TcpConnectionManager::Close(const Address& peer)
{
    NS_LOG_FUNCTION(this << peer);

    auto it = m_peerToSocket.find(peer);
    if (it != m_peerToSocket.end())
    {
        CleanupSocket(it->second);
    }
    else
    {
        NS_LOG_WARN("No connection to peer " << peer);
    }
}

std::string
TcpConnectionManager::GetName() const
{
    return "TCP";
}

bool
TcpConnectionManager::IsReliable() const
{
    return true;
}

TcpConnectionManager::ConnectionId
TcpConnectionManager::GenerateConnectionId()
{
    return m_nextConnectionId++;
}

TcpConnectionManager::ConnectionId
TcpConnectionManager::AcquireConnection()
{
    NS_LOG_FUNCTION(this);

    Ptr<Socket> socket = GetIdleSocket();
    if (!socket)
    {
        NS_LOG_WARN("No idle connection available to acquire");
        return INVALID_CONNECTION;
    }

    // Mark as busy
    m_socketBusy[socket] = true;

    // Generate and track connection ID
    ConnectionId connId = GenerateConnectionId();
    m_idToSocket[connId] = socket;
    m_socketToId[socket] = connId;

    NS_LOG_DEBUG("Acquired connection " << connId);
    return connId;
}

TcpConnectionManager::ConnectionId
TcpConnectionManager::AcquireConnection(const Address& peer)
{
    NS_LOG_FUNCTION(this << peer);

    auto it = m_peerToSocket.find(peer);
    if (it == m_peerToSocket.end())
    {
        NS_LOG_WARN("No connection to peer " << peer);
        return INVALID_CONNECTION;
    }

    Ptr<Socket> socket = it->second;

    // Check if already acquired
    if (m_socketBusy[socket])
    {
        NS_LOG_WARN("Connection to " << peer << " is already acquired");
        return INVALID_CONNECTION;
    }

    // Mark as busy
    m_socketBusy[socket] = true;

    // Generate and track connection ID
    ConnectionId connId = GenerateConnectionId();
    m_idToSocket[connId] = socket;
    m_socketToId[socket] = connId;

    NS_LOG_DEBUG("Acquired connection " << connId << " to " << peer);
    return connId;
}

void
TcpConnectionManager::Send(ConnectionId connId, Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << connId << packet);

    auto it = m_idToSocket.find(connId);
    if (it == m_idToSocket.end())
    {
        NS_LOG_ERROR("Invalid connection ID " << connId);
        m_txDropTrace(packet, Address());
        return;
    }

    Ptr<Socket> socket = it->second;
    Address peerAddr = GetPeerAddress(socket);
    int sent = socket->Send(packet);
    if (sent > 0)
    {
        NS_LOG_DEBUG("Sent " << sent << " bytes on connection " << connId);
        m_txTrace(packet, peerAddr);
    }
    else
    {
        NS_LOG_ERROR("Failed to send on connection " << connId);
        m_txDropTrace(packet, peerAddr);
    }
}

void
TcpConnectionManager::ReleaseConnection(ConnectionId connId)
{
    NS_LOG_FUNCTION(this << connId);

    auto it = m_idToSocket.find(connId);
    if (it == m_idToSocket.end())
    {
        NS_LOG_WARN("Invalid connection ID " << connId);
        return;
    }

    Ptr<Socket> socket = it->second;

    // Mark as not busy
    m_socketBusy[socket] = false;

    // Remove connection ID mapping
    m_socketToId.erase(socket);
    m_idToSocket.erase(it);

    NS_LOG_DEBUG("Released connection " << connId);
}

uint32_t
TcpConnectionManager::GetConnectionCount() const
{
    return static_cast<uint32_t>(m_sockets.size());
}

uint32_t
TcpConnectionManager::GetIdleConnectionCount() const
{
    uint32_t count = 0;
    for (const auto& pair : m_socketBusy)
    {
        if (!pair.second)
        {
            count++;
        }
    }
    return count;
}

uint32_t
TcpConnectionManager::GetActiveConnectionCount() const
{
    uint32_t count = 0;
    for (const auto& pair : m_socketBusy)
    {
        if (pair.second)
        {
            count++;
        }
    }
    return count;
}

} // namespace ns3
