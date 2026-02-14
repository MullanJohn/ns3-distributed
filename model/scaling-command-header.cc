/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "scaling-command-header.h"

#include "ns3/log.h"

#include <cstring>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ScalingCommandHeader");

NS_OBJECT_ENSURE_REGISTERED(ScalingCommandHeader);

TypeId
ScalingCommandHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ScalingCommandHeader")
                            .SetParent<Header>()
                            .SetGroupName("Distributed")
                            .AddConstructor<ScalingCommandHeader>();
    return tid;
}

ScalingCommandHeader::ScalingCommandHeader()
    : m_messageType(SCALING_COMMAND),
      m_targetFrequency(0),
      m_targetVoltage(0)
{
    NS_LOG_FUNCTION(this);
}

ScalingCommandHeader::~ScalingCommandHeader()
{
    NS_LOG_FUNCTION(this);
}

uint8_t
ScalingCommandHeader::GetMessageType() const
{
    return m_messageType;
}

void
ScalingCommandHeader::SetMessageType(uint8_t type)
{
    NS_LOG_FUNCTION(this << static_cast<int>(type));
    m_messageType = type;
}

double
ScalingCommandHeader::GetTargetFrequency() const
{
    return m_targetFrequency;
}

void
ScalingCommandHeader::SetTargetFrequency(double frequency)
{
    m_targetFrequency = frequency;
}

double
ScalingCommandHeader::GetTargetVoltage() const
{
    return m_targetVoltage;
}

void
ScalingCommandHeader::SetTargetVoltage(double voltage)
{
    m_targetVoltage = voltage;
}

TypeId
ScalingCommandHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
ScalingCommandHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
ScalingCommandHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this);

    start.WriteU8(m_messageType);

    uint64_t freqBits;
    std::memcpy(&freqBits, &m_targetFrequency, sizeof(freqBits));
    start.WriteHtonU64(freqBits);

    uint64_t voltBits;
    std::memcpy(&voltBits, &m_targetVoltage, sizeof(voltBits));
    start.WriteHtonU64(voltBits);
}

uint32_t
ScalingCommandHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this);

    m_messageType = start.ReadU8();

    uint64_t freqBits = start.ReadNtohU64();
    std::memcpy(&m_targetFrequency, &freqBits, sizeof(m_targetFrequency));

    uint64_t voltBits = start.ReadNtohU64();
    std::memcpy(&m_targetVoltage, &voltBits, sizeof(m_targetVoltage));

    return SERIALIZED_SIZE;
}

void
ScalingCommandHeader::Print(std::ostream& os) const
{
    os << "ScalingCommandHeader(type=" << static_cast<int>(m_messageType)
       << ", targetFreq=" << m_targetFrequency << ", targetVolt=" << m_targetVoltage << ")";
}

} // namespace ns3
