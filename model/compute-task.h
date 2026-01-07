/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef COMPUTE_TASK_H
#define COMPUTE_TASK_H

#include "task.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Concrete task representing a computational workload for accelerators.
 *
 * ComputeTask extends the Task interface with compute-specific attributes:
 * - Compute demand in FLOPS (floating-point operations)
 * - Input data size in bytes (data to transfer to accelerator)
 * - Output data size in bytes (results to transfer back)
 *
 * This task type is designed for GPU and similar compute accelerators that
 * process workloads characterized by compute intensity and I/O transfer requirements.
 */
class ComputeTask : public Task
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ComputeTask();
    ~ComputeTask() override;

    /**
     * @brief Get the unique task identifier.
     * @return The task ID.
     */
    uint64_t GetTaskId() const override;

    /**
     * @brief Get the task type name.
     * @return "ComputeTask"
     */
    std::string GetName() const override;

    /**
     * @brief Set the unique task identifier.
     * @param id The task ID.
     */
    void SetTaskId(uint64_t id);

    /**
     * @brief Set the compute demand in FLOPS.
     * @param flops The compute demand.
     */
    void SetComputeDemand(double flops);

    /**
     * @brief Get the compute demand in FLOPS.
     * @return The compute demand.
     */
    double GetComputeDemand() const;

    /**
     * @brief Set the input data size in bytes.
     * @param bytes The input size.
     */
    void SetInputSize(uint64_t bytes);

    /**
     * @brief Get the input data size in bytes.
     * @return The input size.
     */
    uint64_t GetInputSize() const;

    /**
     * @brief Set the output data size in bytes.
     * @param bytes The output size.
     */
    void SetOutputSize(uint64_t bytes);

    /**
     * @brief Get the output data size in bytes.
     * @return The output size.
     */
    uint64_t GetOutputSize() const;

  protected:
    void DoDispose() override;

  private:
    uint64_t m_taskId;      //!< Unique task identifier
    double m_computeDemand; //!< Compute demand in FLOPS
    uint64_t m_inputSize;   //!< Input data size in bytes
    uint64_t m_outputSize;  //!< Output data size in bytes
};

} // namespace ns3

#endif // COMPUTE_TASK_H
