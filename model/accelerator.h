/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ACCELERATOR_H
#define ACCELERATOR_H

#include "task.h"

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class Node;

/**
 * @ingroup distributed
 * @brief Abstract base class for computational accelerators.
 *
 * Accelerator defines the interface for hardware that processes computational
 * tasks. Concrete implementations include GpuAccelerator, and could include
 * ASIC, FPGA, or other specialized hardware accelerators.
 *
 * Example usage:
 * @code
 * // Create a concrete accelerator (e.g., GPU)
 * Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
 * gpu->SetAttribute("ProcessingModel", PointerValue(processingModel));
 * node->AggregateObject(gpu);
 *
 * // Connect to trace sources
 * gpu->TraceConnectWithoutContext("TaskStarted", MakeCallback(&OnStart));
 * gpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&OnComplete));
 *
 * // Submit a task
 * Ptr<ComputeTask> task = CreateObject<ComputeTask>();
 * task->SetComputeDemand(1e9);
 * task->SetInputSize(1e6);
 * task->SetOutputSize(1e6);
 * gpu->SubmitTask(task);
 * @endcode
 */
class Accelerator : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Default constructor.
     */
    Accelerator();

    /**
     * @brief Destructor.
     */
    ~Accelerator() override;

    /**
     * @brief Submit a task for execution.
     *
     * This is the core method that all accelerators must implement.
     * The implementation decides how to process the task (queueing,
     * scheduling, execution model, etc.).
     *
     * @param task The task to execute.
     */
    virtual void SubmitTask(Ptr<Task> task) = 0;

    /**
     * @brief Get the name of this accelerator type.
     *
     * @return A string identifying the accelerator (e.g., "GPU", "FPGA", "ASIC").
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Get the current queue length.
     *
     * Default implementation returns 0. Override in subclasses that
     * maintain a task queue.
     *
     * @return Number of tasks in queue (including currently executing).
     */
    virtual uint32_t GetQueueLength() const;

    /**
     * @brief Check if accelerator is currently busy.
     *
     * Default implementation returns false. Override in subclasses that
     * track execution state.
     *
     * @return True if executing a task.
     */
    virtual bool IsBusy() const;

    /**
     * @brief Get the node this accelerator is aggregated to.
     * @return Pointer to the node, or nullptr if not aggregated.
     */
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

    /**
     * @brief TracedCallback signature for task failure.
     * @param task The failed task.
     * @param reason Description of why the task failed.
     */
    typedef void (*TaskFailedTracedCallback)(Ptr<const Task> task, std::string reason);

  protected:
    void DoDispose() override;
    void NotifyNewAggregate() override;

    Ptr<Node> m_node; //!< Node this accelerator is aggregated to

    // Trace sources for subclasses to fire
    TracedCallback<Ptr<const Task>> m_taskStartedTrace;              //!< Task started
    TracedCallback<Ptr<const Task>, Time> m_taskCompletedTrace;      //!< Task completed
    TracedCallback<Ptr<const Task>, std::string> m_taskFailedTrace;  //!< Task failed
};

} // namespace ns3

#endif // ACCELERATOR_H
