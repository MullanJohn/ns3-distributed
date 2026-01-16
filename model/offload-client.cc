/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-client.h"

#include "tcp-connection-manager.h"

#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OffloadClient");

NS_OBJECT_ENSURE_REGISTERED(OffloadClient);

// Static counter for assigning unique client IDs
uint32_t OffloadClient::s_nextClientId = 0;

TypeId
OffloadClient::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OffloadClient")
            .SetParent<Application>()
            .SetGroupName("Distributed")
            .AddConstructor<OffloadClient>()
            .AddAttribute("Remote",
                          "The address of the remote server",
                          AddressValue(),
                          MakeAddressAccessor(&OffloadClient::m_peer),
                          MakeAddressChecker())
            .AddAttribute("ConnectionManager",
                          "Connection manager for transport (defaults to TCP)",
                          PointerValue(),
                          MakePointerAccessor(&OffloadClient::m_connMgr),
                          MakePointerChecker<ConnectionManager>())
            .AddAttribute("MaxTasks",
                          "Maximum number of tasks to send (0 = unlimited)",
                          UintegerValue(0),
                          MakeUintegerAccessor(&OffloadClient::m_maxTasks),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("InterArrivalTime",
                          "Random variable for inter-arrival time between tasks",
                          PointerValue(CreateObject<ExponentialRandomVariable>()),
                          MakePointerAccessor(&OffloadClient::m_interArrivalTime),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("ComputeDemand",
                          "Random variable for task compute demand in FLOPS",
                          PointerValue(CreateObject<ExponentialRandomVariable>()),
                          MakePointerAccessor(&OffloadClient::m_computeDemand),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("InputSize",
                          "Random variable for task input size in bytes",
                          PointerValue(CreateObject<ExponentialRandomVariable>()),
                          MakePointerAccessor(&OffloadClient::m_inputSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("OutputSize",
                          "Random variable for task output size in bytes",
                          PointerValue(CreateObject<ExponentialRandomVariable>()),
                          MakePointerAccessor(&OffloadClient::m_outputSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddTraceSource("TaskSent",
                            "Trace fired when a task is sent",
                            MakeTraceSourceAccessor(&OffloadClient::m_taskSentTrace),
                            "ns3::OffloadClient::TaskSentTracedCallback")
            .AddTraceSource("ResponseReceived",
                            "Trace fired when a response is received",
                            MakeTraceSourceAccessor(&OffloadClient::m_responseReceivedTrace),
                            "ns3::OffloadClient::ResponseReceivedTracedCallback");
    return tid;
}

OffloadClient::OffloadClient()
    : m_connMgr(nullptr),
      m_maxTasks(0),
      m_clientId(s_nextClientId++),
      m_taskCount(0),
      m_totalTx(0),
      m_totalRx(0),
      m_rxBuffer(Create<Packet>()),
      m_responsesReceived(0)
{
    NS_LOG_FUNCTION(this);
}

OffloadClient::~OffloadClient()
{
    NS_LOG_FUNCTION(this);
}

void
OffloadClient::DoDispose()
{
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);

    // Clean up ConnectionManager
    if (m_connMgr)
    {
        m_connMgr->Close();
        m_connMgr = nullptr;
    }

    m_rxBuffer = nullptr;
    m_sendTimes.clear();
    m_interArrivalTime = nullptr;
    m_computeDemand = nullptr;
    m_inputSize = nullptr;
    m_outputSize = nullptr;

    Application::DoDispose();
}

void
OffloadClient::SetRemote(const Address& addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peer = addr;
}

Address
OffloadClient::GetRemote() const
{
    return m_peer;
}

uint64_t
OffloadClient::GetTasksSent() const
{
    return m_taskCount;
}

uint64_t
OffloadClient::GetTotalTx() const
{
    return m_totalTx;
}

uint64_t
OffloadClient::GetTotalRx() const
{
    return m_totalRx;
}

uint64_t
OffloadClient::GetResponsesReceived() const
{
    return m_responsesReceived;
}

void
OffloadClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(m_peer.IsInvalid(), "Remote address not set");

    // Create default ConnectionManager if not set
    if (!m_connMgr)
    {
        m_connMgr = CreateObject<TcpConnectionManager>();
    }

    // Configure ConnectionManager
    m_connMgr->SetNode(GetNode());
    m_connMgr->SetReceiveCallback(MakeCallback(&OffloadClient::HandleReceive, this));

    // Set TCP-specific callbacks
    Ptr<TcpConnectionManager> tcpConnMgr = DynamicCast<TcpConnectionManager>(m_connMgr);
    if (tcpConnMgr)
    {
        tcpConnMgr->SetConnectionCallback(MakeCallback(&OffloadClient::HandleConnected, this));
        tcpConnMgr->SetConnectionFailedCallback(
            MakeCallback(&OffloadClient::HandleConnectionFailed, this));
    }

    // Connect to server
    m_connMgr->Connect(m_peer);

    // For connectionless transports (UDP), start sending immediately
    if (!tcpConnMgr)
    {
        ScheduleNextTask();
    }
}

void
OffloadClient::StopApplication()
{
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);

    // Close ConnectionManager
    if (m_connMgr)
    {
        m_connMgr->Close();
    }
}

void
OffloadClient::HandleConnected(const Address& serverAddr)
{
    NS_LOG_FUNCTION(this << serverAddr);
    NS_LOG_INFO("Client " << m_clientId << " connected to " << serverAddr);

    // Start sending tasks
    ScheduleNextTask();
}

void
OffloadClient::HandleConnectionFailed(const Address& serverAddr)
{
    NS_LOG_FUNCTION(this << serverAddr);
    NS_LOG_ERROR("Client " << m_clientId << " failed to connect to " << serverAddr);
}

void
OffloadClient::HandleReceive(Ptr<Packet> packet, const Address& from)
{
    NS_LOG_FUNCTION(this << packet << from);

    if (packet->GetSize() == 0)
    {
        return;
    }

    m_totalRx += packet->GetSize();

    // Append to receive buffer
    m_rxBuffer->AddAtEnd(packet);

    // Process complete messages
    ProcessBuffer();
}

void
OffloadClient::SendTask()
{
    NS_LOG_FUNCTION(this);

    if (!m_connMgr || !m_connMgr->IsConnected())
    {
        NS_LOG_DEBUG("Not connected, cannot send task");
        return;
    }

    // Generate task parameters
    uint64_t inputSize = static_cast<uint64_t>(m_inputSize->GetValue());
    uint64_t outputSize = static_cast<uint64_t>(m_outputSize->GetValue());
    double computeDemand = m_computeDemand->GetValue();

    // Ensure minimum sizes
    if (inputSize < 1)
    {
        inputSize = 1;
    }
    if (outputSize < 1)
    {
        outputSize = 1;
    }
    if (computeDemand < 1.0)
    {
        computeDemand = 1.0;
    }

    // Create header with globally unique task ID
    // Task ID format: upper 32 bits = client ID, lower 32 bits = sequence number
    uint64_t taskId = (static_cast<uint64_t>(m_clientId) << 32) | m_taskCount;

    OffloadHeader header;
    header.SetMessageType(OffloadHeader::TASK_REQUEST);
    header.SetTaskId(taskId);
    header.SetComputeDemand(computeDemand);
    header.SetInputSize(inputSize);
    header.SetOutputSize(outputSize);

    // Create packet with payload matching input size
    uint32_t headerSize = header.GetSerializedSize();
    uint32_t payloadSize = (inputSize > headerSize) ? (inputSize - headerSize) : 0;
    Ptr<Packet> packet = Create<Packet>(payloadSize);
    packet->AddHeader(header);

    // Track send time for RTT calculation
    m_sendTimes[header.GetTaskId()] = Simulator::Now();

    // Send packet via ConnectionManager
    m_connMgr->Send(packet);

    uint32_t sent = packet->GetSize();
    m_taskCount++;
    m_totalTx += sent;

    NS_LOG_INFO("Sent task " << header.GetTaskId() << " with " << sent << " bytes"
                             << " (compute=" << computeDemand << " FLOPS"
                             << ", input=" << inputSize << " bytes"
                             << ", output=" << outputSize << " bytes)");

    m_taskSentTrace(header);

    // Schedule next task if not at limit
    if (m_maxTasks == 0 || m_taskCount < m_maxTasks)
    {
        ScheduleNextTask();
    }
}

void
OffloadClient::ScheduleNextTask()
{
    NS_LOG_FUNCTION(this);

    if (m_sendEvent.IsPending())
    {
        return;
    }

    Time nextTime = Seconds(m_interArrivalTime->GetValue());
    m_sendEvent = Simulator::Schedule(nextTime, &OffloadClient::SendTask, this);

    NS_LOG_DEBUG("Next task scheduled in " << nextTime.GetSeconds() << " seconds");
}

void
OffloadClient::ProcessBuffer()
{
    NS_LOG_FUNCTION(this);

    // Header size is constant (33 bytes)
    static const uint32_t headerSize = OffloadHeader::SERIALIZED_SIZE;

    while (m_rxBuffer->GetSize() >= headerSize)
    {
        // Peek header to determine message type
        OffloadHeader header;
        m_rxBuffer->PeekHeader(header);

        // For responses, we only need the header (no additional payload)
        if (header.GetMessageType() != OffloadHeader::TASK_RESPONSE)
        {
            NS_LOG_WARN("Received unexpected message type: " << header.GetMessageType());
            // Remove the header and continue
            m_rxBuffer->RemoveAtStart(headerSize);
            continue;
        }

        // Calculate total message size (header + output payload from server)
        uint64_t payloadSize = header.GetResponsePayloadSize();
        uint64_t totalMessageSize = headerSize + payloadSize;

        // Ensure we have the complete message
        if (m_rxBuffer->GetSize() < totalMessageSize)
        {
            NS_LOG_DEBUG("Buffer has " << m_rxBuffer->GetSize() << " bytes, need "
                                       << totalMessageSize << " for full response");
            break;
        }

        // Remove header from buffer
        m_rxBuffer->RemoveAtStart(headerSize);

        // Remove output payload (we don't need the actual data, just consume it)
        if (payloadSize > 0)
        {
            m_rxBuffer->RemoveAtStart(payloadSize);
        }

        // Validate that this response corresponds to a task we sent
        auto it = m_sendTimes.find(header.GetTaskId());
        if (it == m_sendTimes.end())
        {
            // Response for a task we didn't send - could be a malformed packet
            // or a routing error. Log and skip to avoid corrupting statistics.
            NS_LOG_WARN("Received response for unknown task " << header.GetTaskId()
                                                              << " (not sent by this client)");
            continue;
        }

        // Calculate RTT
        Time rtt = Simulator::Now() - it->second;
        m_sendTimes.erase(it);

        m_responsesReceived++;

        NS_LOG_INFO("Received response for task " << header.GetTaskId()
                                                  << " (RTT=" << rtt.GetMilliSeconds() << "ms)");

        // Fire trace
        m_responseReceivedTrace(header, rtt);
    }
}

} // namespace ns3
