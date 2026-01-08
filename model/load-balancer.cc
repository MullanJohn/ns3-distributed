/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "load-balancer.h"

#include "round-robin-scheduler.h"

#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("LoadBalancer");

NS_OBJECT_ENSURE_REGISTERED(LoadBalancer);

TypeId
LoadBalancer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::LoadBalancer")
            .SetParent<Application>()
            .SetGroupName("Distributed")
            .AddConstructor<LoadBalancer>()
            .AddAttribute("Port",
                          "Port to listen on for client connections",
                          UintegerValue(8000),
                          MakeUintegerAccessor(&LoadBalancer::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Scheduler",
                          "The node scheduler to use for backend selection",
                          PointerValue(CreateObject<RoundRobinScheduler>()),
                          MakePointerAccessor(&LoadBalancer::m_scheduler),
                          MakePointerChecker<NodeScheduler>())
            .AddTraceSource("TaskForwarded",
                            "A task was forwarded to a backend",
                            MakeTraceSourceAccessor(&LoadBalancer::m_taskForwardedTrace),
                            "ns3::LoadBalancer::TaskForwardedCallback")
            .AddTraceSource("ResponseRouted",
                            "A response was routed to a client",
                            MakeTraceSourceAccessor(&LoadBalancer::m_responseRoutedTrace),
                            "ns3::LoadBalancer::ResponseRoutedCallback");
    return tid;
}

LoadBalancer::LoadBalancer()
    : m_port(8000),
      m_scheduler(nullptr),
      m_listenSocket(nullptr),
      m_tasksForwarded(0),
      m_responsesRouted(0),
      m_clientRx(0),
      m_backendRx(0)
{
    NS_LOG_FUNCTION(this);
}

LoadBalancer::~LoadBalancer()
{
    NS_LOG_FUNCTION(this);
}

void
LoadBalancer::DoDispose()
{
    NS_LOG_FUNCTION(this);

    // Close all client sockets with callback cleanup
    for (auto& socket : m_clientSockets)
    {
        socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                  MakeNullCallback<void, Ptr<Socket>>());
        socket->Close();
    }
    m_clientSockets.clear();

    // Close all backend sockets with callback cleanup
    for (auto& backend : m_backends)
    {
        if (backend.socket)
        {
            backend.socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
            backend.socket->SetConnectCallback(MakeNullCallback<void, Ptr<Socket>>(),
                                               MakeNullCallback<void, Ptr<Socket>>());
            backend.socket->Close();
        }
    }
    m_backends.clear();

    // Close listening socket with callback cleanup
    if (m_listenSocket)
    {
        m_listenSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                          MakeNullCallback<void, Ptr<Socket>, const Address&>());
        m_listenSocket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                          MakeNullCallback<void, Ptr<Socket>>());
        m_listenSocket->Close();
        m_listenSocket = nullptr;
    }
    m_clientRxBuffers.clear();
    m_socketToBackend.clear();
    m_pendingResponses.clear();
    m_scheduler = nullptr;
    Application::DoDispose();
}

void
LoadBalancer::SetCluster(const Cluster& cluster)
{
    NS_LOG_FUNCTION(this << cluster.GetN());
    m_cluster = cluster;
}

uint64_t
LoadBalancer::GetTasksForwarded() const
{
    return m_tasksForwarded;
}

uint64_t
LoadBalancer::GetResponsesRouted() const
{
    return m_responsesRouted;
}

uint64_t
LoadBalancer::GetClientRx() const
{
    return m_clientRx;
}

uint64_t
LoadBalancer::GetBackendRx() const
{
    return m_backendRx;
}

void
LoadBalancer::StartApplication()
{
    NS_LOG_FUNCTION(this);

    // Initialize scheduler with cluster
    if (m_scheduler)
    {
        m_scheduler->Initialize(m_cluster);
    }
    else
    {
        NS_LOG_WARN("No scheduler configured, creating default RoundRobinScheduler");
        m_scheduler = CreateObject<RoundRobinScheduler>();
        m_scheduler->Initialize(m_cluster);
    }

    // Warn if cluster is empty
    if (m_cluster.GetN() == 0)
    {
        NS_LOG_WARN("LoadBalancer started with empty cluster - no backends available");
    }

    // Connect to all backends first
    ConnectToBackends();

    // Create listening socket for clients
    if (!m_listenSocket)
    {
        m_listenSocket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);

        if (m_listenSocket->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind load balancer socket to port " << m_port);
        }

        m_listenSocket->Listen();
        m_listenSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                          MakeCallback(&LoadBalancer::HandleClientAccept, this));
        m_listenSocket->SetCloseCallbacks(MakeCallback(&LoadBalancer::HandleClientClose, this),
                                          MakeCallback(&LoadBalancer::HandleClientError, this));

        NS_LOG_INFO("LoadBalancer listening on port " << m_port << " with " << m_cluster.GetN()
                                                      << " backends");
    }
}

