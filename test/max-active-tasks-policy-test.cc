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
#include "ns3/internet-stack-helper.h"
#include "ns3/max-active-tasks-policy.h"
#include "ns3/scaling-policy.h"
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
 * @brief Test MaxActiveTasksPolicy admits when at least one backend has capacity.
 */
class MaxActiveTasksAdmitCapacityTestCase : public TestCase
{
  public:
    MaxActiveTasksAdmitCapacityTestCase()
        : TestCase("MaxActiveTasksPolicy admits when capacity exists")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000));

        Ptr<MaxActiveTasksPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
        policy->SetAttribute("MaxActiveTasks", UintegerValue(5));

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        Ptr<DagTask> dag = CreateObject<DagTask>();
        dag->AddTask(task);

        // Partial load: backend 0 at threshold (5/5), backend 1 below (3/5) -> admit.
        ClusterState partialState;
        partialState.Resize(2);
        for (int i = 0; i < 5; ++i)
        {
            partialState.NotifyTaskDispatched(0);
        }
        for (int i = 0; i < 3; ++i)
        {
            partialState.NotifyTaskDispatched(1);
        }
        NS_TEST_ASSERT_MSG_EQ(policy->ShouldAdmit(dag, cluster, partialState),
                              true,
                              "Should admit when at least one backend has capacity");

        // All idle: both backends at 0/5 -> admit.
        ClusterState idleState;
        idleState.Resize(2);
        NS_TEST_ASSERT_MSG_EQ(policy->ShouldAdmit(dag, cluster, idleState),
                              true,
                              "Should admit when all backends are idle");

        NS_TEST_ASSERT_MSG_EQ(policy->GetName(),
                              "MaxActiveTasks",
                              "Policy name should be MaxActiveTasks");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test MaxActiveTasksPolicy rejects when all backends are at capacity.
 */
class MaxActiveTasksRejectFullTestCase : public TestCase
{
  public:
    MaxActiveTasksRejectFullTestCase()
        : TestCase("MaxActiveTasksPolicy rejects when all backends are at capacity")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000));

        ClusterState state;
        state.Resize(2);

        for (int i = 0; i < 5; ++i)
        {
            state.NotifyTaskDispatched(0);
            state.NotifyTaskDispatched(1);
        }

        Ptr<MaxActiveTasksPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
        policy->SetAttribute("MaxActiveTasks", UintegerValue(5));

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        Ptr<DagTask> dag = CreateObject<DagTask>();
        dag->AddTask(task);

        bool admitted = policy->ShouldAdmit(dag, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(admitted, false, "Should reject when all backends are at capacity");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test MaxActiveTasksPolicy counts pending admission reservations against capacity.
 */
class MaxActiveTasksPendingReservationTestCase : public TestCase
{
  public:
    MaxActiveTasksPendingReservationTestCase()
        : TestCase("MaxActiveTasksPolicy rejects when pending admissions consume remaining capacity")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000));

        ClusterState state;
        state.Resize(2);

        // Each backend at 4/5 -> 2 slots available; reserve both.
        for (int i = 0; i < 4; ++i)
        {
            state.NotifyTaskDispatched(0);
            state.NotifyTaskDispatched(1);
        }
        state.ReserveTasks(2);

        Ptr<MaxActiveTasksPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
        policy->SetAttribute("MaxActiveTasks", UintegerValue(5));

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        Ptr<DagTask> dag = CreateObject<DagTask>();
        dag->AddTask(task);

        NS_TEST_ASSERT_MSG_EQ(policy->ShouldAdmit(dag, cluster, state),
                              false,
                              "Should reject when pending admissions consume remaining capacity");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test MaxActiveTasksPolicy rejects when the incoming workload size exceeds remaining capacity.
 */
class MaxActiveTasksIncomingWorkloadSizeTestCase : public TestCase
{
  public:
    MaxActiveTasksIncomingWorkloadSizeTestCase()
        : TestCase("MaxActiveTasksPolicy rejects when the incoming workload exceeds remaining capacity")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000));

        ClusterState state;
        state.Resize(2);

        // Each backend at 4/5 -> 2 free slots total; the incoming DAG needs 3.
        for (int i = 0; i < 4; ++i)
        {
            state.NotifyTaskDispatched(0);
            state.NotifyTaskDispatched(1);
        }

        Ptr<MaxActiveTasksPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
        policy->SetAttribute("MaxActiveTasks", UintegerValue(5));

        Ptr<DagTask> dag = CreateObject<DagTask>();
        for (uint64_t taskId = 1; taskId <= 3; ++taskId)
        {
            Ptr<SimpleTask> task = CreateObject<SimpleTask>();
            task->SetTaskId(taskId);
            dag->AddTask(task);
        }

        NS_TEST_ASSERT_MSG_EQ(
            policy->ShouldAdmit(dag, cluster, state),
            false,
            "Should reject when the incoming workload needs more slots than remain available");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test MaxActiveTasksPolicy keeps pending reservations scoped to their accelerator type.
 */
class MaxActiveTasksTypeScopedReservationTestCase : public TestCase
{
  public:
    MaxActiveTasksTypeScopedReservationTestCase()
        : TestCase("MaxActiveTasksPolicy keeps pending reservations scoped to accelerator type")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000), "GPU");
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000), "TPU");

        ClusterState state;
        state.Resize(2);
        state.ReserveTasks(1, "GPU");

        Ptr<MaxActiveTasksPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
        policy->SetAttribute("MaxActiveTasks", UintegerValue(1));

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetRequiredAcceleratorType("TPU");
        Ptr<DagTask> dag = CreateObject<DagTask>();
        dag->AddTask(task);

        NS_TEST_ASSERT_MSG_EQ(policy->ShouldAdmit(dag, cluster, state),
                              true,
                              "GPU reservations should not consume TPU-only capacity");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateMaxActiveTasksAdmitCapacityTestCase()
{
    return new MaxActiveTasksAdmitCapacityTestCase;
}

TestCase*
CreateMaxActiveTasksRejectFullTestCase()
{
    return new MaxActiveTasksRejectFullTestCase;
}

TestCase*
CreateMaxActiveTasksPendingReservationTestCase()
{
    return new MaxActiveTasksPendingReservationTestCase;
}

TestCase*
CreateMaxActiveTasksIncomingWorkloadSizeTestCase()
{
    return new MaxActiveTasksIncomingWorkloadSizeTestCase;
}

TestCase*
CreateMaxActiveTasksTypeScopedReservationTestCase()
{
    return new MaxActiveTasksTypeScopedReservationTestCase;
}

} // namespace ns3
