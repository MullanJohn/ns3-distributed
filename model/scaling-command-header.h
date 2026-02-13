/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef SCALING_COMMAND_HEADER_H
#define SCALING_COMMAND_HEADER_H

#include "ns3/header.h"

#include <cstdint>
#include <ostream>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Header for scaling commands sent from orchestrator to backend.
 *
 * ScalingCommandHeader carries DVFS commands (target frequency, target voltage)
 * from the EdgeOrchestrator's DeviceManager to an OffloadServer. It is
 * multiplexed on the same connection as task data using message type 5.
 *
 * Wire format (17 bytes):
 * - messageType: 1 byte (uint8_t, always SCALING_COMMAND = 5)
 * - targetFrequency: 8 bytes (double as uint64_t via memcpy, network byte order)
 * - targetVoltage: 8 bytes (double as uint64_t via memcpy, network byte order)
 */
class ScalingCommandHeader : public Header
{
  public:
    /**
     * @brief Message type value for scaling commands.
     */
    static constexpr uint8_t SCALING_COMMAND = 5;

    /**
     * @brief Serialized size of the header in bytes.
     */
    static constexpr uint32_t SERIALIZED_SIZE = 17;

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ScalingCommandHeader();
    ~ScalingCommandHeader() override;

    uint8_t GetMessageType() const;
    void SetMessageType(uint8_t type);

    double GetTargetFrequency() const;
    void SetTargetFrequency(double frequency);

    double GetTargetVoltage() const;
    void SetTargetVoltage(double voltage);

    // Header interface
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

  private:
    uint8_t m_messageType{SCALING_COMMAND}; //!< Message type (always 5)
    double m_targetFrequency{0};            //!< Target frequency in Hz
    double m_targetVoltage{0};              //!< Target voltage in Volts
};

} // namespace ns3

#endif // SCALING_COMMAND_HEADER_H
