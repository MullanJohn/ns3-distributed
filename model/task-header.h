/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef TASK_HEADER_H
#define TASK_HEADER_H

#include "ns3/header.h"

#include <cstdint>
#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Header for task offload packets.
 *
 * TaskHeader serializes task metadata for transmission between
 * clients and backends. It provides:
 * - Message type identification (request vs response)
 * - Task identification for routing and correlation
 * - Payload size calculation for TCP stream reassembly
 *
 * Implementations MUST serialize messageType (1 byte) followed by taskId
 * (8 bytes, network byte order) as the first 9 bytes. This common prefix
 * enables the EdgeOrchestrator to peek the taskId for response routing
 * without knowing the concrete header type.
 *
 * Example usage:
 * @code
 * TaskHeader header;
 * header.SetMessageType(TaskHeader::TASK_REQUEST);
 * header.SetTaskId(42);
 * header.SetComputeDemand(1e12);
 * header.SetInputSize(1024);
 *
 * Ptr<Packet> packet = Create<Packet>();
 * packet->AddHeader(header);
 * @endcode
 */
class TaskHeader : public Header
{
  public:
    /**
     * @brief Message types for task-based protocols.
     */
    enum MessageType
    {
        TASK_REQUEST = 0, //!< Request message
        TASK_RESPONSE = 1 //!< Response message
    };

    /**
     * @brief Fixed size for accelerator type string.
     */
    static constexpr uint32_t ACCEL_TYPE_SIZE = 16;

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
     * - priority: 4 bytes
     * - acceleratorType: 16 bytes
     */
    static constexpr uint32_t SERIALIZED_SIZE = 61;

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Default constructor.
     */
    TaskHeader();

    /**
     * @brief Virtual destructor.
     */
    ~TaskHeader() override;

    /**
     * @brief Get the message type.
     *
     * Returns TASK_REQUEST or TASK_RESPONSE for standard messages.
     * Derived classes may define additional message types beyond these.
     *
     * @return The message type.
     */
    MessageType GetMessageType() const;

    /**
     * @brief Set the message type.
     * @param messageType The message type.
     */
    void SetMessageType(MessageType messageType);

    /**
     * @brief Get the task identifier.
     *
     * The task ID uniquely identifies a task for routing responses
     * back to the originating client.
     *
     * @return The unique task ID.
     */
    uint64_t GetTaskId() const;

    /**
     * @brief Set the task identifier.
     * @param taskId The unique task ID.
     */
    void SetTaskId(uint64_t taskId);

    /**
     * @brief Get the payload size for a request message.
     *
     * Used for TCP stream reassembly. The payload size is the number
     * of bytes following the header in a request message.
     *
     * @return Payload size in bytes.
     */
    uint64_t GetRequestPayloadSize() const;

    /**
     * @brief Get the payload size for a response message.
     *
     * Used for TCP stream reassembly. The payload size is the number
     * of bytes following the header in a response message.
     *
     * @return Payload size in bytes.
     */
    uint64_t GetResponsePayloadSize() const;

    /**
     * @brief Check if this is a request message.
     * @return True if message type equals TASK_REQUEST.
     */
    bool IsRequest() const;

    /**
     * @brief Check if this is a response message.
     * @return True if message type equals TASK_RESPONSE.
     */
    bool IsResponse() const;

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
     * @brief Get the task priority.
     * @return The priority value. Higher value = higher priority.
     */
    uint32_t GetPriority() const;

    /**
     * @brief Set the task priority.
     * @param priority The priority value. Higher value = higher priority.
     */
    void SetPriority(uint32_t priority);

    /**
     * @brief Get the required accelerator type.
     * @return The accelerator type string (e.g., "GPU", "TPU"). Empty means any.
     */
    std::string GetAcceleratorType() const;

    /**
     * @brief Set the required accelerator type.
     * @param type The accelerator type (max 16 chars, will be truncated if longer).
     */
    void SetAcceleratorType(const std::string& type);

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

  protected:
    MessageType m_messageType{TASK_REQUEST}; //!< Message type (request/response)
    uint64_t m_taskId{0};                   //!< Unique task identifier
    double m_computeDemand{0.0};            //!< Compute demand in FLOPS
    uint64_t m_inputSize{0};                //!< Input data size in bytes
    uint64_t m_outputSize{0};               //!< Output data size in bytes
    int64_t m_deadlineNs{-1};               //!< Task deadline in nanoseconds (-1 = no deadline)
    uint32_t m_priority{0};                 //!< Task priority (higher = higher priority)
    std::string m_acceleratorType;          //!< Required accelerator type (empty = any)
};

} // namespace ns3

#endif // TASK_HEADER_H
