/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "orchestrator-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OrchestratorHeader");

NS_OBJECT_ENSURE_REGISTERED(OrchestratorHeader);

TypeId
OrchestratorHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::OrchestratorHeader")
                            .SetParent<Header>()
                            .SetGroupName("Distributed")
                            .AddConstructor<OrchestratorHeader>();
    return tid;
}

OrchestratorHeader::OrchestratorHeader()
    : m_messageType(ADMISSION_REQUEST), // 2
      m_taskId(0),
      m_admitted(false),
      m_payloadSize(0)
{
    NS_LOG_FUNCTION(this);
}

OrchestratorHeader::~OrchestratorHeader()
{
    NS_LOG_FUNCTION(this);
}

OrchestratorHeader::MessageType
OrchestratorHeader::GetMessageType() const
{
    return m_messageType;
}

void
OrchestratorHeader::SetMessageType(MessageType type)
{
    NS_LOG_FUNCTION(this << static_cast<int>(type));
    m_messageType = type;
}

uint64_t
OrchestratorHeader::GetTaskId() const
{
    return m_taskId;
}

void
OrchestratorHeader::SetTaskId(uint64_t id)
{
    NS_LOG_FUNCTION(this << id);
    m_taskId = id;
}

bool
OrchestratorHeader::IsAdmitted() const
{
    return m_admitted;
}

void
OrchestratorHeader::SetAdmitted(bool admitted)
{
    NS_LOG_FUNCTION(this << admitted);
    m_admitted = admitted;
}

uint64_t
OrchestratorHeader::GetPayloadSize() const
{
    return m_payloadSize;
}

void
OrchestratorHeader::SetPayloadSize(uint64_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_payloadSize = size;
}

bool
OrchestratorHeader::IsRequest() const
{
    return m_messageType == ADMISSION_REQUEST;
}

bool
OrchestratorHeader::IsResponse() const
{
    return m_messageType == ADMISSION_RESPONSE;
}

std::string
OrchestratorHeader::GetMessageTypeName() const
{
    switch (m_messageType)
    {
    case ADMISSION_REQUEST:
        return "ADMISSION_REQUEST";
    case ADMISSION_RESPONSE:
        return "ADMISSION_RESPONSE";
    default:
        return "UNKNOWN";
    }
}

TypeId
OrchestratorHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
OrchestratorHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
OrchestratorHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this);

    start.WriteU8(static_cast<uint8_t>(m_messageType));
    start.WriteHtonU64(m_taskId);
    start.WriteU8(m_admitted ? 1 : 0);
    start.WriteHtonU64(m_payloadSize);
}

uint32_t
OrchestratorHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this);

    m_messageType = static_cast<MessageType>(start.ReadU8());
    m_taskId = start.ReadNtohU64();
    m_admitted = (start.ReadU8() != 0);
    m_payloadSize = start.ReadNtohU64();

    return SERIALIZED_SIZE;
}

void
OrchestratorHeader::Print(std::ostream& os) const
{
    os << "OrchestratorHeader(type=" << GetMessageTypeName() << ", taskId=" << m_taskId
       << ", admitted=" << (m_admitted ? "true" : "false")
       << ", payloadSize=" << m_payloadSize << ")";
}

} // namespace ns3
