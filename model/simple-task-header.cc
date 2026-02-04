/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "simple-task-header.h"

#include "ns3/log.h"

#include <cstring>
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SimpleTaskHeader");
NS_OBJECT_ENSURE_REGISTERED(SimpleTaskHeader);

SimpleTaskHeader::SimpleTaskHeader()
    : TaskHeader(),
      m_messageType(TASK_REQUEST),
      m_taskId(0),
      m_computeDemand(0.0),
      m_inputSize(0),
      m_outputSize(0),
      m_deadlineNs(-1),
      m_acceleratorType("")
{
    NS_LOG_FUNCTION(this);
}

SimpleTaskHeader::~SimpleTaskHeader()
{
    NS_LOG_FUNCTION(this);
}

TypeId
SimpleTaskHeader::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::SimpleTaskHeader").SetParent<TaskHeader>().AddConstructor<SimpleTaskHeader>();
    return tid;
}

TypeId
SimpleTaskHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
SimpleTaskHeader::GetSerializedSize() const
{
    return sizeof(uint8_t) +  // m_messageType
           sizeof(uint64_t) + // m_taskId
           sizeof(uint64_t) + // m_computeDemand (as uint64_t)
           sizeof(uint64_t) + // m_inputSize
           sizeof(uint64_t) + // m_outputSize
           sizeof(int64_t) +  // m_deadlineNs
           ACCEL_TYPE_SIZE;   // m_acceleratorType (fixed 16 bytes)
}

void
SimpleTaskHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this << &start);

    start.WriteU8(static_cast<uint8_t>(m_messageType));
    start.WriteHtonU64(m_taskId);

    // Serialize double as uint64_t (bit-level copy) in network byte order
    uint64_t computeDemandBits;
    std::memcpy(&computeDemandBits, &m_computeDemand, sizeof(m_computeDemand));
    start.WriteHtonU64(computeDemandBits);

    start.WriteHtonU64(m_inputSize);
    start.WriteHtonU64(m_outputSize);

    // Serialize deadline as int64_t in network byte order
    start.WriteHtonU64(static_cast<uint64_t>(m_deadlineNs));

    // Serialize accelerator type as fixed 16 bytes (null-padded)
    for (uint32_t i = 0; i < ACCEL_TYPE_SIZE; i++)
    {
        start.WriteU8(i < m_acceleratorType.size() ? m_acceleratorType[i] : 0);
    }
}

uint32_t
SimpleTaskHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this << &start);

    Buffer::Iterator original = start;

    uint8_t messageTypeByte = start.ReadU8();
    if (messageTypeByte > TASK_RESPONSE)
    {
        NS_LOG_WARN("Invalid message type " << static_cast<int>(messageTypeByte)
                                            << " received in SimpleTaskHeader");
    }
    m_messageType = static_cast<MessageType>(messageTypeByte);

    m_taskId = start.ReadNtohU64();

    // Deserialize double from uint64_t in network byte order
    uint64_t computeDemandBits = start.ReadNtohU64();
    std::memcpy(&m_computeDemand, &computeDemandBits, sizeof(m_computeDemand));

    m_inputSize = start.ReadNtohU64();
    m_outputSize = start.ReadNtohU64();

    // Deserialize deadline (stored as uint64_t in network byte order)
    m_deadlineNs = static_cast<int64_t>(start.ReadNtohU64());

    // Deserialize accelerator type (fixed 16 bytes, null-terminated)
    char accelBuf[ACCEL_TYPE_SIZE + 1] = {0};
    for (uint32_t i = 0; i < ACCEL_TYPE_SIZE; i++)
    {
        accelBuf[i] = static_cast<char>(start.ReadU8());
    }
    m_acceleratorType = std::string(accelBuf); // Stops at first null

    return start.GetDistanceFrom(original);
}

void
SimpleTaskHeader::Print(std::ostream& os) const
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
       << ", AcceleratorType: " << (m_acceleratorType.empty() ? "any" : m_acceleratorType)
       << ")";
}

std::string
SimpleTaskHeader::ToString() const
{
    NS_LOG_FUNCTION(this);
    std::ostringstream oss;
    Print(oss);
    return oss.str();
}

void
SimpleTaskHeader::SetMessageType(MessageType messageType)
{
    NS_LOG_FUNCTION(this << messageType);
    m_messageType = messageType;
}

TaskHeader::MessageType
SimpleTaskHeader::GetMessageType() const
{
    return m_messageType;
}

void
SimpleTaskHeader::SetTaskId(uint64_t taskId)
{
    NS_LOG_FUNCTION(this << taskId);
    m_taskId = taskId;
}

uint64_t
SimpleTaskHeader::GetTaskId() const
{
    return m_taskId;
}

void
SimpleTaskHeader::SetComputeDemand(double computeDemand)
{
    NS_LOG_FUNCTION(this << computeDemand);
    m_computeDemand = computeDemand;
}

double
SimpleTaskHeader::GetComputeDemand() const
{
    return m_computeDemand;
}

void
SimpleTaskHeader::SetInputSize(uint64_t inputSize)
{
    NS_LOG_FUNCTION(this << inputSize);
    m_inputSize = inputSize;
}

uint64_t
SimpleTaskHeader::GetInputSize() const
{
    return m_inputSize;
}

void
SimpleTaskHeader::SetOutputSize(uint64_t outputSize)
{
    NS_LOG_FUNCTION(this << outputSize);
    m_outputSize = outputSize;
}

uint64_t
SimpleTaskHeader::GetOutputSize() const
{
    return m_outputSize;
}

uint64_t
SimpleTaskHeader::GetRequestPayloadSize() const
{
    return m_inputSize;
}

uint64_t
SimpleTaskHeader::GetResponsePayloadSize() const
{
    return m_outputSize;
}

bool
SimpleTaskHeader::HasDeadline() const
{
    return m_deadlineNs >= 0;
}

int64_t
SimpleTaskHeader::GetDeadlineNs() const
{
    return m_deadlineNs;
}

void
SimpleTaskHeader::SetDeadlineNs(int64_t deadlineNs)
{
    NS_LOG_FUNCTION(this << deadlineNs);
    m_deadlineNs = deadlineNs;
}

std::string
SimpleTaskHeader::GetAcceleratorType() const
{
    return m_acceleratorType;
}

void
SimpleTaskHeader::SetAcceleratorType(const std::string& type)
{
    NS_LOG_FUNCTION(this << type);
    // Truncate if longer than ACCEL_TYPE_SIZE
    m_acceleratorType = type.substr(0, ACCEL_TYPE_SIZE);
}

} // namespace ns3
