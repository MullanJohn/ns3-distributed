/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-server.h"

#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OffloadServer");

NS_OBJECT_ENSURE_REGISTERED(OffloadServer);

TypeId
OffloadServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OffloadServer")
            .SetParent<Application>()
            .SetGroupName("Distributed")
            .AddConstructor<OffloadServer>()
            .AddAttribute("Local",
                          "The local address to bind to",
                          AddressValue(),
                          MakeAddressAccessor(&OffloadServer::m_local),
                          MakeAddressChecker())
            .AddAttribute("Port",
                          "Port on which to listen for incoming connections",
                          UintegerValue(9000),
                          MakeUintegerAccessor(&OffloadServer::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddTraceSource("TaskReceived",
                            "A task request has been received",
                            MakeTraceSourceAccessor(&OffloadServer::m_taskReceivedTrace),
                            "ns3::OffloadServer::TaskReceivedTracedCallback")
            .AddTraceSource("TaskCompleted",
                            "A task has been completed and response sent",
                            MakeTraceSourceAccessor(&OffloadServer::m_taskCompletedTrace),
                            "ns3::OffloadServer::TaskCompletedTracedCallback");
    return tid;
}

OffloadServer::OffloadServer()
    : m_port(9000),
      m_socket(nullptr),
      m_socket6(nullptr),
      m_accelerator(nullptr),
      m_tasksReceived(0),
      m_tasksCompleted(0),
      m_totalRx(0)
{
    NS_LOG_FUNCTION(this);
}

OffloadServer::~OffloadServer()
{
    NS_LOG_FUNCTION(this);
}

void
OffloadServer::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_socket = nullptr;
    m_socket6 = nullptr;
    m_socketList.clear();
    m_rxBuffer.clear();
    m_pendingTasks.clear();
    m_accelerator = nullptr;
    Application::DoDispose();
}

uint64_t
OffloadServer::GetTasksReceived() const
{
    return m_tasksReceived;
}

uint64_t
OffloadServer::GetTasksCompleted() const
{
    return m_tasksCompleted;
}

uint64_t
OffloadServer::GetTotalRx() const
{
    return m_totalRx;
}

uint16_t
OffloadServer::GetPort() const
{
    return m_port;
}

Address
OffloadServer::GetLocalAddress() const
{
    return m_local;
}

void
OffloadServer::StartApplication()
{
    NS_LOG_FUNCTION(this);

    // Get the GpuAccelerator aggregated to this node
    m_accelerator = GetNode()->GetObject<GpuAccelerator>();
    if (!m_accelerator)
    {
        NS_LOG_WARN("No GpuAccelerator aggregated to this node. Tasks will be rejected.");
    }
    else
    {
        // Subscribe to task completion events
        m_accelerator->TraceConnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&OffloadServer::OnTaskCompleted, this));
    }

    // Create the listening socket
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());

        Address local = m_local;
        if (local.IsInvalid())
        {
            local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            NS_LOG_INFO("Binding to port " << m_port);
        }
        else if (InetSocketAddress::IsMatchingType(local))
        {
            m_port = InetSocketAddress::ConvertFrom(local).GetPort();
            NS_LOG_INFO("Binding to " << InetSocketAddress::ConvertFrom(local).GetIpv4()
                                      << ":" << m_port);
        }
        else if (Inet6SocketAddress::IsMatchingType(local))
        {
            m_port = Inet6SocketAddress::ConvertFrom(local).GetPort();
            NS_LOG_INFO("Binding to " << Inet6SocketAddress::ConvertFrom(local).GetIpv6()
                                      << ":" << m_port);
        }

        if (m_socket->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }

        m_socket->Listen();
        // NOTE: Unlike PacketSink, we do NOT call ShutdownSend() here because
        // OffloadServer needs to send TASK_RESPONSE messages back to clients
        // when tasks complete on the GPU.
        m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                    MakeCallback(&OffloadServer::HandleAccept, this));
        m_socket->SetCloseCallbacks(MakeCallback(&OffloadServer::HandlePeerClose, this),
                                    MakeCallback(&OffloadServer::HandlePeerError, this));
    }

    // Create IPv6 socket if only port was specified (for dual-stack support)
    if (!m_socket6 && m_local.IsInvalid())
    {
        m_socket6 = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
        Address local6 = Inet6SocketAddress(Ipv6Address::GetAny(), m_port);

        if (m_socket6->Bind(local6) == -1)
        {
            NS_LOG_WARN("Failed to bind IPv6 socket");
            m_socket6 = nullptr;
        }
        else
        {
            m_socket6->Listen();
            // NOTE: No ShutdownSend() - we need to send responses on IPv6 too
            m_socket6->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                         MakeCallback(&OffloadServer::HandleAccept, this));
            m_socket6->SetCloseCallbacks(MakeCallback(&OffloadServer::HandlePeerClose, this),
                                         MakeCallback(&OffloadServer::HandlePeerError, this));
        }
    }
}

