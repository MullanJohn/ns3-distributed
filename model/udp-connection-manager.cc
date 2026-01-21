/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "udp-connection-manager.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UdpConnectionManager");

NS_OBJECT_ENSURE_REGISTERED(UdpConnectionManager);

TypeId
UdpConnectionManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::distributed::UdpConnectionManager")
                            .SetParent<ConnectionManager>()
                            .SetGroupName("Distributed")
                            .AddConstructor<UdpConnectionManager>();
    return tid;
}

UdpConnectionManager::UdpConnectionManager()
    : m_node(nullptr),
      m_socket(nullptr),
      m_hasDefaultDestination(false)
{
    NS_LOG_FUNCTION(this);
}

UdpConnectionManager::~UdpConnectionManager()
{
    NS_LOG_FUNCTION(this);
}

void
UdpConnectionManager::DoDispose()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->Close();
        m_socket = nullptr;
    }

    m_receiveCallback = ReceiveCallback();
    m_node = nullptr;

    ConnectionManager::DoDispose();
}

void
UdpConnectionManager::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

Ptr<Node>
UdpConnectionManager::GetNode() const
{
    return m_node;
}

void
UdpConnectionManager::Bind(uint16_t port)
{
    NS_LOG_FUNCTION(this << port);
    Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
}

void
UdpConnectionManager::Bind(const Address& local)
{
    NS_LOG_FUNCTION(this << local);

    if (!m_node)
    {
        NS_LOG_ERROR("Node not set. Call SetNode() before Bind().");
        return;
    }

    if (m_socket)
    {
        NS_LOG_WARN("Socket already exists. Closing existing socket.");
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->Close();
    }

    m_socket = Socket::CreateSocket(m_node, UdpSocketFactory::GetTypeId());

    if (m_socket->Bind(local) == -1)
    {
        NS_LOG_ERROR("Failed to bind UDP socket");
        return;
    }

    m_socket->SetRecvCallback(MakeCallback(&UdpConnectionManager::HandleRead, this));

    NS_LOG_INFO("UDP socket bound to " << local);
}

void
UdpConnectionManager::Connect(const Address& remote)
{
    NS_LOG_FUNCTION(this << remote);

    if (!m_node)
    {
        NS_LOG_ERROR("Node not set. Call SetNode() before Connect().");
        return;
    }

    // Create socket if not already created
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(m_node, UdpSocketFactory::GetTypeId());

        // Bind to ephemeral port
        if (InetSocketAddress::IsMatchingType(remote))
        {
            m_socket->Bind();
        }
        else if (Inet6SocketAddress::IsMatchingType(remote))
        {
            m_socket->Bind6();
        }

        m_socket->SetRecvCallback(MakeCallback(&UdpConnectionManager::HandleRead, this));
    }

    // Set default destination (UDP "connect" just sets default destination)
    m_defaultDestination = remote;
    m_hasDefaultDestination = true;

    // Optionally call socket Connect for ICMP error handling
    m_socket->Connect(remote);

    NS_LOG_INFO("UDP default destination set to " << remote);
}

void
UdpConnectionManager::Send(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    if (!m_socket)
    {
        NS_LOG_ERROR("Socket not created. Call Connect() or Bind() first.");
        m_txDropTrace(packet, Address());
        return;
    }

    if (!m_hasDefaultDestination)
    {
        NS_LOG_ERROR("No default destination. Use Send(packet, address) or call Connect() first.");
        m_txDropTrace(packet, Address());
        return;
    }

    int sent = m_socket->Send(packet);
    if (sent > 0)
    {
        NS_LOG_DEBUG("Sent " << sent << " bytes to default destination");
        m_txTrace(packet, m_defaultDestination);
    }
    else
    {
        NS_LOG_ERROR("Failed to send packet");
        m_txDropTrace(packet, m_defaultDestination);
    }
}

void
UdpConnectionManager::Send(Ptr<Packet> packet, const Address& to)
{
    NS_LOG_FUNCTION(this << packet << to);

    if (!m_socket)
    {
        NS_LOG_ERROR("Socket not created. Call Connect() or Bind() first.");
        m_txDropTrace(packet, to);
        return;
    }

    int sent = m_socket->SendTo(packet, 0, to);
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

void
UdpConnectionManager::HandleRead(Ptr<Socket> socket)
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

        NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from " << from);

        m_rxTrace(packet, from);

        if (!m_receiveCallback.IsNull())
        {
            m_receiveCallback(packet, from);
        }
    }
}

void
UdpConnectionManager::SetReceiveCallback(ReceiveCallback callback)
{
    NS_LOG_FUNCTION(this);
    m_receiveCallback = callback;
}

void
UdpConnectionManager::Close()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->Close();
        m_socket = nullptr;
    }

    m_hasDefaultDestination = false;
}

void
UdpConnectionManager::Close(const Address& peer)
{
    NS_LOG_FUNCTION(this << peer);
    // UDP is connectionless - closing a specific peer is a no-op
    NS_LOG_DEBUG("Close(peer) is a no-op for UDP (connectionless)");
}

std::string
UdpConnectionManager::GetName() const
{
    return "UDP";
}

bool
UdpConnectionManager::IsReliable() const
{
    return false;
}

bool
UdpConnectionManager::IsConnected() const
{
    // Either way, having a socket means we can send/receive
    return m_socket != nullptr && m_hasDefaultDestination;
}

} // namespace ns3
