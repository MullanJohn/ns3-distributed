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
 * @brief Test DagTask dependency tracking and task completion
 *
 * Tests core functionality: adding tasks, dependencies, ready task tracking,
 * completion propagation, and DAG completion. Uses diamond pattern which
 * covers both parallel and sequential dependencies.
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

        // Empty DAG
        NS_TEST_ASSERT_MSG_EQ(dag->GetTaskCount(), 0, "New DAG should have no tasks");
        NS_TEST_ASSERT_MSG_EQ(dag->IsComplete(), true, "Empty DAG is complete");
        NS_TEST_ASSERT_MSG_EQ(dag->Validate(), true, "Empty DAG is valid");

        // Create diamond: A -> B -> D
        //                 A -> C -> D
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

        // Initially only A is ready
        std::vector<uint32_t> ready = dag->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 1, "Only A should be ready");

        // Complete A -> B and C become ready (parallel)
        dag->MarkCompleted(a);
        ready = dag->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 2, "B and C should be ready");

        // Complete B and C -> D becomes ready
        dag->MarkCompleted(b);
        dag->MarkCompleted(c);
        ready = dag->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 1, "Only D should be ready");
        NS_TEST_ASSERT_MSG_EQ(ready[0], d, "D should be ready");

        // Complete D -> DAG complete
        dag->MarkCompleted(d);
        NS_TEST_ASSERT_MSG_EQ(dag->IsComplete(), true, "DAG should be complete");
        NS_TEST_ASSERT_MSG_EQ(dag->GetReadyTasks().size(), 0, "No tasks ready when complete");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test DagTask cycle detection
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
        dag->AddDependency(c, a); // Creates cycle A -> B -> C -> A

        NS_TEST_ASSERT_MSG_EQ(dag->Validate(), false, "Cycle should be detected");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test DagTask data dependency propagation
 *
 * Tests that when a task completes, its output size is added to
 * data-dependent successors' input sizes.
 */
class DagTaskDataDependencyTestCase : public TestCase
{
  public:
    DagTaskDataDependencyTestCase()
        : TestCase("Test DagTask data dependency propagation")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<DagTask> dag = CreateObject<DagTask>();

        // A -> B (data dependency)
        Ptr<Task> taskA = CreateObject<SimpleTask>();
        taskA->SetOutputSize(1000000); // 1MB output
        Ptr<Task> taskB = CreateObject<SimpleTask>();
        taskB->SetInputSize(100); // Initial input

        uint32_t a = dag->AddTask(taskA);
        uint32_t b = dag->AddTask(taskB);

        dag->AddDataDependency(a, b);

        // Initially B has its original input
        NS_TEST_ASSERT_MSG_EQ(taskB->GetInputSize(), 100, "B should have initial input size");

        // Complete A -> B's input should increase by A's output
        dag->MarkCompleted(a);
        NS_TEST_ASSERT_MSG_EQ(taskB->GetInputSize(),
                              1000100,
                              "B's input should include A's output");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test DagTask data accumulation from multiple predecessors
 *
 * Tests that when multiple tasks feed into one successor, their
 * outputs accumulate in the successor's input size.
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

        // A -> C, B -> C (fan-in with data dependencies)
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

        // Complete A -> C gets A's output
        dag->MarkCompleted(a);
        NS_TEST_ASSERT_MSG_EQ(taskC->GetInputSize(), 500, "C should have A's output");

        // Complete B -> C accumulates B's output
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
CreateDagTaskDataDependencyTestCase()
{
    return new DagTaskDataDependencyTestCase;
}

TestCase*
CreateDagTaskDataAccumulationTestCase()
{
    return new DagTaskDataAccumulationTestCase;
}

} // namespace ns3
