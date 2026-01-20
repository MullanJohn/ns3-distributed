/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef SIMPLE_TASK_HEADER_H
#define SIMPLE_TASK_HEADER_H

#include "task-header.h"

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
 *
 * SimpleTaskHeader is a concrete implementation of TaskHeader
 * designed for compute offloading scenarios.
 */
class SimpleTaskHeader : public TaskHeader
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
     * - deadline: 8 bytes (int64_t nanoseconds, -1 = no deadline)
     */
    static constexpr uint32_t SERIALIZED_SIZE = 41;

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    SimpleTaskHeader();
    ~SimpleTaskHeader() override;

    // DistributedHeader interface implementation
    MessageType GetMessageType() const override;
    void SetMessageType(MessageType messageType) override;
    uint64_t GetTaskId() const override;
    void SetTaskId(uint64_t taskId) override;
    uint64_t GetRequestPayloadSize() const override;
    uint64_t GetResponsePayloadSize() const override;

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
     * @brief Check if the header has a deadline set.
     * @return true if deadline >= 0, false otherwise.
     */
    bool HasDeadline() const;

    /**
     * @brief Get the task deadline.
     * @return The deadline as nanoseconds. Returns -1 if no deadline.
     */
    int64_t GetDeadlineNs() const;

    /**
     * @brief Set the task deadline.
     * @param deadlineNs The deadline in nanoseconds. Use -1 for no deadline.
     */
    void SetDeadlineNs(int64_t deadlineNs);

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
    MessageType m_messageType; //!< Message type (request/response)
    uint64_t m_taskId;         //!< Unique task identifier
    double m_computeDemand;    //!< Compute demand in FLOPS
    uint64_t m_inputSize;      //!< Input data size in bytes
    uint64_t m_outputSize;     //!< Output data size in bytes
    int64_t m_deadlineNs;      //!< Task deadline in nanoseconds (-1 = no deadline)
};

} // namespace ns3

#endif // SIMPLE_TASK_HEADER_H
