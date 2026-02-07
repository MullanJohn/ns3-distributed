/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef SIMPLE_TASK_H
#define SIMPLE_TASK_H

#include "task.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Concrete task implementation using the common Task fields.
 *
 * SimpleTask is the basic concrete implementation of Task, providing
 * a task with compute demand, input size, and output size. It uses
 * the fields defined in the Task base class.
 */
class SimpleTask : public Task
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    SimpleTask();
    ~SimpleTask() override;

    static constexpr uint8_t TASK_TYPE = 0; //!< Task type identifier for SimpleTask

    /**
     * @brief Get the task type name.
     * @return "SimpleTask"
     */
    std::string GetName() const override;

    /**
     * @brief Get the task type identifier.
     * @return SimpleTask::TASK_TYPE (0)
     */
    uint8_t GetTaskType() const override;

    /**
     * @brief Serialize this task to a packet using SimpleTaskHeader.
     *
     * @param isResponse true for response (output payload), false for request (input payload).
     * @return A packet with SimpleTaskHeader and payload.
     */
    Ptr<Packet> Serialize(bool isResponse) const override;

    /**
     * @brief Get SimpleTaskHeader size.
     * @return SimpleTaskHeader::SERIALIZED_SIZE
     */
    uint32_t GetSerializedHeaderSize() const override;

    /**
     * @brief Deserialize a SimpleTask from a packet buffer.
     *
     * Stream-aware deserialization that handles message boundary detection.
     * Peeks at the header to determine total message size, and only extracts
     * data if a complete message is available.
     *
     * @param packet The packet buffer (may contain multiple messages or partial data).
     * @param consumedBytes Output: bytes consumed from packet (0 if not enough data).
     * @return A new SimpleTask, or nullptr if not enough data for complete message.
     */
    static Ptr<Task> Deserialize(Ptr<Packet> packet, uint64_t& consumedBytes);

    /**
     * @brief Deserialize a SimpleTask from header bytes only (no payload).
     *
     * Creates a task with all metadata fields populated but does not
     * expect or consume any payload bytes. Used for DAG admission
     * requests where only task metadata is sent.
     *
     * @param packet The packet buffer containing at least one header.
     * @param consumedBytes Output: bytes consumed (header size, or 0 if insufficient data).
     * @return A new SimpleTask, or nullptr if not enough data for header.
     */
    static Ptr<Task> DeserializeHeader(Ptr<Packet> packet, uint64_t& consumedBytes);

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // SIMPLE_TASK_H
