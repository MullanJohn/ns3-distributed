/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster-state.h"
#include "ns3/cluster.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/network-aware-scheduler.h"
#include "ns3/scaling-policy.h"
#include "ns3/simple-task.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
 * @brief Test NetworkAwareScheduler is identical to LeastLoadedScheduler when no
 * comm properties are configured (safe drop-in default).
 */
class NetworkAwareSchedulerDefaultTestCase : public TestCase
{
  public:
    NetworkAwareSchedulerDefaultTestCase()
        : TestCase("NetworkAwareScheduler matches LeastLoadedScheduler when unconfigured")
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
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);

        // Backend 0 already has 5s of queued work; backend 1 is empty.
        // No commDelay/commCapacity configured on either backend, so the network
        // term is zero and this reduces to LeastLoadedScheduler's scoring exactly:
        // score(0) = (5e12 + 1e12)/1e12 = 6s; score(1) = 1e12/1e12 = 1s.
        state.NotifyTaskDispatched(0, 5.0e12);

        Ptr<NetworkAwareScheduler> scheduler = CreateObject<NetworkAwareScheduler>();

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetComputeDemand(1.0e12);
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(task, cluster, state),
                              1,
                              "Backend 1 has lower estimated completion time");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test NetworkAwareScheduler's static commDelay term can flip a compute tie.
 */
class NetworkAwareSchedulerDelayTestCase : public TestCase
{
  public:
    NetworkAwareSchedulerDelayTestCase()
        : TestCase("NetworkAwareScheduler prefers the lower-delay backend on a compute tie")
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
        // Backend 0 is "cloud" (1s one-way delay); backend 1 is "edge" (no delay).
        cluster.AddBackend(nodes.Get(0),
                           InetSocketAddress(Ipv4Address("10.0.0.1"), 9000),
                           "",
                           Seconds(1));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000));

        ClusterState state;
        state.Resize(2);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);

        Ptr<NetworkAwareScheduler> scheduler = CreateObject<NetworkAwareScheduler>();

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetComputeDemand(1.0e12);

        // computeScore(0) = computeScore(1) = 1e12/1e12 = 1s (tied).
        // networkScore(0) = 2*1s = 2s -> score(0) = 1 + 2 = 3.
        // networkScore(1) = 0       -> score(1) = 1 + 0 = 1.
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(task, cluster, state),
                              1,
                              "Backend 1 (lower delay) should win despite equal compute score");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test NetworkAwareScheduler's static commCapacity transmission-time term.
 */
class NetworkAwareSchedulerCapacityTestCase : public TestCase
{
  public:
    NetworkAwareSchedulerCapacityTestCase()
        : TestCase("NetworkAwareScheduler prefers the higher-capacity backend for large payloads")
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
        // Equal delay (0), but backend 0 has a much slower link than backend 1.
        cluster.AddBackend(nodes.Get(0),
                           InetSocketAddress(Ipv4Address("10.0.0.1"), 9000),
                           "",
                           Seconds(0),
                           1.0e6);
        cluster.AddBackend(nodes.Get(1),
                           InetSocketAddress(Ipv4Address("10.0.0.2"), 9000),
                           "",
                           Seconds(0),
                           1.0e9);

        ClusterState state;
        state.Resize(2);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);

        Ptr<NetworkAwareScheduler> scheduler = CreateObject<NetworkAwareScheduler>();

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetComputeDemand(0.0);
        task->SetInputSize(125000);
        task->SetOutputSize(0);

        // computeScore is 0 for both (no compute demand), so the network term decides.
        // transmitSeconds(0) = 125000 / 1e6 = 0.125s.
        // transmitSeconds(1) = 125000 / 1e9 = 0.000125s.
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(task, cluster, state),
                              1,
                              "Backend 1 (higher capacity) should win on transmission time");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test NetworkWeight=0 neutralizes the network term (falls back to tie-break).
 */
class NetworkAwareSchedulerWeightTestCase : public TestCase
{
  public:
    NetworkAwareSchedulerWeightTestCase()
        : TestCase("NetworkAwareScheduler NetworkWeight=0 disables network sensitivity")
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
        cluster.AddBackend(nodes.Get(0),
                           InetSocketAddress(Ipv4Address("10.0.0.1"), 9000),
                           "",
                           Seconds(1));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000));

        ClusterState state;
        state.Resize(2);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);

        Ptr<NetworkAwareScheduler> scheduler = CreateObject<NetworkAwareScheduler>();
        scheduler->SetAttribute("NetworkWeight", DoubleValue(0.0));

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetComputeDemand(1.0e12);

        // With NetworkWeight=0, score(0) == score(1) == 1s (tied); tie-break falls
        // back to activeTasks (tied at 0) then backend index, so backend 0 wins.
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(task, cluster, state),
                              0,
                              "Zero network weight should neutralize the delay penalty");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test that a live-measured delay in ClusterState overrides the static
 * Cluster::Backend commDelay.
 */
class NetworkAwareSchedulerLiveOverrideTestCase : public TestCase
{
  public:
    NetworkAwareSchedulerLiveOverrideTestCase()
        : TestCase("NetworkAwareScheduler prefers live-measured delay over static commDelay")
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
        // Same static setup as the delay test: backend 0 would normally lose.
        cluster.AddBackend(nodes.Get(0),
                           InetSocketAddress(Ipv4Address("10.0.0.1"), 9000),
                           "",
                           Seconds(1));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000));

        ClusterState state;
        state.Resize(2);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);

        // Live measurement on backend 0 overrides its static 1s delay with 0s,
        // matching backend 1's (unmeasured, static) delay.
        state.UpdateMeasuredNetworkDelay(0, Seconds(0), 1.0);

        Ptr<NetworkAwareScheduler> scheduler = CreateObject<NetworkAwareScheduler>();

        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetComputeDemand(1.0e12);

        // Both backends now have an effective one-way delay of 0s, so the network
        // and compute terms tie; tie-break falls back to backend index, so backend
        // 0 wins here (the opposite of NetworkAwareSchedulerDelayTestCase), proving
        // the live measurement took priority over the static commDelay.
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(task, cluster, state),
                              0,
                              "Live measurement should override the static commDelay");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test NetworkAwareScheduler::GetName().
 */
class NetworkAwareSchedulerNameTestCase : public TestCase
{
  public:
    NetworkAwareSchedulerNameTestCase()
        : TestCase("NetworkAwareScheduler::GetName() returns NetworkAware")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<NetworkAwareScheduler> scheduler = CreateObject<NetworkAwareScheduler>();
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetName(),
                              "NetworkAware",
                              "Scheduler name should be NetworkAware");
    }
};

} // namespace

TestCase*
CreateNetworkAwareSchedulerDefaultTestCase()
{
    return new NetworkAwareSchedulerDefaultTestCase;
}

TestCase*
CreateNetworkAwareSchedulerDelayTestCase()
{
    return new NetworkAwareSchedulerDelayTestCase;
}

TestCase*
CreateNetworkAwareSchedulerCapacityTestCase()
{
    return new NetworkAwareSchedulerCapacityTestCase;
}

TestCase*
CreateNetworkAwareSchedulerWeightTestCase()
{
    return new NetworkAwareSchedulerWeightTestCase;
}

TestCase*
CreateNetworkAwareSchedulerLiveOverrideTestCase()
{
    return new NetworkAwareSchedulerLiveOverrideTestCase;
}

TestCase*
CreateNetworkAwareSchedulerNameTestCase()
{
    return new NetworkAwareSchedulerNameTestCase;
}

} // namespace ns3
