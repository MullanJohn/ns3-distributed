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

    /**
     * @brief Get the task type name.
     * @return "SimpleTask"
     */
    std::string GetName() const override;

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
     * @brief Create a SimpleTask from a packet containing SimpleTaskHeader.
     *
     * Static factory method for deserialization. The packet should have
     * a SimpleTaskHeader at the front.
     *
     * @param packet The packet to deserialize.
     * @return A new SimpleTask populated from the header.
     */
    static Ptr<SimpleTask> Deserialize(Ptr<Packet> packet);

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // SIMPLE_TASK_H
