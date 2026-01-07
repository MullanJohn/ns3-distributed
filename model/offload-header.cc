/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-header.h"

#include "ns3/log.h"

#include <cstring>
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OffloadHeader");

// Compile-time validation that SERIALIZED_SIZE matches actual serialization
static_assert(OffloadHeader::SERIALIZED_SIZE ==
              sizeof(uint8_t) +     // messageType
              sizeof(uint64_t) +    // taskId
              sizeof(uint64_t) +    // computeDemand (as uint64_t)
              sizeof(uint64_t) +    // inputSize
              sizeof(uint64_t),     // outputSize
              "OffloadHeader::SERIALIZED_SIZE does not match actual serialization");

NS_OBJECT_ENSURE_REGISTERED(OffloadHeader);

OffloadHeader::OffloadHeader()
    : Header(),
      m_messageType(TASK_REQUEST),
      m_taskId(0),
      m_computeDemand(0.0),
      m_inputSize(0),
      m_outputSize(0)
{
    NS_LOG_FUNCTION(this);
}

OffloadHeader::~OffloadHeader()
{
    NS_LOG_FUNCTION(this);
}

TypeId
OffloadHeader::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OffloadHeader").SetParent<Header>().AddConstructor<OffloadHeader>();
    return tid;
}

TypeId
OffloadHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
OffloadHeader::GetSerializedSize() const
{
    return sizeof(uint8_t) +   // m_messageType
           sizeof(uint64_t) +  // m_taskId
           sizeof(uint64_t) +  // m_computeDemand (as uint64_t)
           sizeof(uint64_t) +  // m_inputSize
           sizeof(uint64_t);   // m_outputSize
}

void
OffloadHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this << &start);

    start.WriteU8(static_cast<uint8_t>(m_messageType));
    start.WriteU64(m_taskId);

    // Serialize double as uint64_t (bit-level copy)
    uint64_t computeDemandBits;
    std::memcpy(&computeDemandBits, &m_computeDemand, sizeof(m_computeDemand));
    start.WriteU64(computeDemandBits);

    start.WriteU64(m_inputSize);
    start.WriteU64(m_outputSize);
}

uint32_t
OffloadHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this << &start);

    Buffer::Iterator original = start;

    uint8_t messageTypeByte = start.ReadU8();
    if (messageTypeByte > TASK_RESPONSE)
    {
        NS_LOG_WARN("Invalid message type " << static_cast<int>(messageTypeByte)
                    << " received in OffloadHeader");
    }
    m_messageType = static_cast<MessageType>(messageTypeByte);

    m_taskId = start.ReadU64();

    // Deserialize double from uint64_t
    uint64_t computeDemandBits = start.ReadU64();
    std::memcpy(&m_computeDemand, &computeDemandBits, sizeof(m_computeDemand));

    m_inputSize = start.ReadU64();
    m_outputSize = start.ReadU64();

    return start.GetDistanceFrom(original);
}

void
OffloadHeader::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);

    os << "(Type: ";
    switch (m_messageType)
    {
    case TASK_REQUEST:
        os << "TaskRequest";
        break;
    case TASK_RESPONSE:
        os << "TaskResponse";
        break;
    default:
        os << "Unknown";
        break;
    }
    os << ", TaskId: " << m_taskId << ", ComputeDemand: " << m_computeDemand
       << ", InputSize: " << m_inputSize << ", OutputSize: " << m_outputSize << ")";
}

std::string
OffloadHeader::ToString() const
{
    NS_LOG_FUNCTION(this);
    std::ostringstream oss;
    Print(oss);
    return oss.str();
}

void
OffloadHeader::SetMessageType(MessageType messageType)
{
    NS_LOG_FUNCTION(this << messageType);
    m_messageType = messageType;
}

OffloadHeader::MessageType
OffloadHeader::GetMessageType() const
{
    return m_messageType;
}

void
OffloadHeader::SetTaskId(uint64_t taskId)
{
    NS_LOG_FUNCTION(this << taskId);
    m_taskId = taskId;
}

uint64_t
OffloadHeader::GetTaskId() const
{
    return m_taskId;
}

void
OffloadHeader::SetComputeDemand(double computeDemand)
{
    NS_LOG_FUNCTION(this << computeDemand);
    m_computeDemand = computeDemand;
}

double
OffloadHeader::GetComputeDemand() const
{
    return m_computeDemand;
}

void
OffloadHeader::SetInputSize(uint64_t inputSize)
{
    NS_LOG_FUNCTION(this << inputSize);
    m_inputSize = inputSize;
}

uint64_t
OffloadHeader::GetInputSize() const
{
    return m_inputSize;
}

void
OffloadHeader::SetOutputSize(uint64_t outputSize)
{
    NS_LOG_FUNCTION(this << outputSize);
    m_outputSize = outputSize;
}

uint64_t
OffloadHeader::GetOutputSize() const
{
    return m_outputSize;
}

uint64_t
OffloadHeader::GetRequestPayloadSize() const
{
    return (m_inputSize > SERIALIZED_SIZE) ? (m_inputSize - SERIALIZED_SIZE) : 0;
}

uint64_t
OffloadHeader::GetResponsePayloadSize() const
{
    return m_outputSize;
}

} // namespace ns3
