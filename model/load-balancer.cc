/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "load-balancer.h"

#include "round-robin-scheduler.h"
#include "tcp-connection-manager.h"

#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
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
            .AddAttribute("FrontendConnectionManager",
                          "ConnectionManager for client connections (optional, defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&LoadBalancer::m_frontendConnMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddAttribute("BackendConnectionManager",
                          "ConnectionManager for backend connections (optional, defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&LoadBalancer::m_backendConnMgr),
                          MakePointerChecker<ConnectionManager>())
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
      m_frontendConnMgr(nullptr),
      m_backendConnMgr(nullptr),
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

    // Clear buffers
    m_clientBuffers.clear();
    m_backendBuffers.clear();
    m_pendingResponses.clear();

    // Clean up scheduler
    m_scheduler = nullptr;

    // Clean up ConnectionManagers
    if (m_frontendConnMgr)
    {
        m_frontendConnMgr->Close();
        m_frontendConnMgr = nullptr;
    }
    if (m_backendConnMgr)
    {
        m_backendConnMgr->Close();
        m_backendConnMgr = nullptr;
    }

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

    // Create default frontend ConnectionManager if not set
    if (!m_frontendConnMgr)
    {
        m_frontendConnMgr = CreateObject<TcpConnectionManager>();
    }
    m_frontendConnMgr->SetNode(GetNode());
    m_frontendConnMgr->SetReceiveCallback(
        MakeCallback(&LoadBalancer::HandleFrontendReceive, this));
    m_frontendConnMgr->Bind(m_port);

    // Create default backend ConnectionManager if not set
    if (!m_backendConnMgr)
    {
        m_backendConnMgr = CreateObject<TcpConnectionManager>();
    }
    m_backendConnMgr->SetNode(GetNode());
    m_backendConnMgr->SetReceiveCallback(
        MakeCallback(&LoadBalancer::HandleBackendReceive, this));

    // Connect to all backends
    for (uint32_t i = 0; i < m_cluster.GetN(); i++)
    {
        const Cluster::Backend& backend = m_cluster.Get(i);
        m_backendConnMgr->Connect(backend.address);
        NS_LOG_DEBUG("Connecting to backend " << i << " at " << backend.address);
    }

    NS_LOG_INFO("LoadBalancer listening on port " << m_port << " with " << m_cluster.GetN()
                                                  << " backends");
}

void
LoadBalancer::StopApplication()
{
    NS_LOG_FUNCTION(this);

    // Close ConnectionManagers
    if (m_frontendConnMgr)
    {
        m_frontendConnMgr->Close();
    }
    if (m_backendConnMgr)
    {
        m_backendConnMgr->Close();
    }

    // Clear buffers
    m_clientBuffers.clear();
    m_backendBuffers.clear();
}

// ========== ConnectionManager callbacks ==========

void
LoadBalancer::HandleFrontendReceive(Ptr<Packet> packet, const Address& from)
{
    NS_LOG_FUNCTION(this << packet << from);

    m_clientRx += packet->GetSize();
    NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from client " << from);

    // Add to buffer for this client
    auto it = m_clientBuffers.find(from);
    if (it == m_clientBuffers.end())
    {
        m_clientBuffers[from] = packet->Copy();
    }
    else
    {
        it->second->AddAtEnd(packet);
    }

    Ptr<Packet> buffer = m_clientBuffers[from];
    static const uint32_t headerSize = SimpleTaskHeader::SERIALIZED_SIZE;

    // Process complete messages
    while (buffer->GetSize() >= headerSize)
    {
        SimpleTaskHeader header;
        buffer->PeekHeader(header);

        uint64_t payloadSize = header.GetRequestPayloadSize();
        uint64_t totalSize = headerSize + payloadSize;

        if (buffer->GetSize() < totalSize)
        {
            NS_LOG_DEBUG("Buffer has " << buffer->GetSize() << " bytes, need " << totalSize);
            break;
        }

        // Remove header from buffer
        buffer->RemoveHeader(header);

        // Extract payload
        Ptr<Packet> payload = Create<Packet>();
        if (payloadSize > 0)
        {
            payload = buffer->CreateFragment(0, payloadSize);
            buffer->RemoveAtStart(payloadSize);
        }

        // Forward the task if it's a request
        if (header.GetMessageType() == TaskHeader::TASK_REQUEST)
        {
            ForwardTask(header, payload, from);
        }
        else
        {
            NS_LOG_WARN("Received non-request message from client, ignoring");
        }
    }

    // Clean up empty buffer
    if (buffer->GetSize() == 0)
    {
        m_clientBuffers.erase(from);
    }
}