void
OffloadServer::StopApplication()
{
    NS_LOG_FUNCTION(this);

    // Disconnect from accelerator
    if (m_accelerator)
    {
        m_accelerator->TraceDisconnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&OffloadServer::OnTaskCompleted, this));
    }

    // Close listening sockets - clear callbacks before close
    if (m_socket)
    {
        m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                    MakeNullCallback<void, Ptr<Socket>, const Address&>());
        m_socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                    MakeNullCallback<void, Ptr<Socket>>());
        m_socket->Close();
        m_socket = nullptr;
    }
    if (m_socket6)
    {
        m_socket6->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                     MakeNullCallback<void, Ptr<Socket>, const Address&>());
        m_socket6->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                     MakeNullCallback<void, Ptr<Socket>>());
        m_socket6->Close();
        m_socket6 = nullptr;
    }

    // Close all accepted sockets - clear callbacks before close
    for (auto& socket : m_socketList)
    {
        socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket>>(),
                                  MakeNullCallback<void, Ptr<Socket>>());
        socket->Close();
    }
    m_socketList.clear();
}

void
OffloadServer::HandleAccept(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);
    NS_LOG_INFO("Accepted connection from " << from);

    socket->SetRecvCallback(MakeCallback(&OffloadServer::HandleRead, this));
    socket->SetCloseCallbacks(MakeCallback(&OffloadServer::HandlePeerClose, this),
                              MakeCallback(&OffloadServer::HandlePeerError, this));
    m_socketList.push_back(socket);
}

void
OffloadServer::HandleRead(Ptr<Socket> socket)
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

        m_totalRx += packet->GetSize();
        NS_LOG_DEBUG("Received " << packet->GetSize() << " bytes from " << from);

        // Add to buffer for this client (TCP stream reassembly)
        auto it = m_rxBuffer.find(from);
        if (it == m_rxBuffer.end())
        {
            m_rxBuffer[from] = packet;
        }
        else
        {
            it->second->AddAtEnd(packet);
        }

        // Try to process complete messages from the buffer
        ProcessBuffer(socket, from);
    }
}

void
OffloadServer::ProcessBuffer(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_FUNCTION(this << socket << from);

    auto it = m_rxBuffer.find(from);
    if (it == m_rxBuffer.end())
    {
        return;
    }

    Ptr<Packet> buffer = it->second;

    // Header size is constant (33 bytes)
    static const uint32_t headerSize = OffloadHeader::SERIALIZED_SIZE;

    // Process all complete messages in the buffer
    while (buffer->GetSize() > 0)
    {
        // Check if we have enough for the header
        OffloadHeader header;

        if (buffer->GetSize() < headerSize)
        {
            NS_LOG_DEBUG("Buffer has " << buffer->GetSize()
                                       << " bytes, need " << headerSize << " for header");
            break;
        }

        // Peek at the header to get input size
        buffer->PeekHeader(header);

        // Calculate total message size (header + payload)
        // Client sends: header + max(0, inputSize - headerSize) payload bytes
        uint64_t inputSize = header.GetInputSize();
        uint64_t payloadSize = (inputSize > headerSize) ? (inputSize - headerSize) : 0;
        uint64_t totalMessageSize = headerSize + payloadSize;

        // Ensure we have the complete message
        if (totalMessageSize > buffer->GetSize())
        {
            NS_LOG_DEBUG("Buffer has " << buffer->GetSize()
                                       << " bytes, need " << totalMessageSize << " for full message");
            break;
        }

        // Remove the header from the buffer
        buffer->RemoveHeader(header);

        // Remove the payload (we don't need the actual data, just consume it)
        if (payloadSize > 0)
        {
            buffer->RemoveAtStart(payloadSize);
        }

        // Process the task
        ProcessTask(header, socket);
    }

    // Update or remove the buffer entry
    if (buffer->GetSize() == 0)
    {
        m_rxBuffer.erase(it);
    }
}

