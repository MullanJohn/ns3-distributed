/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/fifo-queue-scheduler.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
 * @brief Test FifoQueueScheduler enqueue, dequeue, peek, empty and clear behaviour.
 */
class FifoQueueSchedulerTestCase : public TestCase
{
  public:
    FifoQueueSchedulerTestCase()
        : TestCase("Test FifoQueueScheduler lifecycle")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), true, "New scheduler should be empty");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 0, "New scheduler should have length 0");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetName(), "FIFO", "Name should be FIFO");
        NS_TEST_ASSERT_MSG_EQ(scheduler->Dequeue(),
                              nullptr,
                              "Dequeue on empty should return nullptr");
        NS_TEST_ASSERT_MSG_EQ(scheduler->Peek(),
                              nullptr,
                              "Peek on empty should return nullptr");

        for (uint32_t i = 1; i <= 5; i++)
        {
            Ptr<Task> task = CreateObject<SimpleTask>();
            task->SetTaskId(i);
            scheduler->Enqueue(task);
        }
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 5, "Length should be 5 after enqueueing 5");

        // Peek is non-destructive and always returns the head.
        NS_TEST_ASSERT_MSG_EQ(scheduler->Peek()->GetTaskId(), 1, "Peek should return head");
        NS_TEST_ASSERT_MSG_EQ(scheduler->Peek()->GetTaskId(), 1, "Peek should be repeatable");
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetLength(), 5, "Peek should not change length");

        for (uint32_t i = 1; i <= 5; i++)
        {
            Ptr<Task> task = scheduler->Dequeue();
            NS_TEST_ASSERT_MSG_EQ(task->GetTaskId(),
                                  i,
                                  "Task " << i << " should be dequeued in FIFO order");
        }
        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), true, "Queue should be empty after draining");

        scheduler->Enqueue(CreateObject<SimpleTask>());
        scheduler->Clear();
        NS_TEST_ASSERT_MSG_EQ(scheduler->IsEmpty(), true, "Clear should empty the queue");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateFifoQueueSchedulerTestCase()
{
    return new FifoQueueSchedulerTestCase;
}

} // namespace ns3
