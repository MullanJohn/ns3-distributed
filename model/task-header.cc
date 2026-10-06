/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "task-header.h"

#include "ns3/log.h"

#include <cstring>
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TaskHeader");

NS_OBJECT_ENSURE_REGISTERED(TaskHeader);

TypeId
TaskHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TaskHeader")
                            .SetParent<Header>()
                            .SetGroupName("Continuum")
                            .AddConstructor<TaskHeader>();
    return tid;
}

TaskHeader::TaskHeader()
{
    NS_LOG_FUNCTION(this);
}

TaskHeader::~TaskHeader()
{
    NS_LOG_FUNCTION(this);
}

TypeId
TaskHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
TaskHeader::GetSerializedSize() const
{
    return sizeof(uint8_t) +  // m_messageType
           sizeof(uint64_t) + // m_taskId
           sizeof(uint64_t) + // m_computeDemand
           sizeof(uint64_t) + // m_inputSize
           sizeof(uint64_t) + // m_outputSize
           sizeof(int64_t) +  // m_deadlineNs
           sizeof(uint32_t) + // m_priority
           ACCEL_TYPE_SIZE +  // m_acceleratorType (fixed 16 bytes)
           sizeof(int64_t) +  // m_backendTimeNs
           sizeof(int64_t);   // m_computeTimeNs
}

void
TaskHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this << &start);

    start.WriteU8(static_cast<uint8_t>(m_messageType));
    start.WriteHtonU64(m_taskId);

    uint64_t computeDemandBits;
    std::memcpy(&computeDemandBits, &m_computeDemand, sizeof(m_computeDemand));
    start.WriteHtonU64(computeDemandBits);

    start.WriteHtonU64(m_inputSize);
    start.WriteHtonU64(m_outputSize);

    start.WriteHtonU64(static_cast<uint64_t>(m_deadlineNs));

    start.WriteHtonU32(m_priority);

    for (uint32_t i = 0; i < ACCEL_TYPE_SIZE; i++)
    {
        start.WriteU8(i < m_acceleratorType.size() ? m_acceleratorType[i] : 0);
    }

    start.WriteHtonU64(static_cast<uint64_t>(m_backendTimeNs));
    start.WriteHtonU64(static_cast<uint64_t>(m_computeTimeNs));
}

uint32_t
TaskHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this << &start);

    Buffer::Iterator original = start;

    uint8_t messageTypeByte = start.ReadU8();
    if (messageTypeByte > TASK_RESPONSE)
    {
        NS_LOG_WARN("Invalid message type " << static_cast<int>(messageTypeByte)
                                            << " in TaskHeader, clamping to TASK_REQUEST");
        m_messageType = TASK_REQUEST;
    }
    else
    {
        m_messageType = static_cast<MessageType>(messageTypeByte);
    }

    m_taskId = start.ReadNtohU64();

    uint64_t computeDemandBits = start.ReadNtohU64();
    std::memcpy(&m_computeDemand, &computeDemandBits, sizeof(m_computeDemand));

    m_inputSize = start.ReadNtohU64();
    m_outputSize = start.ReadNtohU64();

    m_deadlineNs = static_cast<int64_t>(start.ReadNtohU64());

    m_priority = start.ReadNtohU32();

    char accelBuf[ACCEL_TYPE_SIZE + 1] = {0};
    for (uint32_t i = 0; i < ACCEL_TYPE_SIZE; i++)
    {
        accelBuf[i] = static_cast<char>(start.ReadU8());
    }
    m_acceleratorType = std::string(accelBuf);

    m_backendTimeNs = static_cast<int64_t>(start.ReadNtohU64());
    m_computeTimeNs = static_cast<int64_t>(start.ReadNtohU64());

    return start.GetDistanceFrom(original);
}