void
OffloadServer::ProcessTask(const OffloadHeader& header, Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << header.GetTaskId() << socket);

    if (header.GetMessageType() != OffloadHeader::TASK_REQUEST)
    {
        NS_LOG_WARN("Received non-request message type, ignoring");
        return;
    }

    m_tasksReceived++;
    m_taskReceivedTrace(header);

    NS_LOG_INFO("Received task " << header.GetTaskId()
                                 << " (compute=" << header.GetComputeDemand()
                                 << ", input=" << header.GetInputSize()
                                 << ", output=" << header.GetOutputSize() << ")");

    if (!m_accelerator)
    {
        NS_LOG_ERROR("No accelerator available, dropping task " << header.GetTaskId());
        return;
    }

    // Create a Task object from the header
    Ptr<Task> task = CreateObject<Task>();
    task->SetTaskId(header.GetTaskId());
    task->SetComputeDemand(header.GetComputeDemand());
    task->SetInputSize(header.GetInputSize());
    task->SetOutputSize(header.GetOutputSize());
    task->SetArrivalTime(Simulator::Now());

    // Track the pending task for response routing
    PendingTask pending;
    pending.socket = socket;
    pending.task = task;
    m_pendingTasks[header.GetTaskId()] = pending;

    // Submit to the accelerator
    m_accelerator->SubmitTask(task);

    NS_LOG_DEBUG("Submitted task " << header.GetTaskId() << " to accelerator");
}

void
OffloadServer::OnTaskCompleted(Ptr<const Task> task, Time duration)
{
    NS_LOG_FUNCTION(this << task->GetTaskId() << duration);

    auto it = m_pendingTasks.find(task->GetTaskId());
    if (it == m_pendingTasks.end())
    {
        // Task not found in our pending map - it may have been submitted by
        // another source (e.g., TaskGenerator) directly to the accelerator
        NS_LOG_DEBUG("Task " << task->GetTaskId() << " not found in pending tasks (not ours)");
        return;
    }

    Ptr<Socket> socket = it->second.socket;
    m_pendingTasks.erase(it);

    // Check if socket is still valid
    if (socket)
    {
        SendResponse(socket, task, duration);
    }
    else
    {
        NS_LOG_WARN("Client socket closed before task " << task->GetTaskId() << " completed");
    }
}

void
OffloadServer::SendResponse(Ptr<Socket> socket, Ptr<const Task> task, Time duration)
{
    NS_LOG_FUNCTION(this << socket << task->GetTaskId() << duration);

    // Create response header
    OffloadHeader response;
    response.SetMessageType(OffloadHeader::TASK_RESPONSE);
    response.SetTaskId(task->GetTaskId());
    response.SetComputeDemand(task->GetComputeDemand());
    response.SetInputSize(task->GetInputSize());
    response.SetOutputSize(task->GetOutputSize());

    // Create packet with output payload to simulate result data transfer
    uint64_t outputSize = task->GetOutputSize();
    Ptr<Packet> packet = Create<Packet>(outputSize);
    packet->AddHeader(response);

    // Send response
    int sent = socket->Send(packet);
    if (sent > 0)
    {
        m_tasksCompleted++;
        m_taskCompletedTrace(response, duration);

        NS_LOG_INFO("Sent response for task " << task->GetTaskId()
                                              << " (output=" << outputSize << " bytes"
                                              << ", duration=" << duration.GetMilliSeconds() << "ms)");
    }
    else
    {
        NS_LOG_ERROR("Failed to send response for task " << task->GetTaskId());
    }
}

void
OffloadServer::HandlePeerClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_INFO("Client disconnected from port " << m_port);
    CleanupSocket(socket);
}

void
OffloadServer::HandlePeerError(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_ERROR("Socket error on connection");
    CleanupSocket(socket);
}

void
OffloadServer::CleanupSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    // Remove from socket list
    m_socketList.remove(socket);

    // Clear pending tasks for this socket
    for (auto it = m_pendingTasks.begin(); it != m_pendingTasks.end();)
    {
        if (it->second.socket == socket)
        {
            NS_LOG_DEBUG("Removing pending task " << it->first << " for closed socket");
            it = m_pendingTasks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Note: We don't clear the rx buffer here as it's keyed by Address, not Socket.
    // Stale buffer entries will be overwritten if the same client reconnects.
}

} // namespace ns3
