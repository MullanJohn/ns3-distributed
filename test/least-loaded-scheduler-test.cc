/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster-state.h"
#include "ns3/cluster.h"
#include "ns3/scaling-policy.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/least-loaded-scheduler.h"
#include "ns3/simple-task.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test LeastLoadedScheduler selects the backend with fewest active tasks.
 */
class LeastLoadedSchedulerTestCase : public TestCase
{
  public:
    LeastLoadedSchedulerTestCase()
        : TestCase("LeastLoadedScheduler selects least-loaded backend")
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
                           InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));
        cluster.AddBackend(nodes.Get(1),
                           InetSocketAddress(Ipv4Address("10.0.0.2"), 9000));

        ClusterState state;
        state.Resize(2);

        Ptr<LeastLoadedScheduler> scheduler = CreateObject<LeastLoadedScheduler>();

        // Both idle — picks index 0 (first match on tie)
        Ptr<SimpleTask> task1 = CreateObject<SimpleTask>();
        task1->SetTaskId(1);
        int32_t idx1 = scheduler->ScheduleTask(task1, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(idx1, 0, "Should pick first backend when both idle");

        // Load backend 0
        state.NotifyTaskDispatched(0);

        // Backend 1 now less loaded
        Ptr<SimpleTask> task2 = CreateObject<SimpleTask>();
        task2->SetTaskId(2);
        int32_t idx2 = scheduler->ScheduleTask(task2, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(idx2, 1, "Should pick backend 1 (less loaded)");

        // Load backend 1 — tie again
        state.NotifyTaskDispatched(1);

        Ptr<SimpleTask> task3 = CreateObject<SimpleTask>();
        task3->SetTaskId(3);
        int32_t idx3 = scheduler->ScheduleTask(task3, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(idx3, 0, "Should pick first backend on tie");

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
        cluster.AddBackend(nodes.Get(0),
                           InetSocketAddress(Ipv4Address("10.0.0.1"), 9000),
                           "GPU");
        cluster.AddBackend(nodes.Get(1),
                           InetSocketAddress(Ipv4Address("10.0.0.2"), 9000),
                           "TPU");
        cluster.AddBackend(nodes.Get(2),
                           InetSocketAddress(Ipv4Address("10.0.0.3"), 9000),
                           "GPU");

        ClusterState state;
        state.Resize(3);

        Ptr<LeastLoadedScheduler> scheduler = CreateObject<LeastLoadedScheduler>();

        // GPU task — picks first GPU backend (idx 0)
        Ptr<SimpleTask> task1 = CreateObject<SimpleTask>();
        task1->SetTaskId(1);
        task1->SetRequiredAcceleratorType("GPU");
        int32_t idx1 = scheduler->ScheduleTask(task1, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(idx1, 0, "Should pick first GPU backend");

        // Load GPU backend 0 — next GPU task goes to idx 2
        state.NotifyTaskDispatched(0);
        Ptr<SimpleTask> task2 = CreateObject<SimpleTask>();
        task2->SetTaskId(2);
        task2->SetRequiredAcceleratorType("GPU");
        int32_t idx2 = scheduler->ScheduleTask(task2, cluster, state);
        NS_TEST_ASSERT_MSG_EQ(idx2, 2, "Should pick second GPU backend (less loaded)");

        // Nonexistent type returns -1
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
