/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/compute-task.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test FifoQueueScheduler basic enqueue and dequeue operations
 */
class FifoQueueSchedulerEnqueueDequeueTestCase : public TestCase
{
  public:
    FifoQueueSchedulerEnqueueDequeueTestCase()
        : TestCase("Test FifoQueueScheduler enqueue and dequeue")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        // Test initial state
        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), true, "New scheduler should be empty");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 0, "New scheduler should have length 0");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetName(), "FIFO", "Name should be FIFO");

        // Create and enqueue tasks
        Ptr<ComputeTask> task1 = CreateObject<ComputeTask>();
        task1->SetTaskId(1);
        Ptr<ComputeTask> task2 = CreateObject<ComputeTask>();
        task2->SetTaskId(2);

        scheduler->Enqueue(task1);
        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), false, "Scheduler should not be empty");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 1, "Length should be 1");

        scheduler->Enqueue(task2);
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 2, "Length should be 2");

        // Dequeue and verify
        Ptr<Task> dequeued = scheduler->Dequeue();
        NS_TEST_ASSERT_MSG_EQ(dequeued->GetTaskId(), 1, "First dequeue should return task 1");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 1, "Length should be 1 after dequeue");

        dequeued = scheduler->Dequeue();
        NS_TEST_ASSERT_MSG_EQ(dequeued->GetTaskId(), 2, "Second dequeue should return task 2");
        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), true, "Scheduler should be empty");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test FifoQueueScheduler maintains FIFO ordering
 */
class FifoQueueSchedulerOrderTestCase : public TestCase
{
  public:
    FifoQueueSchedulerOrderTestCase()
        : TestCase("Test FifoQueueScheduler FIFO ordering")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        // Enqueue 5 tasks
        for (uint32_t i = 1; i <= 5; i++)
        {
            Ptr<ComputeTask> task = CreateObject<ComputeTask>();
            task->SetTaskId(i);
            scheduler->Enqueue(task);
        }

        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 5, "Should have 5 tasks");

        // Verify FIFO order
        for (uint32_t i = 1; i <= 5; i++)
        {
            Ptr<Task> task = scheduler->Dequeue();
            NS_TEST_ASSERT_MSG_EQ(task->GetTaskId(),
                                  i,
                                  "Task " << i << " should be dequeued in order");
        }

        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), true, "Queue should be empty");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test FifoQueueScheduler behavior with empty queue
 */
class FifoQueueSchedulerEmptyTestCase : public TestCase
{
  public:
    FifoQueueSchedulerEmptyTestCase()
        : TestCase("Test FifoQueueScheduler empty queue handling")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        // Dequeue from empty queue should return nullptr
        Ptr<Task> task = scheduler->Dequeue();
        NS_TEST_ASSERT_MSG_EQ((task == nullptr), true, "Dequeue on empty should return nullptr");

        // Peek from empty queue should return nullptr
        task = scheduler->Peek();
        NS_TEST_ASSERT_MSG_EQ((task == nullptr), true, "Peek on empty should return nullptr");

        // Clear on empty queue should not crash
        scheduler->Clear();
        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), true, "Clear should leave queue empty");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test FifoQueueScheduler Peek operation
 */
class FifoQueueSchedulerPeekTestCase : public TestCase
{
  public:
    FifoQueueSchedulerPeekTestCase()
        : TestCase("Test FifoQueueScheduler peek operation")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        Ptr<ComputeTask> task1 = CreateObject<ComputeTask>();
        task1->SetTaskId(1);
        Ptr<ComputeTask> task2 = CreateObject<ComputeTask>();
        task2->SetTaskId(2);

        scheduler->Enqueue(task1);
        scheduler->Enqueue(task2);

        // Peek should return first task without removing it
        Ptr<Task> peeked = scheduler->Peek();
        NS_TEST_ASSERT_MSG_EQ(peeked->GetTaskId(), 1, "Peek should return first task");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 2, "Peek should not remove task");

        // Peek again should return same task
        peeked = scheduler->Peek();
        NS_TEST_ASSERT_MSG_EQ(peeked->GetTaskId(), 1, "Second peek should return same task");

        // After dequeue, peek should return next task
        scheduler->Dequeue();
        peeked = scheduler->Peek();
        NS_TEST_ASSERT_MSG_EQ(peeked->GetTaskId(), 2, "Peek after dequeue should return task 2");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateFifoQueueSchedulerEnqueueDequeueTestCase()
{
    return new FifoQueueSchedulerEnqueueDequeueTestCase;
}

TestCase*
CreateFifoQueueSchedulerOrderTestCase()
{
    return new FifoQueueSchedulerOrderTestCase;
}

TestCase*
CreateFifoQueueSchedulerEmptyTestCase()
{
    return new FifoQueueSchedulerEmptyTestCase;
}

TestCase*
CreateFifoQueueSchedulerPeekTestCase()
{
    return new FifoQueueSchedulerPeekTestCase;
}

} // namespace ns3
