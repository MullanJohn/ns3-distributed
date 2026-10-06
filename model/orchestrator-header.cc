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
                            .SetGroupName("Continuum")
                            .AddConstructor<OrchestratorHeader>();
    return tid;
}

OrchestratorHeader::OrchestratorHeader()
    : m_messageType(WORKLOAD_REQUEST), // 8
      m_dagId(0),
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
OrchestratorHeader::GetDagId() const
{
    return m_dagId;
}

void
OrchestratorHeader::SetDagId(uint64_t id)
{
    NS_LOG_FUNCTION(this << id);
    m_dagId = id;
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
OrchestratorHeader::IsResponse() const
{
    return m_messageType == ADMISSION_RESPONSE;
}

bool
OrchestratorHeader::IsWorkloadRequest() const
{
    return m_messageType == WORKLOAD_REQUEST;
}

bool
OrchestratorHeader::IsWorkloadResponse() const
{
    return m_messageType == WORKLOAD_RESPONSE;
}

std::string
OrchestratorHeader::GetMessageTypeName() const
{
    switch (m_messageType)
    {
    case ADMISSION_RESPONSE:
        return "ADMISSION_RESPONSE";
    case WORKLOAD_RESPONSE:
        return "WORKLOAD_RESPONSE";
    case WORKLOAD_REQUEST:
        return "WORKLOAD_REQUEST";
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
    start.WriteHtonU64(m_dagId);
    start.WriteU8(m_admitted ? 1 : 0);
    start.WriteHtonU64(m_payloadSize);
}

uint32_t
OrchestratorHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this);

    uint8_t messageTypeByte = start.ReadU8();
    // Explicit set rather than a range: 5 and 6 fall between ADMISSION_RESPONSE
    // and WORKLOAD_RESPONSE but belong to ScalingCommandHeader/DeviceMetricsHeader.
    if (messageTypeByte != ADMISSION_RESPONSE && messageTypeByte != WORKLOAD_RESPONSE &&
        messageTypeByte != WORKLOAD_REQUEST)
    {
        NS_LOG_WARN("Invalid OrchestratorHeader message type "
                    << static_cast<uint32_t>(messageTypeByte) << ", clamping to WORKLOAD_REQUEST");
        m_messageType = WORKLOAD_REQUEST;
    }
    else
    {
        m_messageType = static_cast<MessageType>(messageTypeByte);
    }
    m_dagId = start.ReadNtohU64();
    m_admitted = (start.ReadU8() != 0);
    m_payloadSize = start.ReadNtohU64();

    return SERIALIZED_SIZE;
}

void
OrchestratorHeader::Print(std::ostream& os) const
{
    os << "OrchestratorHeader(type=" << GetMessageTypeName() << ", dagId=" << m_dagId
       << ", admitted=" << (m_admitted ? "true" : "false") << ", payloadSize=" << m_payloadSize
       << ")";
}

} // namespace ns3