void
LoadBalancer::StopApplication()
{
    NS_LOG_FUNCTION(this);

    // Close listening socket - clear callbacks before close
    if (m_listenSocket)
    {
        m_listenSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                          MakeNullCallback<void, Ptr<Socket>, const Address&>());
        m_listenSocket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                          MakeNullCallback<void, Ptr<Socket>>());
        m_listenSocket->Close();
        m_listenSocket = nullptr;
    }

    // Close all client sockets - clear callbacks before close
    for (auto& socket : m_clientSockets)
    {
        socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                  MakeNullCallback<void, Ptr<Socket>>());
        socket->Close();
    }
    m_clientSockets.clear();

    // Close all backend sockets - clear callbacks before close
    for (auto& backend : m_backends)
    {
        if (backend.socket)
        {
            backend.socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
            backend.socket->SetConnectCallback(MakeNullCallback<void, Ptr<Socket>>(),
                                               MakeNullCallback<void, Ptr<Socket>>());
            backend.socket->Close();
        }
    }
    m_backends.clear();
}

// ========== Client-facing methods ==========

void
LoadBalancer::HandleClientAccept(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);
    NS_LOG_INFO("Accepted client connection from "
                << InetSocketAddress::ConvertFrom(from).GetIpv4());

    socket->SetRecvCallback(MakeCallback(&LoadBalancer::HandleClientRead, this));
    socket->SetCloseCallbacks(MakeCallback(&LoadBalancer::HandleClientClose, this),
                              MakeCallback(&LoadBalancer::HandleClientError, this));
    m_clientSockets.push_back(socket);
}

void
LoadBalancer::HandleClientRead(Ptr<Socket> socket)
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

        m_clientRx += packet->GetSize();
        NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from client");

        // Add to buffer for this client (keyed by socket)
        auto it = m_clientRxBuffers.find(socket);
        if (it == m_clientRxBuffers.end())
        {
            m_clientRxBuffers[socket] = packet;
        }
        else
        {
            it->second->AddAtEnd(packet);
        }

        // Process complete messages
        ProcessClientBuffer(socket, from);
    }
}

void
LoadBalancer::ProcessClientBuffer(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);

    auto it = m_clientRxBuffers.find(socket);
    if (it == m_clientRxBuffers.end())
    {
        return;
    }

    Ptr<Packet> buffer = it->second;
    static const uint32_t headerSize = OffloadHeader::SERIALIZED_SIZE;

    while (buffer->GetSize() > 0)
    {
        if (buffer->GetSize() < headerSize)
        {
            NS_LOG_DEBUG("Buffer has " << buffer->GetSize() << " bytes, need " << headerSize);
            break;
        }

        OffloadHeader header;
        buffer->PeekHeader(header);

        // Calculate total message size
        uint64_t payloadSize = header.GetRequestPayloadSize();
        uint64_t totalMessageSize = headerSize + payloadSize;

        if (buffer->GetSize() < totalMessageSize)
        {
            NS_LOG_DEBUG("Buffer has " << buffer->GetSize() << " bytes, need " << totalMessageSize);
            break;
        }

        // Remove header
        buffer->RemoveHeader(header);

        // Extract payload
        Ptr<Packet> payload = Create<Packet>();
        if (payloadSize > 0)
        {
            payload = buffer->CreateFragment(0, payloadSize);
            buffer->RemoveAtStart(payloadSize);
        }

        // Forward the task
        if (header.GetMessageType() == TaskHeader::TASK_REQUEST)
        {
            ForwardTask(header, payload, socket, from);
        }
        else
        {
            NS_LOG_WARN("Received non-request message from client, ignoring");
        }
    }

    if (buffer->GetSize() == 0)
    {
        m_clientRxBuffers.erase(it);
    }
}

