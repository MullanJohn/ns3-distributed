/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DEVICE_METRICS_HEADER_H
#define DEVICE_METRICS_HEADER_H

#include "ns3/header.h"

#include <cstdint>
#include <ostream>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Header for device metrics reports sent from backend to orchestrator.
 *
 * DeviceMetricsHeader carries accelerator state (frequency, voltage, busy
 * status, queue length, power) from an OffloadServer to the EdgeOrchestrator's
 * DeviceManager. It is multiplexed on the same connection as task data using
 * message type 4.
 *
 * Wire format (30 bytes):
 * - messageType: 1 byte (uint8_t, always DEVICE_METRICS = 4)
 * - frequency: 8 bytes (double as uint64_t via memcpy, network byte order)
 * - voltage: 8 bytes (double as uint64_t via memcpy, network byte order)
 * - busy: 1 byte (uint8_t, 0=idle, 1=busy)
 * - queueLength: 4 bytes (uint32_t, network byte order)
 * - currentPower: 8 bytes (double as uint64_t via memcpy, network byte order)
 */
class DeviceMetricsHeader : public Header
{
  public:
    /**
     * @brief Message type value for device metrics.
     */
    static constexpr uint8_t DEVICE_METRICS = 4;

    /**
     * @brief Serialized size of the header in bytes.
     */
    static constexpr uint32_t SERIALIZED_SIZE = 30;

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    DeviceMetricsHeader();
    ~DeviceMetricsHeader() override;

    uint8_t GetMessageType() const;
    void SetMessageType(uint8_t type);

    double GetFrequency() const;
    void SetFrequency(double frequency);

    double GetVoltage() const;
    void SetVoltage(double voltage);

    bool GetBusy() const;
    void SetBusy(bool busy);

    uint32_t GetQueueLength() const;
    void SetQueueLength(uint32_t queueLength);

    double GetCurrentPower() const;
    void SetCurrentPower(double power);

    // Header interface
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

  private:
    uint8_t m_messageType{DEVICE_METRICS}; //!< Message type (always 4)
    double m_frequency{0};                 //!< Current frequency in Hz
    double m_voltage{0};                   //!< Current voltage in Volts
    bool m_busy{false};                    //!< Whether accelerator is busy
    uint32_t m_queueLength{0};             //!< Tasks in queue
    double m_currentPower{0};              //!< Current power in Watts
};

} // namespace ns3

#endif // DEVICE_METRICS_HEADER_H
