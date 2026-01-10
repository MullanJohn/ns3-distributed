/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef QUEUE_SCHEDULER_H
#define QUEUE_SCHEDULER_H

#include "task.h"

#include "ns3/object.h"
#include "ns3/ptr.h"

#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Abstract base class for task queue scheduling policies.
 *
 * QueueScheduler defines the interface for managing task queues within
 * accelerators. Subclasses implement different scheduling algorithms
 * such as FIFO, priority queues, or batching strategies.
 *
 * This abstraction allows accelerators to use different queue management
 * policies without changing their implementation, enabling experiments
 * with various scheduling strategies for distributed computing workloads.
 *
 * Example usage:
 * @code
 * Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();
 *
 * Ptr<ComputeTask> task1 = CreateObject<ComputeTask>();
 * Ptr<ComputeTask> task2 = CreateObject<ComputeTask>();
 *
 * scheduler->Enqueue(task1);
 * scheduler->Enqueue(task2);
 *
 * Ptr<Task> next = scheduler->Dequeue();  // Returns task1 (FIFO order)
 * @endcode
 */
class QueueScheduler : public Object
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
    QueueScheduler();

    /**
     * @brief Destructor.
     */
    ~QueueScheduler() override;

    /**
     * @brief Add a task to the queue.
     *
     * The task is added according to the scheduling policy implemented
     * by the subclass (e.g., at the back for FIFO, by priority for
     * priority queues).
     *
     * @param task The task to enqueue.
     */
    virtual void Enqueue(Ptr<Task> task) = 0;

    /**
     * @brief Remove and return the next task from the queue.
     *
     * Returns the next task according to the scheduling policy.
     * For FIFO, this is the oldest task; for priority queues,
     * this is the highest priority task.
     *
     * @return The next task, or nullptr if the queue is empty.
     */
    virtual Ptr<Task> Dequeue() = 0;

    /**
     * @brief Return the next task without removing it.
     *
     * @return The next task, or nullptr if the queue is empty.
     */
    virtual Ptr<Task> Peek() const = 0;

    /**
     * @brief Check if the queue is empty.
     *
     * @return True if the queue contains no tasks.
     */
    virtual bool IsEmpty() const = 0;

    /**
     * @brief Get the number of tasks in the queue.
     *
     * @return The number of queued tasks.
     */
    virtual uint32_t GetLength() const = 0;

    /**
     * @brief Get the name of this scheduling algorithm.
     *
     * @return A string identifying the scheduler (e.g., "FIFO", "Batching").
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Remove all tasks from the queue.
     *
     * Used during cleanup to release all task references.
     */
    virtual void Clear() = 0;

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // QUEUE_SCHEDULER_H
