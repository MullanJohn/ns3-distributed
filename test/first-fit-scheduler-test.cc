/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster-state.h"
#include "ns3/cluster.h"
#include "ns3/first-fit-scheduler.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/scaling-policy.h"
#include "ns3/simple-task.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
 * @brief Test FirstFitScheduler round-robin selection across untyped, typed and unmatched cases.
 */
class FirstFitSchedulerTestCase : public TestCase
{
  public:
    FirstFitSchedulerTestCase()
        : TestCase("FirstFitScheduler round-robin with type filtering")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FirstFitScheduler> scheduler = CreateObject<FirstFitScheduler>();

        {
            Cluster empty;
            ClusterState state;
            Ptr<SimpleTask> task = CreateObject<SimpleTask>();
            task->SetTaskId(1);
            NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(task, empty, state),
                                  -1,
                                  "Empty cluster returns -1");
        }

        NodeContainer untypedNodes;
        untypedNodes.Create(3);
        InternetStackHelper internet;
        internet.Install(untypedNodes);

        Cluster untyped;
        untyped.AddBackend(untypedNodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000));
        untyped.AddBackend(untypedNodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000));
        untyped.AddBackend(untypedNodes.Get(2), InetSocketAddress(Ipv4Address("10.0.0.3"), 9000));
        ClusterState untypedState;
        untypedState.Resize(3);

        for (uint32_t i = 0; i < 7; ++i)
        {
            Ptr<SimpleTask> task = CreateObject<SimpleTask>();
            task->SetTaskId(i + 1);
            int32_t idx = scheduler->ScheduleTask(task, untyped, untypedState);
            NS_TEST_ASSERT_MSG_EQ(static_cast<uint32_t>(idx),
                                  i % 3,
                                  "Round-robin advances modulo backend count");
        }

        // Mixed-type cluster: per-type counters advance independently.
        NodeContainer mixedNodes;
        mixedNodes.Create(4);
        internet.Install(mixedNodes);

        Cluster mixed;
        mixed.AddBackend(mixedNodes.Get(0), InetSocketAddress(Ipv4Address("10.0.1.1"), 9000), "GPU");
        mixed.AddBackend(mixedNodes.Get(1), InetSocketAddress(Ipv4Address("10.0.1.2"), 9000), "TPU");
        mixed.AddBackend(mixedNodes.Get(2), InetSocketAddress(Ipv4Address("10.0.1.3"), 9000), "GPU");
        mixed.AddBackend(mixedNodes.Get(3), InetSocketAddress(Ipv4Address("10.0.1.4"), 9000), "TPU");
        ClusterState mixedState;
        mixedState.Resize(4);

        Ptr<SimpleTask> gpu1 = CreateObject<SimpleTask>();
        gpu1->SetRequiredAcceleratorType("GPU");
        Ptr<SimpleTask> gpu2 = CreateObject<SimpleTask>();
        gpu2->SetRequiredAcceleratorType("GPU");
        Ptr<SimpleTask> gpu3 = CreateObject<SimpleTask>();
        gpu3->SetRequiredAcceleratorType("GPU");
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(gpu1, mixed, mixedState), 0, "GPU#1 → 0");
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(gpu2, mixed, mixedState), 2, "GPU#2 → 2");
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(gpu3, mixed, mixedState), 0, "GPU#3 wraps");

        Ptr<SimpleTask> tpu1 = CreateObject<SimpleTask>();
        tpu1->SetRequiredAcceleratorType("TPU");
        Ptr<SimpleTask> tpu2 = CreateObject<SimpleTask>();
        tpu2->SetRequiredAcceleratorType("TPU");
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(tpu1, mixed, mixedState), 1, "TPU#1 → 1");
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(tpu2, mixed, mixedState), 3, "TPU#2 → 3");

        Ptr<SimpleTask> fpga = CreateObject<SimpleTask>();
        fpga->SetRequiredAcceleratorType("FPGA");
        NS_TEST_ASSERT_MSG_EQ(scheduler->ScheduleTask(fpga, mixed, mixedState),
                              -1,
                              "Unmatched type returns -1");

        NS_TEST_ASSERT_MSG_EQ(scheduler->GetName(), "FirstFit", "Scheduler name");
    }
};

} // namespace

TestCase*
CreateFirstFitSchedulerTestCase()
{
    return new FirstFitSchedulerTestCase;
}

} // namespace ns3
