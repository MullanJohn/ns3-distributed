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
                            .SetGroupName("Distributed")
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
DeviceMetricsHeader::GetFrequency() const
{
    return m_frequency;
}

void
DeviceMetricsHeader::SetFrequency(double frequency)
{
    NS_LOG_FUNCTION(this << frequency);
    m_frequency = frequency;
}

double
DeviceMetricsHeader::GetVoltage() const
{
    return m_voltage;
}

void
DeviceMetricsHeader::SetVoltage(double voltage)
{
    NS_LOG_FUNCTION(this << voltage);
    m_voltage = voltage;
}

bool
DeviceMetricsHeader::GetBusy() const
{
    return m_busy;
}

void
DeviceMetricsHeader::SetBusy(bool busy)
{
    NS_LOG_FUNCTION(this << busy);
    m_busy = busy;
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

uint32_t
DeviceMetricsHeader::GetQueueLength() const
{
    return m_queueLength;
}

void
DeviceMetricsHeader::SetQueueLength(uint32_t queueLength)
{
    NS_LOG_FUNCTION(this << queueLength);
    m_queueLength = queueLength;
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
    std::memcpy(&bits, &m_frequency, sizeof(bits));
    start.WriteHtonU64(bits);
    std::memcpy(&bits, &m_voltage, sizeof(bits));
    start.WriteHtonU64(bits);
    start.WriteU8(m_busy ? 1 : 0);
    std::memcpy(&bits, &m_utilization, sizeof(bits));
    start.WriteHtonU64(bits);
    start.WriteHtonU32(m_queueLength);
    std::memcpy(&bits, &m_currentPower, sizeof(bits));
    start.WriteHtonU64(bits);
}

uint32_t
DeviceMetricsHeader::Deserialize(Buffer::Iterator start)
{
    m_messageType = start.ReadU8();

    uint64_t bits = start.ReadNtohU64();
    std::memcpy(&m_frequency, &bits, sizeof(m_frequency));
    bits = start.ReadNtohU64();
    std::memcpy(&m_voltage, &bits, sizeof(m_voltage));
    m_busy = (start.ReadU8() != 0);
    bits = start.ReadNtohU64();
    std::memcpy(&m_utilization, &bits, sizeof(m_utilization));
    m_queueLength = start.ReadNtohU32();
    bits = start.ReadNtohU64();
    std::memcpy(&m_currentPower, &bits, sizeof(m_currentPower));
    return SERIALIZED_SIZE;
}

void
DeviceMetricsHeader::Print(std::ostream& os) const
{
    os << "DeviceMetricsHeader(type=" << static_cast<int>(m_messageType) << ", freq=" << m_frequency
       << ", volt=" << m_voltage << ", busy=" << (m_busy ? "true" : "false")
       << ", util=" << m_utilization << ", qLen=" << m_queueLength
       << ", power=" << m_currentPower << ")";
}

} // namespace ns3
