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

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Abstract base class for task-based protocol headers.
 *
 * TaskHeader defines the minimal interface for headers used in
 * distributed computing scenarios. It provides:
 * - Message type identification (request vs response)
 * - Task identification for routing and correlation
 * - Payload size calculation for TCP stream reassembly
 *
 * Concrete implementations (e.g., SimpleTaskHeader) add task-specific
 * fields like compute demand, I/O sizes, or inference parameters.
 *
 * Implementations MUST serialize messageType (1 byte) followed by taskId
 * (8 bytes, network byte order) as the first 9 bytes. This common prefix
 * enables the EdgeOrchestrator to peek the taskId for response routing
 * without knowing the concrete header type.
 *
 * OffloadServer and EdgeOrchestrator use this interface to route messages
 * without inspecting task-specific fields.
 *
 * Example usage:
 * @code
 * // Create a concrete header
 * SimpleTaskHeader concrete;
 * concrete.SetMessageType(TaskHeader::TASK_REQUEST);
 * concrete.SetTaskId(42);
 *
 * // Use via base class reference for routing
 * const TaskHeader& base = concrete;
 * if (base.IsRequest())
 * {
 *     uint64_t taskId = base.GetTaskId();
 *     // Route request based on taskId...
 * }
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
        TASK_REQUEST = 0, //!< Request message (client to server)
        TASK_RESPONSE = 1 //!< Response message (server to client)
    };

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
     * @return True if message type equals TASK_REQUEST.
     */
    bool IsRequest() const;

    /**
     * @brief Check if this is a response message.
     * @return True if message type equals TASK_RESPONSE.
     */
    bool IsResponse() const;
};

} // namespace ns3

#endif // TASK_HEADER_H
