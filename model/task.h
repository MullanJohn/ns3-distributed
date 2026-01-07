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

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Represents a computational task to be executed on an accelerator.
 *
 * A Task encapsulates the compute demand (in FLOPS), input/output data sizes,
 * and timing metadata for simulation of distributed computing workloads.
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
     * @brief Set the compute demand in FLOPS.
     * @param flops The compute demand.
     */
    void SetComputeDemand(double flops);

    /**
     * @brief Set the input data size in bytes.
     * @param bytes The input size.
     */
    void SetInputSize(uint64_t bytes);

    /**
     * @brief Set the output data size in bytes.
     * @param bytes The output size.
     */
    void SetOutputSize(uint64_t bytes);

    /**
     * @brief Set the task arrival time.
     * @param time The arrival time.
     */
    void SetArrivalTime(Time time);

    /**
     * @brief Set the unique task identifier.
     * @param id The task ID.
     */
    void SetTaskId(uint64_t id);

    /**
     * @brief Get the compute demand in FLOPS.
     * @return The compute demand.
     */
    double GetComputeDemand() const;

    /**
     * @brief Get the input data size in bytes.
     * @return The input size.
     */
    uint64_t GetInputSize() const;

    /**
     * @brief Get the output data size in bytes.
     * @return The output size.
     */
    uint64_t GetOutputSize() const;

    /**
     * @brief Get the task arrival time.
     * @return The arrival time.
     */
    Time GetArrivalTime() const;

    /**
     * @brief Get the unique task identifier.
     * @return The task ID.
     */
    uint64_t GetTaskId() const;

  protected:
    void DoDispose() override;

  private:
    double m_computeDemand; //!< Compute demand in FLOPS
    uint64_t m_inputSize;   //!< Input data size in bytes
    uint64_t m_outputSize;  //!< Output data size in bytes
    Time m_arrivalTime;     //!< Time when task arrived
    uint64_t m_taskId;      //!< Unique task identifier
};

} // namespace ns3

#endif // TASK_H
