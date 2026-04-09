/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster-state.h"
#include "ns3/cluster.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/least-loaded-scheduler.h"
#include "ns3/scaling-policy.h"
#include "ns3/simple-task.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test LeastLoadedScheduler selects the backend with the shortest estimated completion time.
 */
class LeastLoadedSchedulerTestCase : public TestCase
{
  public:
    LeastLoadedSchedulerTestCase()
        : TestCase("LeastLoadedScheduler selects backend by estimated completion time")
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
        state.NotifyTaskDispatched(0, 5.0e12);

        Ptr<LeastLoadedScheduler> scheduler = CreateObject<LeastLoadedScheduler>();

        // Score(0) = (5e12 + 1e12) / 1e12 = 6s; score(1) = (0 + 1e12) / 1e12 = 1s.
        // Backend 1 wins by score (smaller estimated completion), not by tie-break.
        Ptr<SimpleTask> task1 = CreateObject<SimpleTask>();
        task1->SetTaskId(1);
        task1->SetComputeDemand(1.0e12);
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(task1, cluster, state),
                              1,
                              "Backend 1 has lower estimated completion time");

        // Dispatch enough work to backend 1 to invert the relative load, then check
        // backend 0 becomes the better candidate.
        state.NotifyTaskDispatched(1, 6.0e12);

        Ptr<SimpleTask> task2 = CreateObject<SimpleTask>();
        task2->SetTaskId(2);
        task2->SetComputeDemand(1.0e12);
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(task2, cluster, state),
                              0,
                              "Backend 0 now has lower estimated completion time");

        NS_TEST_ASSERT_MSG_EQ(scheduler->GetName(),
                              "LeastLoaded",
                              "Scheduler name should be LeastLoaded");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test LeastLoadedScheduler filters by required accelerator type.
 */
class LeastLoadedSchedulerTypeFilterTestCase : public TestCase
{
  public:
    LeastLoadedSchedulerTypeFilterTestCase()
        : TestCase("LeastLoadedScheduler filters by accelerator type")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(3);
        InternetStackHelper internet;
        internet.Install(nodes);

        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000), "GPU");
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000), "TPU");
        cluster.AddBackend(nodes.Get(2), InetSocketAddress(Ipv4Address("10.0.0.3"), 9000), "GPU");

        ClusterState state;
        state.Resize(3);

        Ptr<LeastLoadedScheduler> scheduler = CreateObject<LeastLoadedScheduler>();

        Ptr<SimpleTask> task1 = CreateObject<SimpleTask>();
        task1->SetTaskId(1);
        task1->SetRequiredAcceleratorType("GPU");
        int32_t idx1 = scheduler->ScheduleTask(task1, cluster, state);
        bool validGpuTie = (idx1 == 0 || idx1 == 2);
        NS_TEST_ASSERT_MSG_EQ(validGpuTie, true, "Should pick a GPU backend when both idle");

        state.NotifyTaskDispatched(idx1);
        Ptr<SimpleTask> task2 = CreateObject<SimpleTask>();
        task2->SetTaskId(2);
        task2->SetRequiredAcceleratorType("GPU");
        int32_t idx2 = scheduler->ScheduleTask(task2, cluster, state);
        int32_t expectedOther = (idx1 == 0) ? 2 : 0;
        NS_TEST_ASSERT_MSG_EQ(idx2,
                              expectedOther,
                              "Should pick the other GPU backend (less loaded)");

        Ptr<SimpleTask> task3 = CreateObject<SimpleTask>();
        task3->SetTaskId(3);
        task3->SetRequiredAcceleratorType("FPGA");
        int32_t idx3 = scheduler->ScheduleTask(task3, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(idx3, -1, "Should return -1 for unknown type");
    }
};

} // namespace

TestCase*
CreateLeastLoadedSchedulerTestCase()
{
    return new LeastLoadedSchedulerTestCase;
}

TestCase*
CreateLeastLoadedSchedulerTypeFilterTestCase()
{
    return new LeastLoadedSchedulerTypeFilterTestCase;
}

} // namespace ns3
