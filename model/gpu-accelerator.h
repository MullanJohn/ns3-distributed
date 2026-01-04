/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef GPU_ACCELERATOR_H
#define GPU_ACCELERATOR_H

#include "task.h"

#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/traced-value.h"

#include <queue>

namespace ns3
{

class Node;

class GpuAccelerator : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    GpuAccelerator();
    ~GpuAccelerator() override;

    /**
     * @brief Submit a task for execution.
     * @param task The task to execute.
     */
    void SubmitTask(Ptr<Task> task);

    /**
     * @brief Get the current queue length.
     * @return Number of tasks in queue (including currently executing).
     */
    uint32_t GetQueueLength() const;

    /**
     * @brief Check if accelerator is currently busy.
     * @return True if executing a task.
     */
    bool IsBusy() const;

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

    Ptr<Node> GetNode() const;

    /**
     * @brief TracedCallback signature for task events.
     * @param task The task.
     */
    typedef void (*TaskTracedCallback)(Ptr<const Task> task);

    /**
     * @brief TracedCallback signature for task completion.
     * @param task The completed task.
     * @param duration The total processing duration.
     */
    typedef void (*TaskCompletedTracedCallback)(Ptr<const Task> task, Time duration);

  protected:
    void DoDispose() override;
    void NotifyNewAggregate() override;

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

    // Attributes

    Ptr<Node> m_node;

    double m_computeRate;     //!< Compute rate in FLOPS
    double m_memoryBandwidth; //!< Memory bandwidth in bytes/sec

    // State
    std::queue<Ptr<Task>> m_taskQueue; //!< Queue of pending tasks
    Ptr<Task> m_currentTask;           //!< Currently executing task
    bool m_busy;                       //!< True if processing a task
    EventId m_currentEvent;            //!< Current scheduled event
    Time m_taskStartTime;              //!< When current task started

    // Statistics
    uint64_t m_tasksCompleted; //!< Number of completed tasks

    // Traced values
    TracedValue<uint32_t> m_queueLength; //!< Current queue length

    // Trace sources
    TracedCallback<Ptr<const Task>> m_taskStartedTrace;         //!< Task started
    TracedCallback<Ptr<const Task>, Time> m_taskCompletedTrace; //!< Task completed
};

} // namespace ns3

#endif // GPU_ACCELERATOR_H
