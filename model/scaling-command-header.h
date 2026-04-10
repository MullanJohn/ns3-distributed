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
 * @ingroup continuum
 * @brief Header for scaling commands sent from orchestrator to backend.
 *
 * ScalingCommandHeader carries a target performance state index from
 * the EdgeOrchestrator's DeviceManager to a backend. It is multiplexed
 * on the same connection as task data using message type 5.
 *
 * Wire format (5 bytes):
 * - messageType: 1 byte (SCALING_COMMAND = 5)
 * - targetStateIdx: 4 bytes (uint32_t, network byte order)
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
    static constexpr uint32_t SERIALIZED_SIZE = 5;

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ScalingCommandHeader();
    ~ScalingCommandHeader() override;

    /**
     * @brief Get the message type.
     * @return The message type byte.
     */
    uint8_t GetMessageType() const;

    /**
     * @brief Set the message type.
     * @param type The message type byte.
     */
    void SetMessageType(uint8_t type);

    /**
     * @brief Get the target performance state index.
     * @return The target state index.
     */
    uint32_t GetTargetStateIdx() const;

    /**
     * @brief Set the target performance state index.
     * @param idx The target state index.
     */
    void SetTargetStateIdx(uint32_t idx);

    // Header interface
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

  private:
    uint8_t m_messageType{SCALING_COMMAND}; //!< Message type (always 5)
    uint32_t m_targetStateIdx{0};           //!< Target performance state index
};

} // namespace ns3

#endif // SCALING_COMMAND_HEADER_H
