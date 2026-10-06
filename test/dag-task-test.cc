/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/dag-task.h"
#include "ns3/double.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <set>

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
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
 * @ingroup continuum-tests
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
 * @ingroup continuum-tests
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

/**
 * @ingroup continuum-tests
 * @brief Test DagTask::Instantiate() replicates structure and freshly
 * samples each task from its own distribution attributes.
 */
class DagTaskInstantiateTestCase : public TestCase
{
  public:
    DagTaskInstantiateTestCase()
        : TestCase("Test DagTask::Instantiate() structure replication and sampling")
    {
    }

  private:
    void DoRun() override
    {
        // 3-task template: 0 -> 1 (data), 0 -> 2 (ordering only).
        // Task 0 has an InputSizeDistribution; task 1 (a data successor of 0)
        // has none, so it should instantiate with InputSize=0 (the "input is
        // relayed from upstream" convention). Every task has a
        // ComputeDemandDistribution so repeated Instantiate() calls draw
        // distinct values.
        Ptr<DagTask> tmpl = CreateObject<DagTask>();

        Ptr<SimpleTask> task0 = CreateObject<SimpleTask>();
        Ptr<UniformRandomVariable> inputRv = CreateObject<UniformRandomVariable>();
        inputRv->SetAttribute("Min", DoubleValue(100));
        inputRv->SetAttribute("Max", DoubleValue(200));
        task0->SetAttribute("InputSizeDistribution", PointerValue(inputRv));

        Ptr<SimpleTask> task1 = CreateObject<SimpleTask>();
        Ptr<SimpleTask> task2 = CreateObject<SimpleTask>();

        Ptr<UniformRandomVariable> computeRv = CreateObject<UniformRandomVariable>();
        computeRv->SetAttribute("Min", DoubleValue(1e9));
        computeRv->SetAttribute("Max", DoubleValue(2e9));
        task0->SetAttribute("ComputeDemandDistribution", PointerValue(computeRv));
        task1->SetAttribute("ComputeDemandDistribution", PointerValue(computeRv));
        task2->SetAttribute("ComputeDemandDistribution", PointerValue(computeRv));

        uint32_t i0 = tmpl->AddTask(task0);
        uint32_t i1 = tmpl->AddTask(task1);
        uint32_t i2 = tmpl->AddTask(task2);
        tmpl->AddDataDependency(i0, i1);
        tmpl->AddDependency(i0, i2);

        Ptr<DagTask> dag = tmpl->Instantiate();

        NS_TEST_ASSERT_MSG_EQ(dag->GetTaskCount(), 3, "Instantiate() should preserve task count");
        NS_TEST_ASSERT_MSG_NE(dag->GetTask(i0), task0, "Instantiate() should produce fresh tasks");

        NS_TEST_ASSERT_MSG_EQ(dag->GetTask(i1)->GetInputSize(),
                              0,
                              "Data successor with no InputSizeDistribution starts at 0");

        std::vector<uint32_t> readyBefore = dag->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(readyBefore.size(), 1, "Only task 0 should be ready initially");

        uint64_t outputBefore = dag->GetTask(i0)->GetOutputSize();
        dag->GetTask(i0)->SetOutputSize(outputBefore + 42);
        dag->MarkCompleted(i0);
        NS_TEST_ASSERT_MSG_EQ(dag->GetTask(i1)->GetInputSize(),
                              outputBefore + 42,
                              "Data dependency should be replicated (0->1)");

        std::vector<uint32_t> ready = dag->GetReadyTasks();
        std::sort(ready.begin(), ready.end());
        NS_TEST_ASSERT_MSG_EQ(ready.size(),
                              2,
                              "Both task 1 (data) and task 2 (ordering) depend only on task 0, "
                              "so both should be ready once it completes");
        NS_TEST_ASSERT_MSG_EQ(ready[0], i1, "Data dependency should be replicated (0->1)");
        NS_TEST_ASSERT_MSG_EQ(ready[1], i2, "Ordering dependency should be replicated (0->2)");

        // Independent samples: instantiating twice should not reuse the same
        // underlying Task objects, and a wide-range RV should (with
        // overwhelming probability) not repeat exactly across instantiations.
        Ptr<DagTask> dag2 = tmpl->Instantiate();
        NS_TEST_ASSERT_MSG_NE(dag2->GetTask(i0)->GetComputeDemand(),
                              dag->GetTask(i0)->GetComputeDemand(),
                              "Repeated Instantiate() calls should draw fresh samples");

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

TestCase*
CreateDagTaskInstantiateTestCase()
{
    return new DagTaskInstantiateTestCase;
}

} // namespace ns3