void
TaskHeader::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);

    os << "(Type: ";
    switch (m_messageType)
    {
    case TASK_REQUEST:
        os << "Request";
        break;
    case TASK_RESPONSE:
        os << "Response";
        break;
    default:
        os << "Unknown";
        break;
    }
    os << ", TaskId: " << m_taskId << ", ComputeDemand: " << m_computeDemand
       << ", InputSize: " << m_inputSize << ", OutputSize: " << m_outputSize
       << ", Deadline: " << (m_deadlineNs >= 0 ? std::to_string(m_deadlineNs) + "ns" : "none")
       << ", Priority: " << m_priority
       << ", AcceleratorType: " << (m_acceleratorType.empty() ? "any" : m_acceleratorType)
       << ", BackendTime: " << m_backendTimeNs << "ns"
       << ", ComputeTime: " << m_computeTimeNs << "ns)";
}

std::string
TaskHeader::ToString() const
{
    NS_LOG_FUNCTION(this);
    std::ostringstream oss;
    Print(oss);
    return oss.str();
}

bool
TaskHeader::IsRequest() const
{
    return m_messageType == TASK_REQUEST;
}

bool
TaskHeader::IsResponse() const
{
    return m_messageType == TASK_RESPONSE;
}

void
TaskHeader::SetMessageType(MessageType messageType)
{
    NS_LOG_FUNCTION(this << messageType);
    m_messageType = messageType;
}

TaskHeader::MessageType
TaskHeader::GetMessageType() const
{
    return m_messageType;
}

void
TaskHeader::SetTaskId(uint64_t taskId)
{
    NS_LOG_FUNCTION(this << taskId);
    m_taskId = taskId;
}

uint64_t
TaskHeader::GetTaskId() const
{
    return m_taskId;
}

void
TaskHeader::SetComputeDemand(double computeDemand)
{
    NS_LOG_FUNCTION(this << computeDemand);
    m_computeDemand = computeDemand;
}

double
TaskHeader::GetComputeDemand() const
{
    return m_computeDemand;
}

void
TaskHeader::SetInputSize(uint64_t inputSize)
{
    NS_LOG_FUNCTION(this << inputSize);
    m_inputSize = inputSize;
}

uint64_t
TaskHeader::GetInputSize() const
{
    return m_inputSize;
}

void
TaskHeader::SetOutputSize(uint64_t outputSize)
{
    NS_LOG_FUNCTION(this << outputSize);
    m_outputSize = outputSize;
}

uint64_t
TaskHeader::GetOutputSize() const
{
    return m_outputSize;
}

uint64_t
TaskHeader::GetRequestPayloadSize() const
{
    return m_inputSize;
}

uint64_t
TaskHeader::GetResponsePayloadSize() const
{
    return m_outputSize;
}

bool
TaskHeader::HasDeadline() const
{
    return m_deadlineNs >= 0;
}

int64_t
TaskHeader::GetDeadlineNs() const
{
    return m_deadlineNs;
}

void
TaskHeader::SetDeadlineNs(int64_t deadlineNs)
{
    NS_LOG_FUNCTION(this << deadlineNs);
    m_deadlineNs = deadlineNs;
}

uint32_t
TaskHeader::GetPriority() const
{
    return m_priority;
}

void
TaskHeader::SetPriority(uint32_t priority)
{
    NS_LOG_FUNCTION(this << priority);
    m_priority = priority;
}

std::string
TaskHeader::GetAcceleratorType() const
{
    return m_acceleratorType;
}

void
TaskHeader::SetAcceleratorType(const std::string& type)
{
    NS_LOG_FUNCTION(this << type);
    m_acceleratorType = type.substr(0, ACCEL_TYPE_SIZE);
}

int64_t
TaskHeader::GetBackendTimeNs() const
{
    return m_backendTimeNs;
}

void
TaskHeader::SetBackendTimeNs(int64_t backendTimeNs)
{
    NS_LOG_FUNCTION(this << backendTimeNs);
    m_backendTimeNs = backendTimeNs;
}

int64_t
TaskHeader::GetComputeTimeNs() const
{
    return m_computeTimeNs;
}

void
TaskHeader::SetComputeTimeNs(int64_t computeTimeNs)
{
    NS_LOG_FUNCTION(this << computeTimeNs);
    m_computeTimeNs = computeTimeNs;
}

} // namespace ns3
