/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef TASK_H
#define TASK_H

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Abstract base class representing a task to be executed on an accelerator.
 *
 * Task provides a common interface for all task types in the distributed computing
 * simulation framework. Concrete implementations (e.g., ComputeTask) define specific
 * task characteristics such as compute demand, I/O sizes, or inference parameters.
 *
 * All tasks have a unique identifier and arrival time metadata. Derived classes
 * must implement GetTaskId() and GetName() to identify the task and its type.
 */
class Task : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    Task();
    ~Task() override;

    /**
     * @brief Get the unique task identifier.
     * @return The task ID.
     *
     * Must be implemented by derived classes to provide task identification.
     */
    virtual uint64_t GetTaskId() const = 0;

    /**
     * @brief Get the task type name.
     * @return A string identifying the task type (e.g., "ComputeTask", "InferenceTask").
     *
     * Used for logging and for dispatching tasks to appropriate accelerators or headers.
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Get the task arrival time.
     * @return The arrival time.
     *
     * Default implementation returns the stored arrival time.
     */
    virtual Time GetArrivalTime() const;

    /**
     * @brief Set the task arrival time.
     * @param time The arrival time.
     *
     * Default implementation stores the arrival time in m_arrivalTime.
     */
    virtual void SetArrivalTime(Time time);

  protected:
    void DoDispose() override;

    Time m_arrivalTime{Seconds(0)}; //!< Time when task arrived
};

} // namespace ns3

#endif // TASK_H
