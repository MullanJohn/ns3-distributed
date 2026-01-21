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
#include "processing-model.h"
#include "queue-scheduler.h"
#include "task.h"

#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/traced-value.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief GPU accelerator for processing computational tasks.
 *
 * GpuAccelerator models a GPU processing unit. Task processing time
 * is determined by the attached ProcessingModel, which must be set
 * before tasks can be submitted.
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
    double GetVoltage() const override;
    double GetFrequency() const override;

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
     * @brief Called when ProcessingModel completes task processing.
     */
    void ProcessingComplete();

    // GPU-specific attributes
    double m_computeRate;                   //!< Compute rate in FLOPS
    double m_memoryBandwidth;               //!< Memory bandwidth in bytes/sec
    double m_frequency;                     //!< Operating frequency in Hz
    double m_voltage;                       //!< Operating voltage in Volts
    Ptr<ProcessingModel> m_processingModel; //!< Processing model for timing calculation
    Ptr<QueueScheduler> m_queueScheduler;   //!< Queue scheduler for task management

    // State
    Ptr<Task> m_currentTask; //!< Currently executing task
    bool m_busy;             //!< True if processing a task
    EventId m_currentEvent;  //!< Current scheduled event
    Time m_taskStartTime;    //!< When current task started

    // Statistics
    uint64_t m_tasksCompleted; //!< Number of completed tasks

    // Traced values
    TracedValue<uint32_t> m_queueLength; //!< Current queue length
};

} // namespace ns3

#endif // GPU_ACCELERATOR_H
