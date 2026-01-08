/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DISTRIBUTED_HEADER_H
#define DISTRIBUTED_HEADER_H

#include "ns3/header.h"

#include <cstdint>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Abstract base class for distributed computing protocol headers.
 *
 * DistributedHeader defines the minimal interface for headers used in
 * distributed computing scenarios.
 *
 * Concrete implementations add task-specific fields.
 *
 * LoadBalancer and NodeScheduler use this interface to route messages
 * without inspecting task-specific fields.
 *
 * Example usage:
 * @code
 * // Create a concrete header
 * OffloadHeader concrete;
 * concrete.SetMessageType(DistributedHeader::DISTRIBUTED_REQUEST);
 * concrete.SetTaskId(42);
 *
 * // Use via base class reference for routing
 * const DistributedHeader& base = concrete;
 * if (base.IsRequest())
 * {
 *     uint64_t taskId = base.GetTaskId();
 *     // Route request based on taskId...
 * }
 * @endcode
 */
class DistributedHeader : public Header
{
  public:
    /**
     * @brief Message types for distributed computing protocols.
     *
     * Derived classes may define additional message type aliases
     * for semantic clarity (e.g., TASK_REQUEST = DISTRIBUTED_REQUEST).
     */
    enum MessageType
    {
        DISTRIBUTED_REQUEST = 0, //!< Generic request message (client to server)
        DISTRIBUTED_RESPONSE = 1 //!< Generic response message (server to client)
    };

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Default constructor.
     */
    DistributedHeader();

    /**
     * @brief Virtual destructor.
     */
    ~DistributedHeader() override;

    /**
     * @brief Get the message type.
     *
     * Returns DISTRIBUTED_REQUEST or DISTRIBUTED_RESPONSE for standard messages.
     * Derived classes may define additional message types beyond these.
     *
     * @return The message type.
     */
    virtual MessageType GetMessageType() const = 0;

    /**
     * @brief Set the message type.
     * @param messageType The message type.
     */
    virtual void SetMessageType(MessageType messageType) = 0;

    /**
     * @brief Get the task identifier.
     *
     * The task ID uniquely identifies a task for routing responses
     * back to the originating client.
     *
     * @return The unique task ID.
     */
    virtual uint64_t GetTaskId() const = 0;

    /**
     * @brief Set the task identifier.
     * @param taskId The unique task ID.
     */
    virtual void SetTaskId(uint64_t taskId) = 0;

    /**
     * @brief Get the payload size for a request message.
     *
     * Used for TCP stream reassembly. The payload size is the number
     * of bytes following the header in a request message.
     *
     * @return Payload size in bytes.
     */
    virtual uint64_t GetRequestPayloadSize() const = 0;

    /**
     * @brief Get the payload size for a response message.
     *
     * Used for TCP stream reassembly. The payload size is the number
     * of bytes following the header in a response message.
     *
     * @return Payload size in bytes.
     */
    virtual uint64_t GetResponsePayloadSize() const = 0;

    /**
     * @brief Check if this is a request message.
     * @return True if message type equals DISTRIBUTED_REQUEST.
     */
    bool IsRequest() const;

    /**
     * @brief Check if this is a response message.
     * @return True if message type equals DISTRIBUTED_RESPONSE.
     */
    bool IsResponse() const;
};

} // namespace ns3

#endif // DISTRIBUTED_HEADER_H
