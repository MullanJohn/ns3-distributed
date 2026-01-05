/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-client.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OffloadClient");

NS_OBJECT_ENSURE_REGISTERED(OffloadClient);

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
            .AddAttribute("Local",
                          "The local address to bind to",
                          AddressValue(),
                          MakeAddressAccessor(&OffloadClient::m_local),
                          MakeAddressChecker())
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
                            "ns3::OffloadClient::TaskSentTracedCallback");
    return tid;
}

OffloadClient::OffloadClient()
    : m_socket(nullptr),
      m_connected(false),
      m_maxTasks(0),
      m_taskCount(0),
      m_totalTx(0)
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
    m_socket = nullptr;
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

void
OffloadClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());

        NS_ABORT_MSG_IF(m_peer.IsInvalid(), "Remote address not set");

        if (!m_local.IsInvalid())
        {
            NS_ABORT_MSG_IF(
                (Inet6SocketAddress::IsMatchingType(m_peer) &&
                 InetSocketAddress::IsMatchingType(m_local)) ||
                    (InetSocketAddress::IsMatchingType(m_peer) &&
                     Inet6SocketAddress::IsMatchingType(m_local)),
                "Incompatible peer and local address IP version");

            if (m_socket->Bind(m_local) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
        }
        else
        {
            if (InetSocketAddress::IsMatchingType(m_peer))
            {
                if (m_socket->Bind() == -1)
                {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
            }
            else if (Inet6SocketAddress::IsMatchingType(m_peer))
            {
                if (m_socket->Bind6() == -1)
                {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
            }
        }

        m_socket->SetConnectCallback(MakeCallback(&OffloadClient::ConnectionSucceeded, this),
                                     MakeCallback(&OffloadClient::ConnectionFailed, this));
        m_socket->Connect(m_peer);
    }
}

void
OffloadClient::StopApplication()
{
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);

    if (m_socket)
    {
        m_socket->Close();
        m_connected = false;
    }
}

void
OffloadClient::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_INFO("Connection succeeded");
    m_connected = true;

    // Start sending tasks
    ScheduleNextTask();
}

void
OffloadClient::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_ERROR("Connection failed");
}

void
OffloadClient::SendTask()
{
    NS_LOG_FUNCTION(this);

    if (!m_connected)
    {
        NS_LOG_WARN("Not connected, cannot send task");
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

    // Create header
    OffloadHeader header;
    header.SetMessageType(OffloadHeader::TASK_REQUEST);
    header.SetTaskId(m_taskCount);
    header.SetComputeDemand(computeDemand);
    header.SetInputSize(inputSize);
    header.SetOutputSize(outputSize);

    // Create packet with payload matching input size
    uint32_t headerSize = header.GetSerializedSize();
    uint32_t payloadSize = (inputSize > headerSize) ? (inputSize - headerSize) : 0;
    Ptr<Packet> packet = Create<Packet>(payloadSize);
    packet->AddHeader(header);

    // Send packet
    int sent = m_socket->Send(packet);
    if (sent > 0)
    {
        m_taskCount++;
        m_totalTx += sent;

        NS_LOG_INFO("Sent task " << header.GetTaskId() << " with " << sent << " bytes"
                                 << " (compute=" << computeDemand << " FLOPS"
                                 << ", input=" << inputSize << " bytes"
                                 << ", output=" << outputSize << " bytes)");

        m_taskSentTrace(header);
    }
    else
    {
        NS_LOG_ERROR("Failed to send task");
    }

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

} // namespace ns3