void
LoadBalancer::HandleBackendReceive(Ptr<Packet> packet, const Address& from)
{
    NS_LOG_FUNCTION(this << packet << from);

    m_backendRx += packet->GetSize();
    NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from backend " << from);

    // Add to buffer for this backend
    auto it = m_backendBuffers.find(from);
    if (it == m_backendBuffers.end())
    {
        m_backendBuffers[from] = packet->Copy();
    }
    else
    {
        it->second->AddAtEnd(packet);
    }

    Ptr<Packet> buffer = m_backendBuffers[from];
    static const uint32_t headerSize = SimpleTaskHeader::SERIALIZED_SIZE;

    // Process complete messages
    while (buffer->GetSize() >= headerSize)
    {
        SimpleTaskHeader header;
        buffer->PeekHeader(header);

        // For responses, server sends header + outputSize bytes
        uint64_t payloadSize = header.GetResponsePayloadSize();
        uint64_t totalSize = headerSize + payloadSize;

        if (buffer->GetSize() < totalSize)
        {
            NS_LOG_DEBUG("Backend buffer has " << buffer->GetSize() << " bytes, need " << totalSize);
            break;
        }

        // Remove header from buffer
        buffer->RemoveHeader(header);

        // Extract payload
        Ptr<Packet> payload = Create<Packet>();
        if (payloadSize > 0)
        {
            payload = buffer->CreateFragment(0, payloadSize);
            buffer->RemoveAtStart(payloadSize);
        }

        // Route the response if it's a response message
        if (header.GetMessageType() == TaskHeader::TASK_RESPONSE)
        {
            RouteResponse(header, payload, from);
        }
        else
        {
            NS_LOG_WARN("Received non-response message from backend, ignoring");
        }
    }

    // Clean up empty buffer
    if (buffer->GetSize() == 0)
    {
        m_backendBuffers.erase(from);
    }
}

// ========== Task forwarding and response routing ==========

void
LoadBalancer::ForwardTask(const SimpleTaskHeader& header,
                          Ptr<Packet> payload,
                          const Address& clientAddr)
{
    NS_LOG_FUNCTION(this << header.GetTaskId() << clientAddr);

    // Select a backend
    int32_t backendIndex = m_scheduler->SelectBackend(header, m_cluster);

    if (backendIndex < 0)
    {
        NS_LOG_ERROR("No backend available for task " << header.GetTaskId());
        return;
    }

    // Store pending response info
    PendingResponse pending;
    pending.clientAddr = clientAddr;
    pending.arrivalTime = Simulator::Now();
    pending.backendIndex = static_cast<uint32_t>(backendIndex);
    m_pendingResponses[header.GetTaskId()] = pending;

    // Reconstruct packet with header
    Ptr<Packet> forwardPacket = payload->Copy();
    forwardPacket->AddHeader(header);

    // Send via ConnectionManager to the selected backend
    const Address& backendAddr = m_cluster.Get(backendIndex).address;
    m_backendConnMgr->Send(forwardPacket, backendAddr);

    m_tasksForwarded++;
    m_scheduler->NotifyTaskSent(backendIndex, header);
    m_taskForwardedTrace(header, backendIndex);

    NS_LOG_INFO("Forwarded task " << header.GetTaskId() << " to backend " << backendIndex);
}

void
LoadBalancer::RouteResponse(const SimpleTaskHeader& header,
                            Ptr<Packet> payload,
                            const Address& backendAddr)
{
    NS_LOG_FUNCTION(this << header.GetTaskId() << backendAddr);

    auto it = m_pendingResponses.find(header.GetTaskId());
    if (it == m_pendingResponses.end())
    {
        NS_LOG_WARN("Received response for unknown task " << header.GetTaskId());
        return;
    }

    PendingResponse& pending = it->second;
    Time latency = Simulator::Now() - pending.arrivalTime;

    // Notify scheduler
    m_scheduler->NotifyTaskCompleted(pending.backendIndex, header.GetTaskId(), latency);

    // Reconstruct packet with header
    Ptr<Packet> responsePacket = payload->Copy();
    responsePacket->AddHeader(header);

    // Send response to client via ConnectionManager
    m_frontendConnMgr->Send(responsePacket, pending.clientAddr);

    m_responsesRouted++;
    m_responseRoutedTrace(header, latency);

    NS_LOG_INFO("Routed response for task " << header.GetTaskId() << " to client (latency="
                                            << latency.GetMilliSeconds() << "ms)");

    // Remove from pending
    m_pendingResponses.erase(it);
}

} // namespace ns3
