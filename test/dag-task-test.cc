/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/dag-task.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

#include <algorithm>

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test DagTask dependency tracking, ready-set propagation and completion via a diamond DAG.
 */
class DagTaskDependencyTestCase : public TestCase
{
  public:
    DagTaskDependencyTestCase()
        : TestCase("Test DagTask dependency tracking and completion")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<DagTask> dag = CreateObject<DagTask>();

        NS_TEST_ASSERT_MSG_EQ(dag->GetTaskCount(), 0, "New DAG should have no tasks");
        NS_TEST_ASSERT_MSG_EQ(dag->IsComplete(), true, "Empty DAG is complete");
        NS_TEST_ASSERT_MSG_EQ(dag->Validate(), true, "Empty DAG is valid");

        // Diamond: A -> B,C; B,C -> D.
        Ptr<Task> taskA = CreateObject<SimpleTask>();
        taskA->SetTaskId(1);
        Ptr<Task> taskB = CreateObject<SimpleTask>();
        Ptr<Task> taskC = CreateObject<SimpleTask>();
        Ptr<Task> taskD = CreateObject<SimpleTask>();

        uint32_t a = dag->AddTask(taskA);
        uint32_t b = dag->AddTask(taskB);
        uint32_t c = dag->AddTask(taskC);
        uint32_t d = dag->AddTask(taskD);

        NS_TEST_ASSERT_MSG_EQ(dag->GetTaskCount(), 4, "Should have 4 tasks");
        NS_TEST_ASSERT_MSG_EQ(dag->GetTask(a)->GetTaskId(), 1, "Should retrieve correct task");
        NS_TEST_ASSERT_MSG_EQ((dag->GetTask(99) == nullptr), true, "Invalid index returns nullptr");

        dag->AddDependency(a, b);
        dag->AddDependency(a, c);
        dag->AddDependency(b, d);
        dag->AddDependency(c, d);

        NS_TEST_ASSERT_MSG_EQ(dag->Validate(), true, "Diamond is valid");
        NS_TEST_ASSERT_MSG_EQ(dag->IsComplete(), false, "DAG not complete initially");

        std::vector<uint32_t> ready = dag->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 1, "Only A should be ready");

        dag->MarkCompleted(a);
        ready = dag->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 2, "B and C should be ready");

        dag->MarkCompleted(b);
        dag->MarkCompleted(c);
        ready = dag->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 1, "Only D should be ready");
        NS_TEST_ASSERT_MSG_EQ(ready[0], d, "D should be ready");

        dag->MarkCompleted(d);
        NS_TEST_ASSERT_MSG_EQ(dag->IsComplete(), true, "DAG should be complete");
        NS_TEST_ASSERT_MSG_EQ(dag->GetReadyTasks().size(), 0, "No tasks ready when complete");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test DagTask cycle detection.
 */
class DagTaskCycleDetectionTestCase : public TestCase
{
  public:
    DagTaskCycleDetectionTestCase()
        : TestCase("Test DagTask cycle detection")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<DagTask> dag = CreateObject<DagTask>();
        Ptr<Task> taskA = CreateObject<SimpleTask>();
        Ptr<Task> taskB = CreateObject<SimpleTask>();
        Ptr<Task> taskC = CreateObject<SimpleTask>();

        uint32_t a = dag->AddTask(taskA);
        uint32_t b = dag->AddTask(taskB);
        uint32_t c = dag->AddTask(taskC);

        dag->AddDependency(a, b);
        dag->AddDependency(b, c);
        dag->AddDependency(c, a);

        NS_TEST_ASSERT_MSG_EQ(dag->Validate(), false, "Cycle A->B->C->A should be detected");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test DagTask accumulates predecessor outputs into a successor's input on completion.
 */
class DagTaskDataAccumulationTestCase : public TestCase
{
  public:
    DagTaskDataAccumulationTestCase()
        : TestCase("Test DagTask data accumulation from multiple predecessors")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<DagTask> dag = CreateObject<DagTask>();

        // Fan-in A,B -> C with data dependencies.
        Ptr<Task> taskA = CreateObject<SimpleTask>();
        taskA->SetOutputSize(500);
        Ptr<Task> taskB = CreateObject<SimpleTask>();
        taskB->SetOutputSize(300);
        Ptr<Task> taskC = CreateObject<SimpleTask>();
        taskC->SetInputSize(0);

        uint32_t a = dag->AddTask(taskA);
        uint32_t b = dag->AddTask(taskB);
        uint32_t c = dag->AddTask(taskC);

        dag->AddDataDependency(a, c);
        dag->AddDataDependency(b, c);

        dag->MarkCompleted(a);
        NS_TEST_ASSERT_MSG_EQ(taskC->GetInputSize(), 500, "C should have A's output");

        dag->MarkCompleted(b);
        NS_TEST_ASSERT_MSG_EQ(taskC->GetInputSize(), 800, "C should have A+B outputs");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateDagTaskDependencyTestCase()
{
    return new DagTaskDependencyTestCase;
}

TestCase*
CreateDagTaskCycleDetectionTestCase()
{
    return new DagTaskCycleDetectionTestCase;
}

TestCase*
CreateDagTaskDataAccumulationTestCase()
{
    return new DagTaskDataAccumulationTestCase;
}

} // namespace ns3
