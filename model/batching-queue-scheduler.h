/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef BATCHING_QUEUE_SCHEDULER_H
#define BATCHING_QUEUE_SCHEDULER_H

#include "queue-scheduler.h"

#include <queue>
#include <vector>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Queue scheduler with batch dequeue capability.
 *
 * BatchingQueueScheduler extends the basic FIFO queue with the ability
 * to dequeue multiple tasks at once as a batch. This is useful for
 * accelerators that can process multiple tasks simultaneously or
 * benefit from batched execution (e.g., batched inference on GPUs).
 *
 * The scheduler maintains FIFO ordering. Standard Dequeue() returns
 * a single task, while DequeueBatch() returns up to MaxBatchSize tasks.
 *
 * Example usage:
 * @code
 * Ptr<BatchingQueueScheduler> scheduler = CreateObject<BatchingQueueScheduler>();
 * scheduler->SetAttribute("MaxBatchSize", UintegerValue(4));
 *
 * // Enqueue 6 tasks
 * for (int i = 0; i < 6; i++) {
 *     scheduler->Enqueue(CreateObject<ComputeTask>());
 * }
 *
 * // Dequeue batch of up to 4 tasks
 * std::vector<Ptr<Task>> batch = scheduler->DequeueBatch();  // Returns 4 tasks
 * batch = scheduler->DequeueBatch();  // Returns remaining 2 tasks
 * @endcode
 */
class BatchingQueueScheduler : public QueueScheduler
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
    BatchingQueueScheduler();

    /**
     * @brief Destructor.
     */
    ~BatchingQueueScheduler() override;

    /**
     * @brief Dequeue a batch of tasks.
     *
     * Returns up to MaxBatchSize tasks from the queue in FIFO order.
     * If the queue has fewer tasks than MaxBatchSize, returns all
     * available tasks.
     *
     * @return Vector of tasks (may be empty if queue is empty).
     */
    std::vector<Ptr<Task>> DequeueBatch();

    /**
     * @brief Dequeue a batch of tasks with specified maximum.
     *
     * @param maxBatch Maximum number of tasks to dequeue.
     * @return Vector of tasks (may be empty or smaller than maxBatch).
     */
    std::vector<Ptr<Task>> DequeueBatch(uint32_t maxBatch);

    /**
     * @brief Get the maximum batch size.
     * @return Maximum number of tasks returned by DequeueBatch().
     */
    uint32_t GetMaxBatchSize() const;

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
    std::queue<Ptr<Task>> m_queue; //!< Internal FIFO queue
    uint32_t m_maxBatchSize;       //!< Maximum batch size for DequeueBatch()
};

} // namespace ns3

#endif // BATCHING_QUEUE_SCHEDULER_H
