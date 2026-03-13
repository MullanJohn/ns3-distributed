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
 * @brief Test that workload is admitted when at least one backend has capacity.
 */
class MaxActiveTasksAdmitCapacityTestCase : public TestCase
{
  public:
    MaxActiveTasksAdmitCapacityTestCase()
        : TestCase("MaxActiveTasksPolicy admits when at least one backend has capacity")
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

        // Backend 0: at threshold (5 active)
        for (int i = 0; i < 5; ++i)
        {
            state.NotifyTaskDispatched(0);
        }
        // Backend 1: below threshold (3 active)
        for (int i = 0; i < 3; ++i)
        {
            state.NotifyTaskDispatched(1);
        }

        Ptr<MaxActiveTasksPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
        policy->SetAttribute("MaxActiveTasks", UintegerValue(5));

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        Ptr<DagTask> dag = CreateObject<DagTask>();
        dag->AddTask(task);

        bool admitted = policy->ShouldAdmit(dag, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(admitted,
                              true,
                              "Should admit when at least one backend has capacity");

        NS_TEST_ASSERT_MSG_EQ(policy->GetName(),
                              "MaxActiveTasks",
                              "Policy name should be MaxActiveTasks");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test that workload is rejected when all backends are at capacity.
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

        // Both backends at threshold
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
 * @brief Test that workload is admitted when all backends are idle.
 */
class MaxActiveTasksAdmitEmptyTestCase : public TestCase
{
  public:
    MaxActiveTasksAdmitEmptyTestCase()
        : TestCase("MaxActiveTasksPolicy admits when all backends are idle")
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

        Ptr<MaxActiveTasksPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
        policy->SetAttribute("MaxActiveTasks", UintegerValue(5));

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        Ptr<DagTask> dag = CreateObject<DagTask>();
        dag->AddTask(task);

        bool admitted = policy->ShouldAdmit(dag, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(admitted, true, "Should admit when all backends are idle");

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
CreateMaxActiveTasksAdmitEmptyTestCase()
{
    return new MaxActiveTasksAdmitEmptyTestCase;
}

} // namespace ns3
