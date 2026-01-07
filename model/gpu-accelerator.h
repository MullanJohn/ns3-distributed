/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef GPU_ACCELERATOR_H
#define GPU_ACCELERATOR_H

#include "accelerator.h"
#include "compute-task.h"

#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/traced-value.h"

#include <queue>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief GPU accelerator for processing computational tasks.
 *
 * GpuAccelerator models a GPU processing unit with configurable compute
 * rate and memory bandwidth. Tasks are processed using a three-phase model:
 * input transfer, compute, output transfer.
 *
 * This is a concrete implementation of the Accelerator interface.
 */
class GpuAccelerator : public Accelerator
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    GpuAccelerator();
    ~GpuAccelerator() override;

    // Accelerator interface implementation
    void SubmitTask(Ptr<Task> task) override;
    std::string GetName() const override;
    uint32_t GetQueueLength() const override;
    bool IsBusy() const override;

    /**
     * @brief Get compute rate in FLOPS.
     * @return The compute rate.
     */
    double GetComputeRate() const;

    /**
     * @brief Get memory bandwidth in bytes/sec.
     * @return The memory bandwidth.
     */
    double GetMemoryBandwidth() const;

  protected:
    void DoDispose() override;

  private:
    /**
     * @brief Start processing the next task from queue.
     */
    void StartNextTask();

    /**
     * @brief Called when input transfer completes.
     */
    void InputTransferComplete();

    /**
     * @brief Called when compute phase completes.
     */
    void ComputeComplete();

    /**
     * @brief Called when output transfer completes (task done).
     */
    void OutputTransferComplete();

    // GPU-specific attributes
    double m_computeRate;     //!< Compute rate in FLOPS
    double m_memoryBandwidth; //!< Memory bandwidth in bytes/sec

    // State
    std::queue<Ptr<ComputeTask>> m_taskQueue; //!< Queue of pending compute tasks
    Ptr<ComputeTask> m_currentTask;           //!< Currently executing compute task
    bool m_busy;                       //!< True if processing a task
    EventId m_currentEvent;            //!< Current scheduled event
    Time m_taskStartTime;              //!< When current task started

    // Statistics
    uint64_t m_tasksCompleted; //!< Number of completed tasks

    // Traced values
    TracedValue<uint32_t> m_queueLength; //!< Current queue length
};

} // namespace ns3

#endif // GPU_ACCELERATOR_H
