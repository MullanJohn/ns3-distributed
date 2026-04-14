/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "device-metrics-header.h"

#include "ns3/log.h"

#include <cstring>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DeviceMetricsHeader");

NS_OBJECT_ENSURE_REGISTERED(DeviceMetricsHeader);

TypeId
DeviceMetricsHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DeviceMetricsHeader")
                            .SetParent<Header>()
                            .SetGroupName("Continuum")
                            .AddConstructor<DeviceMetricsHeader>();
    return tid;
}

DeviceMetricsHeader::DeviceMetricsHeader() = default;

DeviceMetricsHeader::~DeviceMetricsHeader() = default;

uint8_t
DeviceMetricsHeader::GetMessageType() const
{
    return m_messageType;
}

void
DeviceMetricsHeader::SetMessageType(uint8_t type)
{
    NS_LOG_FUNCTION(this << static_cast<int>(type));
    m_messageType = type;
}

double
DeviceMetricsHeader::GetUtilization() const
{
    return m_utilization;
}

void
DeviceMetricsHeader::SetUtilization(double utilization)
{
    NS_LOG_FUNCTION(this << utilization);
    m_utilization = utilization;
}

double
DeviceMetricsHeader::GetCurrentPower() const
{
    return m_currentPower;
}

void
DeviceMetricsHeader::SetCurrentPower(double power)
{
    NS_LOG_FUNCTION(this << power);
    m_currentPower = power;
}

TypeId
DeviceMetricsHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
DeviceMetricsHeader::GetSerializedSize() const
{
    return SERIALIZED_SIZE;
}

void
DeviceMetricsHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU8(m_messageType);

    uint64_t bits = 0;
    std::memcpy(&bits, &m_utilization, sizeof(bits));
    start.WriteHtonU64(bits);
    std::memcpy(&bits, &m_currentPower, sizeof(bits));
    start.WriteHtonU64(bits);
}

uint32_t
DeviceMetricsHeader::Deserialize(Buffer::Iterator start)
{
    m_messageType = start.ReadU8();

    uint64_t bits = start.ReadNtohU64();
    std::memcpy(&m_utilization, &bits, sizeof(m_utilization));
    bits = start.ReadNtohU64();
    std::memcpy(&m_currentPower, &bits, sizeof(m_currentPower));
    return SERIALIZED_SIZE;
}

void
DeviceMetricsHeader::Print(std::ostream& os) const
{
    os << "DeviceMetricsHeader(type=" << static_cast<int>(m_messageType)
       << ", util=" << m_utilization << ", power=" << m_currentPower << ")";
}

} // namespace ns3
