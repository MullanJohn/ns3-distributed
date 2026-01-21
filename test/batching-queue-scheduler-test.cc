/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/batching-queue-scheduler.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test BatchingQueueScheduler single dequeue (standard interface)
 */
class BatchingQueueSchedulerSingleTestCase : public TestCase
{
  public:
    BatchingQueueSchedulerSingleTestCase()
        : TestCase("Test BatchingQueueScheduler single dequeue")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<BatchingQueueScheduler> scheduler = CreateObject<BatchingQueueScheduler>();

        NS_TEST_ASSERT_MSG_EQ(scheduler->GetName(), "Batching", "Name should be Batching");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetMaxBatchSize(), 1, "Default batch size should be 1");

        // Create and enqueue tasks
        Ptr<Task> task1 = CreateObject<SimpleTask>();
        task1->SetTaskId(1);
        Ptr<Task> task2 = CreateObject<SimpleTask>();
        task2->SetTaskId(2);

        scheduler->Enqueue(task1);
        scheduler->Enqueue(task2);

        // Standard Dequeue should work like FIFO
        Ptr<Task> dequeued = scheduler->Dequeue();
        NS_TEST_ASSERT_MSG_EQ(dequeued->GetTaskId(), 1, "First dequeue should return task 1");

        dequeued = scheduler->Dequeue();
        NS_TEST_ASSERT_MSG_EQ(dequeued->GetTaskId(), 2, "Second dequeue should return task 2");

        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), true, "Queue should be empty");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test BatchingQueueScheduler batch dequeue
 */
class BatchingQueueSchedulerBatchTestCase : public TestCase
{
  public:
    BatchingQueueSchedulerBatchTestCase()
        : TestCase("Test BatchingQueueScheduler batch dequeue")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<BatchingQueueScheduler> scheduler = CreateObject<BatchingQueueScheduler>();
        scheduler->SetAttribute("MaxBatchSize", UintegerValue(4));

        NS_TEST_ASSERT_MSG_EQ(scheduler->GetMaxBatchSize(), 4, "Batch size should be 4");

        // Enqueue 6 tasks
        for (uint32_t i = 1; i <= 6; i++)
        {
            Ptr<Task> task = CreateObject<SimpleTask>();
            task->SetTaskId(i);
            scheduler->Enqueue(task);
        }

        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 6, "Should have 6 tasks");

        // Dequeue batch of 4
        std::vector<Ptr<Task>> batch = scheduler->DequeueBatch();
        NS_TEST_ASSERT_MSG_EQ(batch.size(), 4, "First batch should have 4 tasks");
        NS_TEST_ASSERT_MSG_EQ(batch[0]->GetTaskId(), 1, "Batch should maintain FIFO order");
        NS_TEST_ASSERT_MSG_EQ(batch[3]->GetTaskId(), 4, "Last task in batch should be 4");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 2, "Should have 2 tasks remaining");

        // Dequeue remaining tasks
        batch = scheduler->DequeueBatch();
        NS_TEST_ASSERT_MSG_EQ(batch.size(), 2, "Second batch should have 2 tasks");
        NS_TEST_ASSERT_MSG_EQ(batch[0]->GetTaskId(), 5, "First task should be 5");
        NS_TEST_ASSERT_MSG_EQ(batch[1]->GetTaskId(), 6, "Second task should be 6");

        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), true, "Queue should be empty");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test BatchingQueueScheduler partial batch (queue smaller than batch size)
 */
class BatchingQueueSchedulerPartialBatchTestCase : public TestCase
{
  public:
    BatchingQueueSchedulerPartialBatchTestCase()
        : TestCase("Test BatchingQueueScheduler partial batch")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<BatchingQueueScheduler> scheduler = CreateObject<BatchingQueueScheduler>();
        scheduler->SetAttribute("MaxBatchSize", UintegerValue(10));

        // Enqueue only 3 tasks
        for (uint32_t i = 1; i <= 3; i++)
        {
            Ptr<Task> task = CreateObject<SimpleTask>();
            task->SetTaskId(i);
            scheduler->Enqueue(task);
        }

        // DequeueBatch should return all 3 (not 10)
        std::vector<Ptr<Task>> batch = scheduler->DequeueBatch();
        NS_TEST_ASSERT_MSG_EQ(batch.size(), 3, "Partial batch should have 3 tasks");

        // DequeueBatch on empty queue should return empty vector
        batch = scheduler->DequeueBatch();
        NS_TEST_ASSERT_MSG_EQ(batch.size(), 0, "Batch from empty queue should be empty");

        // DequeueBatch with maxBatch=0 should return empty
        scheduler->Enqueue(CreateObject<SimpleTask>());
        batch = scheduler->DequeueBatch(0);
        NS_TEST_ASSERT_MSG_EQ(batch.size(), 0, "DequeueBatch(0) should return empty");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 1, "Task should still be in queue");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateBatchingQueueSchedulerSingleTestCase()
{
    return new BatchingQueueSchedulerSingleTestCase;
}

TestCase*
CreateBatchingQueueSchedulerBatchTestCase()
{
    return new BatchingQueueSchedulerBatchTestCase;
}

TestCase*
CreateBatchingQueueSchedulerPartialBatchTestCase()
{
    return new BatchingQueueSchedulerPartialBatchTestCase;
}

} // namespace ns3
