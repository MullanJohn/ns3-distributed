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
 * @ingroup continuum
 * @brief Header for device metrics reports sent from backend to orchestrator.
 *
 * DeviceMetricsHeader carries accelerator telemetry from a backend
 * to the Orchestrator's DeviceManager.
 *
 * Wire format (17 bytes):
 * - messageType: 1 byte (uint8_t, always DEVICE_METRICS = 6)
 * - utilization: 8 bytes (double as uint64_t via memcpy, network byte order)
 * - currentPower: 8 bytes (double as uint64_t via memcpy, network byte order)
 */
class DeviceMetricsHeader : public Header
{
  public:
    /**
     * @brief Message type value for device metrics.
     */
    static constexpr uint8_t DEVICE_METRICS = 6;

    /**
     * @brief Serialized size of the header in bytes.
     */
    static constexpr uint32_t SERIALIZED_SIZE = 17;

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    DeviceMetricsHeader();
    ~DeviceMetricsHeader() override;

    /**
     * @brief Get the message type.
     * @return The message type.
     */
    uint8_t GetMessageType() const;

    /**
     * @brief Set the message type.
     * @param type The message type.
     */
    void SetMessageType(uint8_t type);

    /**
     * @brief Get the rolling compute utilization.
     * @return Utilization in [0.0, 1.0].
     */
    double GetUtilization() const;

    /**
     * @brief Set the rolling compute utilization.
     * @param utilization Utilization in [0.0, 1.0].
     */
    void SetUtilization(double utilization);

    /**
     * @brief Get the current power draw.
     * @return Power in Watts.
     */
    double GetCurrentPower() const;

    /**
     * @brief Set the current power draw.
     * @param power Power in Watts.
     */
    void SetCurrentPower(double power);

    // Header interface
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

  private:
    uint8_t m_messageType{DEVICE_METRICS}; //!< Message type (always 6)
    double m_utilization{0};               //!< Rolling compute utilization [0.0, 1.0]
    double m_currentPower{0};              //!< Current power in Watts
};

} // namespace ns3

#endif // DEVICE_METRICS_HEADER_H
