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

DeviceMetricsHeader::DeviceMetricsHeader()
    : m_messageType(DEVICE_METRICS),
      m_frequency(0),
      m_voltage(0),
      m_busy(false),
      m_queueLength(0),
      m_currentPower(0)
{
    NS_LOG_FUNCTION(this);
}

DeviceMetricsHeader::~DeviceMetricsHeader()
{
    NS_LOG_FUNCTION(this);
}

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
    m_busy = busy;
}

uint32_t
DeviceMetricsHeader::GetQueueLength() const
{
    return m_queueLength;
}

void
DeviceMetricsHeader::SetQueueLength(uint32_t queueLength)
{
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
    NS_LOG_FUNCTION(this);

    start.WriteU8(m_messageType);

    uint64_t freqBits;
    std::memcpy(&freqBits, &m_frequency, sizeof(freqBits));
    start.WriteHtonU64(freqBits);

    uint64_t voltBits;
    std::memcpy(&voltBits, &m_voltage, sizeof(voltBits));
    start.WriteHtonU64(voltBits);

    start.WriteU8(m_busy ? 1 : 0);
    start.WriteHtonU32(m_queueLength);

    uint64_t powerBits;
    std::memcpy(&powerBits, &m_currentPower, sizeof(powerBits));
    start.WriteHtonU64(powerBits);
}

uint32_t
DeviceMetricsHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this);

    m_messageType = start.ReadU8();

    uint64_t freqBits = start.ReadNtohU64();
    std::memcpy(&m_frequency, &freqBits, sizeof(m_frequency));

    uint64_t voltBits = start.ReadNtohU64();
    std::memcpy(&m_voltage, &voltBits, sizeof(m_voltage));

    m_busy = (start.ReadU8() != 0);
    m_queueLength = start.ReadNtohU32();

    uint64_t powerBits = start.ReadNtohU64();
    std::memcpy(&m_currentPower, &powerBits, sizeof(m_currentPower));

    return SERIALIZED_SIZE;
}

void
DeviceMetricsHeader::Print(std::ostream& os) const
{
    os << "DeviceMetricsHeader(type=" << static_cast<int>(m_messageType)
       << ", freq=" << m_frequency << ", volt=" << m_voltage
       << ", busy=" << (m_busy ? "true" : "false") << ", qLen=" << m_queueLength
       << ", power=" << m_currentPower << ")";
}

} // namespace ns3
