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
 * simulation framework. All tasks have common fields for compute demand, I/O sizes,
 * and timing metadata. Derived classes must implement GetName() to identify the
 * task type.
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
     */
    virtual uint64_t GetTaskId() const;

    /**
     * @brief Set the unique task identifier.
     * @param id The task ID.
     */
    void SetTaskId(uint64_t id);

    /**
     * @brief Get the task type name.
     * @return A string identifying the task type (e.g., "SimpleTask", "InferenceTask").
     *
     * Used for logging and for dispatching tasks to appropriate accelerators or headers.
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Get the input data size in bytes.
     * @return The input size.
     */
    uint64_t GetInputSize() const;

    /**
     * @brief Set the input data size in bytes.
     * @param bytes The input size.
     */
    void SetInputSize(uint64_t bytes);

    /**
     * @brief Get the output data size in bytes.
     * @return The output size.
     */
    uint64_t GetOutputSize() const;

    /**
     * @brief Set the output data size in bytes.
     * @param bytes The output size.
     */
    void SetOutputSize(uint64_t bytes);

    /**
     * @brief Get the compute demand in FLOPS.
     * @return The compute demand.
     */
    double GetComputeDemand() const;

    /**
     * @brief Set the compute demand in FLOPS.
     * @param flops The compute demand.
     */
    void SetComputeDemand(double flops);

    /**
     * @brief Get the task arrival time.
     * @return The arrival time.
     */
    virtual Time GetArrivalTime() const;

    /**
     * @brief Set the task arrival time.
     * @param time The arrival time.
     */
    virtual void SetArrivalTime(Time time);

    /**
     * @brief Check if the task has a deadline.
     * @return true if deadline is set (>= 0), false otherwise.
     */
    bool HasDeadline() const;

    /**
     * @brief Get the task deadline.
     * @return The deadline time. Returns Time(-1) if no deadline.
     */
    Time GetDeadline() const;

    /**
     * @brief Set the task deadline.
     * @param deadline The deadline time. Use Time(-1) to clear.
     */
    void SetDeadline(Time deadline);

    /**
     * @brief Clear the task deadline.
     */
    void ClearDeadline();

  protected:
    void DoDispose() override;

    uint64_t m_taskId{0};       //!< Unique task identifier
    uint64_t m_inputSize{0};    //!< Input data size in bytes
    uint64_t m_outputSize{0};   //!< Output data size in bytes
    double m_computeDemand{0.0}; //!< Compute demand in FLOPS
    Time m_arrivalTime{Seconds(0)}; //!< Time when task arrived
    Time m_deadline{Time(-1)};  //!< Task deadline (-1 = no deadline)
};

} // namespace ns3

#endif // TASK_H
