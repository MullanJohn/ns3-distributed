/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "scaling-command-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ScalingCommandHeader");

NS_OBJECT_ENSURE_REGISTERED(ScalingCommandHeader);

TypeId
ScalingCommandHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ScalingCommandHeader")
                            .SetParent<Header>()
                            .SetGroupName("Continuum")
                            .AddConstructor<ScalingCommandHeader>();
    return tid;
}

ScalingCommandHeader::ScalingCommandHeader()
    : m_messageType(SCALING_COMMAND),
      m_targetStateIdx(0)
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

uint32_t
ScalingCommandHeader::GetTargetStateIdx() const
{
    return m_targetStateIdx;
}

void
ScalingCommandHeader::SetTargetStateIdx(uint32_t idx)
{
    NS_LOG_FUNCTION(this << idx);
    m_targetStateIdx = idx;
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
    start.WriteHtonU32(m_targetStateIdx);
}

uint32_t
ScalingCommandHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this);
    m_messageType = start.ReadU8();
    m_targetStateIdx = start.ReadNtohU32();
    return SERIALIZED_SIZE;
}

void
ScalingCommandHeader::Print(std::ostream& os) const
{
    os << "ScalingCommandHeader(type=" << static_cast<int>(m_messageType)
       << ", targetStateIdx=" << m_targetStateIdx << ")";
}

} // namespace ns3
