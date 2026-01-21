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

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // SIMPLE_TASK_H
