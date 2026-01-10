/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef FIFO_QUEUE_SCHEDULER_H
#define FIFO_QUEUE_SCHEDULER_H

#include "queue-scheduler.h"

#include <queue>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief FIFO (First-In-First-Out) task queue scheduler.
 *
 * FifoQueueScheduler processes tasks in strict first-in-first-out order.
 * Tasks are dequeued in the same order they were enqueued, making this
 * the simplest and most predictable scheduling policy.
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
 * Ptr<Task> next = scheduler->Dequeue();  // Returns task1
 * next = scheduler->Dequeue();             // Returns task2
 * @endcode
 */
class FifoQueueScheduler : public QueueScheduler
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
    FifoQueueScheduler();

    /**
     * @brief Destructor.
     */
    ~FifoQueueScheduler() override;

    // QueueScheduler interface
    void Enqueue(Ptr<Task> task) override;
    Ptr<Task> Dequeue() override;
    Ptr<Task> Peek() const override;
    bool IsEmpty() const override;
    uint32_t GetLength() const override;
    std::string GetName() const override;
    void Clear() override;

  protected:
    void DoDispose() override;

  private:
    std::queue<Ptr<Task>> m_queue;  //!< Internal FIFO queue
};

} // namespace ns3

#endif // FIFO_QUEUE_SCHEDULER_H