void
LoadBalancer::HandleClientClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_INFO("Client disconnected");
    CleanupClientSocket(socket);
}

void
LoadBalancer::HandleClientError(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_ERROR("Client socket error");
    CleanupClientSocket(socket);
}

void
LoadBalancer::CleanupClientSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    // Clear callbacks before removing socket
    socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                              MakeNullCallback<void, Ptr<Socket>>());
    m_clientSockets.remove(socket);

    // Remove pending responses for this client
    for (auto it = m_pendingResponses.begin(); it != m_pendingResponses.end();)
    {
        if (it->second.clientSocket == socket)
        {
            NS_LOG_DEBUG("Removing pending task " << it->first << " for closed client");
            it = m_pendingResponses.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Clean up receive buffer for this client
    auto bufferIt = m_clientRxBuffers.find(socket);
    if (bufferIt != m_clientRxBuffers.end())
    {
        NS_LOG_DEBUG("Removing client rx buffer for disconnected client");
        m_clientRxBuffers.erase(bufferIt);
    }
}

// ========== Backend-facing methods ==========

void
LoadBalancer::ConnectToBackends()
{
    NS_LOG_FUNCTION(this);

    m_backends.resize(m_cluster.GetN());

    for (uint32_t i = 0; i < m_cluster.GetN(); i++)
    {
        const Cluster::Backend& clusterBackend = m_cluster.Get(i);

        Ptr<Socket> socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());

        // Bind locally
        if (socket->Bind() == -1)
        {
            NS_LOG_ERROR("Failed to bind backend socket " << i);
            continue;
        }

        socket->SetConnectCallback(MakeCallback(&LoadBalancer::HandleBackendConnected, this),
                                   MakeCallback(&LoadBalancer::HandleBackendFailed, this));

        socket->Connect(clusterBackend.address);

        m_backends[i].socket = socket;
        m_backends[i].connected = false;
        m_backends[i].rxBuffer = Create<Packet>();
        m_socketToBackend[socket] = i;

        NS_LOG_DEBUG("Connecting to backend " << i);
    }
}

void
LoadBalancer::HandleBackendConnected(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    auto it = m_socketToBackend.find(socket);
    if (it == m_socketToBackend.end())
    {
        NS_LOG_ERROR("Unknown socket in HandleBackendConnected");
        return;
    }
    uint32_t backendIndex = it->second;

    NS_LOG_INFO("Connected to backend " << backendIndex);
    m_backends[backendIndex].connected = true;

    // Set up receive callback
    socket->SetRecvCallback(MakeCallback(&LoadBalancer::HandleBackendRead, this));
}

void
LoadBalancer::HandleBackendFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    auto it = m_socketToBackend.find(socket);
    if (it == m_socketToBackend.end())
    {
        NS_LOG_ERROR("Unknown socket in HandleBackendFailed");
        return;
    }
    uint32_t backendIndex = it->second;

    NS_LOG_ERROR("Failed to connect to backend " << backendIndex);
    m_backends[backendIndex].connected = false;
}

void
LoadBalancer::HandleBackendRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    auto it = m_socketToBackend.find(socket);
    if (it == m_socketToBackend.end())
    {
        NS_LOG_ERROR("Unknown socket in HandleBackendRead");
        return;
    }
    uint32_t backendIndex = it->second;

    Ptr<Packet> packet;
    Address from;

    while ((packet = socket->RecvFrom(from)))
    {
        if (packet->GetSize() == 0)
        {
            break;
        }

        m_backendRx += packet->GetSize();
        NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from backend " << backendIndex);

        // Add to buffer for this backend
        m_backends[backendIndex].rxBuffer->AddAtEnd(packet);

        // Process complete messages
        ProcessBackendBuffer(backendIndex);
    }
}

