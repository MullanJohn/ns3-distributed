/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef OFFLOAD_HEADER_H
#define OFFLOAD_HEADER_H

#include "ns3/header.h"

#include <ostream>
#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Header for task offload packets.
 *
 * This header serializes task metadata for transmission between
 * offload clients and servers. It includes the task identifier,
 * compute demand, and input/output data sizes.
 */
class OffloadHeader : public Header
{
  public:
    /**
     * @brief Serialized size of the header in bytes.
     *
     * The header consists of:
     * - messageType: 1 byte
     * - taskId: 8 bytes
     * - computeDemand: 8 bytes
     * - inputSize: 8 bytes
     * - outputSize: 8 bytes
     */
    static constexpr uint32_t SERIALIZED_SIZE = 33;

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    OffloadHeader();
    ~OffloadHeader() override;

    /**
     * @brief Message types for offload protocol.
     */
    enum MessageType
    {
        TASK_REQUEST,  //!< Client sending task to server
        TASK_RESPONSE  //!< Server returning result to client
    };

    /**
     * @brief Set the message type.
     * @param messageType The message type.
     */
    void SetMessageType(MessageType messageType);

    /**
     * @brief Get the message type.
     * @return The message type.
     */
    MessageType GetMessageType() const;

    /**
     * @brief Set the task identifier.
     * @param taskId The unique task ID.
     */
    void SetTaskId(uint64_t taskId);

    /**
     * @brief Get the task identifier.
     * @return The task ID.
     */
    uint64_t GetTaskId() const;

    /**
     * @brief Set the compute demand in FLOPS.
     * @param computeDemand The compute demand.
     */
    void SetComputeDemand(double computeDemand);

    /**
     * @brief Get the compute demand in FLOPS.
     * @return The compute demand.
     */
    double GetComputeDemand() const;

    /**
     * @brief Set the input data size in bytes.
     * @param inputSize The input size.
     */
    void SetInputSize(uint64_t inputSize);

    /**
     * @brief Get the input data size in bytes.
     * @return The input size.
     */
    uint64_t GetInputSize() const;

    /**
     * @brief Set the output data size in bytes.
     * @param outputSize The output size.
     */
    void SetOutputSize(uint64_t outputSize);

    /**
     * @brief Get the output data size in bytes.
     * @return The output size.
     */
    uint64_t GetOutputSize() const;

    /**
     * @brief Get a string representation of the header.
     * @return String representation.
     */
    std::string ToString() const;

    // Inherited from Header
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

  private:
    MessageType m_messageType;  //!< Message type (request/response)
    uint64_t m_taskId;          //!< Unique task identifier
    double m_computeDemand;     //!< Compute demand in FLOPS
    uint64_t m_inputSize;       //!< Input data size in bytes
    uint64_t m_outputSize;      //!< Output data size in bytes
};

} // namespace ns3

#endif // OFFLOAD_HEADER_H
