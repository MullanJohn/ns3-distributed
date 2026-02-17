/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster-state.h"
#include "ns3/cluster.h"
#include "ns3/dag-task.h"
#include "ns3/deadline-aware-admission-policy.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/scaling-policy.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test that a task with a feasible deadline is admitted.
 */
class FeasibleDeadlineTestCase : public TestCase
{
  public:
    FeasibleDeadlineTestCase()
        : TestCase("DeadlineAwareAdmissionPolicy admits feasible deadline")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(1);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));

        ClusterState state;
        state.Resize(1);

        Ptr<DeadlineAwareAdmissionPolicy> policy = CreateObject<DeadlineAwareAdmissionPolicy>();
        policy->SetAttribute("ComputeRate", DoubleValue(1e9));

        // Task: demand=1e9 FLOPS, deadline=Now+2s, computeRate=1e9
        // exec = 1e9/1e9 = 1s, wait = 0 (idle backend)
        // completion = 0 + 1s = 1s <= 2s → feasible
        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetComputeDemand(1e9);
        task->SetDeadline(Simulator::Now() + Seconds(2.0));

        Ptr<DagTask> dag = CreateObject<DagTask>();
        dag->AddTask(task);

        bool admitted = policy->ShouldAdmit(dag, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(admitted, true, "Task with feasible deadline should be admitted");

        NS_TEST_ASSERT_MSG_EQ(policy->GetName(),
                              "DeadlineAware",
                              "Policy name should be DeadlineAware");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test that a task with an infeasible deadline is rejected.
 */
class InfeasibleDeadlineTestCase : public TestCase
{
  public:
    InfeasibleDeadlineTestCase()
        : TestCase("DeadlineAwareAdmissionPolicy rejects infeasible deadline")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(1);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));

        ClusterState state;
        state.Resize(1);

        for (int i = 0; i < 5; ++i)
        {
            state.NotifyTaskDispatched(0);
        }

        Ptr<DeadlineAwareAdmissionPolicy> policy = CreateObject<DeadlineAwareAdmissionPolicy>();
        policy->SetAttribute("ComputeRate", DoubleValue(1e9));

        // Task: demand=1e9 FLOPS, deadline=Now+2s, computeRate=1e9
        // wait = 5 * (1e9/1e9) = 5s, exec = 1s
        // completion = 0 + 6s > 2s → infeasible
        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetComputeDemand(1e9);
        task->SetDeadline(Simulator::Now() + Seconds(2.0));

        Ptr<DagTask> dag = CreateObject<DagTask>();
        dag->AddTask(task);

        bool admitted = policy->ShouldAdmit(dag, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(admitted, false, "Task with infeasible deadline should be rejected");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test that a task without a deadline is always admitted.
 */
class NoDeadlineTestCase : public TestCase
{
  public:
    NoDeadlineTestCase()
        : TestCase("DeadlineAwareAdmissionPolicy admits tasks without deadlines")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(1);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));

        ClusterState state;
        state.Resize(1);

        for (int i = 0; i < 100; ++i)
        {
            state.NotifyTaskDispatched(0);
        }

        Ptr<DeadlineAwareAdmissionPolicy> policy = CreateObject<DeadlineAwareAdmissionPolicy>();
        policy->SetAttribute("ComputeRate", DoubleValue(1e9));

        // Task without deadline — always feasible
        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetComputeDemand(1e9);

        Ptr<DagTask> dag = CreateObject<DagTask>();
        dag->AddTask(task);

        bool admitted = policy->ShouldAdmit(dag, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(admitted, true, "Task without deadline should always be admitted");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test that DAG dependency chains shift the earliest start time.
 */
class DagDependencyDeadlineTestCase : public TestCase
{
  public:
    DagDependencyDeadlineTestCase()
        : TestCase("DeadlineAwareAdmissionPolicy accounts for DAG dependencies")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(1);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));

        ClusterState state;
        state.Resize(1);

        Ptr<DeadlineAwareAdmissionPolicy> policy = CreateObject<DeadlineAwareAdmissionPolicy>();
        policy->SetAttribute("ComputeRate", DoubleValue(1e9));

        // Task A: 1s exec, no deadline
        Ptr<SimpleTask> taskA = CreateObject<SimpleTask>();
        taskA->SetTaskId(1);
        taskA->SetComputeDemand(1e9);

        // Task B: 1s exec, deadline=Now+1.5s
        // Earliest start = Now+1s (after A), completion = Now+2s > Now+1.5s
        Ptr<SimpleTask> taskB = CreateObject<SimpleTask>();
        taskB->SetTaskId(2);
        taskB->SetComputeDemand(1e9);
        taskB->SetDeadline(Simulator::Now() + Seconds(1.5));

        Ptr<DagTask> dag = CreateObject<DagTask>();
        uint32_t a = dag->AddTask(taskA);
        uint32_t b = dag->AddTask(taskB);
        dag->AddDependency(a, b);

        bool admitted = policy->ShouldAdmit(dag, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(admitted,
                              false,
                              "Task B infeasible due to predecessor A delaying its start");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateFeasibleDeadlineTestCase()
{
    return new FeasibleDeadlineTestCase;
}

TestCase*
CreateInfeasibleDeadlineTestCase()
{
    return new InfeasibleDeadlineTestCase;
}

TestCase*
CreateNoDeadlineTestCase()
{
    return new NoDeadlineTestCase;
}

TestCase*
CreateDagDependencyDeadlineTestCase()
{
    return new DagDependencyDeadlineTestCase;
}

} // namespace ns3