void
LoadBalancer::ProcessBackendBuffer(uint32_t backendIndex)
{
    NS_LOG_FUNCTION(this << backendIndex);

    Ptr<Packet> buffer = m_backends[backendIndex].rxBuffer;
    static const uint32_t headerSize = OffloadHeader::SERIALIZED_SIZE;

    while (buffer->GetSize() > 0)
    {
        if (buffer->GetSize() < headerSize)
        {
            break;
        }

        OffloadHeader header;
        buffer->PeekHeader(header);

        // For responses, server sends header + outputSize bytes
        uint64_t payloadSize = header.GetResponsePayloadSize();
        uint64_t totalMessageSize = headerSize + payloadSize;

        if (buffer->GetSize() < totalMessageSize)
        {
            NS_LOG_DEBUG("Backend buffer has " << buffer->GetSize() << " bytes, need "
                                               << totalMessageSize);
            break;
        }

        // Remove header
        buffer->RemoveHeader(header);

        // Extract payload
        Ptr<Packet> payload = Create<Packet>();
        if (payloadSize > 0)
        {
            payload = buffer->CreateFragment(0, payloadSize);
            buffer->RemoveAtStart(payloadSize);
        }

        // Route the response
        if (header.GetMessageType() == TaskHeader::TASK_RESPONSE)
        {
            RouteResponse(header, payload, backendIndex);
        }
        else
        {
            NS_LOG_WARN("Received non-response message from backend " << backendIndex);
        }
    }
}

// ========== Task forwarding and response routing ==========

void
LoadBalancer::ForwardTask(const OffloadHeader& header,
                          Ptr<Packet> payload,
                          Ptr<Socket> clientSocket,
                          const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << header.GetTaskId() << clientSocket);

    // Select a backend
    int32_t backendIndex = m_scheduler->SelectBackend(header, m_cluster);

    if (backendIndex < 0)
    {
        NS_LOG_ERROR("No backend available for task " << header.GetTaskId());
        return;
    }

    if (!m_backends[backendIndex].connected)
    {
        NS_LOG_ERROR("Backend " << backendIndex << " not connected, dropping task "
                                << header.GetTaskId());
        return;
    }

    // Store pending response info
    PendingResponse pending;
    pending.clientSocket = clientSocket;
    pending.clientAddr = clientAddr;
    pending.arrivalTime = Simulator::Now();
    pending.backendIndex = static_cast<uint32_t>(backendIndex);
    m_pendingResponses[header.GetTaskId()] = pending;

    // Reconstruct and send to backend
    Ptr<Packet> forwardPacket = payload->Copy();
    forwardPacket->AddHeader(header);

    int sent = m_backends[backendIndex].socket->Send(forwardPacket);
    if (sent > 0)
    {
        m_tasksForwarded++;
        m_scheduler->NotifyTaskSent(backendIndex, header);
        m_taskForwardedTrace(header, backendIndex);

        NS_LOG_INFO("Forwarded task " << header.GetTaskId() << " to backend " << backendIndex);
    }
    else
    {
        NS_LOG_ERROR("Failed to forward task " << header.GetTaskId() << " to backend "
                                               << backendIndex);
        m_pendingResponses.erase(header.GetTaskId());
    }
}

void
LoadBalancer::RouteResponse(const OffloadHeader& header, Ptr<Packet> payload, uint32_t backendIndex)
{
    NS_LOG_FUNCTION(this << header.GetTaskId() << backendIndex);

    auto it = m_pendingResponses.find(header.GetTaskId());
    if (it == m_pendingResponses.end())
    {
        NS_LOG_WARN("Received response for unknown task " << header.GetTaskId());
        return;
    }

    PendingResponse& pending = it->second;
    Ptr<Socket> clientSocket = pending.clientSocket;
    Time latency = Simulator::Now() - pending.arrivalTime;

    // Notify scheduler
    m_scheduler->NotifyTaskCompleted(pending.backendIndex, header.GetTaskId(), latency);

    // Remove from pending
    m_pendingResponses.erase(it);

    // Check if client socket is still valid
    if (!clientSocket)
    {
        NS_LOG_WARN("Client socket closed before response for task " << header.GetTaskId());
        return;
    }

    // Reconstruct and send to client
    Ptr<Packet> responsePacket = payload->Copy();
    responsePacket->AddHeader(header);

    int sent = clientSocket->Send(responsePacket);
    if (sent > 0)
    {
        m_responsesRouted++;
        m_responseRoutedTrace(header, latency);

        NS_LOG_INFO("Routed response for task " << header.GetTaskId() << " to client (latency="
                                                << latency.GetMilliSeconds() << "ms)");
    }
    else
    {
        NS_LOG_ERROR("Failed to route response for task " << header.GetTaskId());
    }
}

} // namespace ns3
