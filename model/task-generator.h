/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef TASK_GENERATOR_H
#define TASK_GENERATOR_H

#include "gpu-accelerator.h"
#include "task.h"

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Application that generates computational tasks following Poisson arrivals.
 *
 * TaskGenerator creates Task objects at exponentially distributed intervals
 * and submits them to a target GpuAccelerator. Task parameters (compute demand,
 * input/output sizes) can also be randomized.
 */
class TaskGenerator : public Application
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    TaskGenerator();
    ~TaskGenerator() override;

    /**
     * @brief Set the target accelerator for task submission.
     * @param accelerator Pointer to the target GpuAccelerator.
     */
    void SetAccelerator(Ptr<GpuAccelerator> accelerator);

    /**
     * @brief Get the number of tasks generated so far.
     * @return The task count.
     */
    uint64_t GetTaskCount() const;

    /**
     * @brief TracedCallback signature for task generation.
     * @param task The generated task.
     */
    typedef void (*TaskTracedCallback)(Ptr<const Task> task);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * @brief Generate and submit a new task.
     */
    void GenerateTask();

    /**
     * @brief Schedule the next task generation.
     */
    void ScheduleNextTask();

    // Random variable streams
    Ptr<RandomVariableStream> m_interArrivalTime;  //!< Inter-arrival time RNG
    Ptr<RandomVariableStream> m_computeDemand;     //!< Compute demand RNG
    Ptr<RandomVariableStream> m_inputSize;         //!< Input size RNG
    Ptr<RandomVariableStream> m_outputSize;        //!< Output size RNG

    // Target accelerator
    Ptr<GpuAccelerator> m_accelerator;  //!< Target for task submission

    // State
    EventId m_generateEvent;  //!< Next generation event
    uint64_t m_taskCount;     //!< Number of tasks generated
    uint64_t m_maxTasks;      //!< Maximum tasks to generate (0 = unlimited)

    // Trace sources
    TracedCallback<Ptr<const Task>> m_taskGeneratedTrace;  //!< Task generated
};

} // namespace ns3

#endif // TASK_GENERATOR_H
