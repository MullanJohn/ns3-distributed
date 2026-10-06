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
#include "ns3/scaling-policy.h"
#include "ns3/simple-task.h"
#include "ns3/test.h"
#include "ns3/tiered-delay-scheduler.h"

namespace ns3
{
namespace
{

// Builds a SimpleTask with the given required accelerator type (empty = any) and
// compute demand, used to run the actual decision under test.
Ptr<SimpleTask>
MakeTask(uint64_t taskId, const std::string& requiredType, double computeDemand)
{
    Ptr<SimpleTask> task = CreateObject<SimpleTask>();
    task->SetTaskId(taskId);
    task->SetRequiredAcceleratorType(requiredType);
    task->SetComputeDemand(computeDemand);
    return task;
}

/**
 * @ingroup continuum-tests
 * @brief Test TieredDelayScheduler stays in the preferred tier while under threshold.
 */
class TieredDelaySchedulerStaysUnderThresholdTestCase : public TestCase
{
  public:
    TieredDelaySchedulerStaysUnderThresholdTestCase()
        : TestCase("TieredDelayScheduler stays in the preferred tier while under threshold")
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
        // Backend 0 ("edge", tier 0). Backend 1 ("cloud", tier 1).
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000), "",
                           Seconds(0));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000), "",
                           Seconds(1));

        ClusterState state;
        state.Resize(2);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);

        // Backend 0's queued compute drains in 50ms (5e10 / 1e12 FLOP/s),
        // comfortably under the default 100ms OverflowQueueDelay.
        state.NotifyTaskDispatched(0, 5.0e10);

        Ptr<TieredDelayScheduler> scheduler = CreateObject<TieredDelayScheduler>();
        int32_t result = scheduler->ScheduleTask(MakeTask(0, "", 1.0e11), cluster, state);

        NS_TEST_ASSERT_MSG_EQ(result, 0, "Backend 0 (edge) has spare capacity, should stay there");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test TieredDelayScheduler overflows once queue drain time reaches the threshold.
 */
class TieredDelaySchedulerOverflowsAtThresholdTestCase : public TestCase
{
  public:
    TieredDelaySchedulerOverflowsAtThresholdTestCase()
        : TestCase("TieredDelayScheduler overflows once queue drain time reaches the threshold")
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
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000), "",
                           Seconds(0));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000), "",
                           Seconds(1));

        ClusterState state;
        state.Resize(2);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);

        // Backend 0's queued compute drains in exactly 100ms (1e11 / 1e12
        // FLOP/s), at/above the default 100ms OverflowQueueDelay.
        state.NotifyTaskDispatched(0, 1.0e11);

        Ptr<TieredDelayScheduler> scheduler = CreateObject<TieredDelayScheduler>();
        int32_t result = scheduler->ScheduleTask(MakeTask(0, "", 1.0e11), cluster, state);

        NS_TEST_ASSERT_MSG_EQ(result,
                              1,
                              "Backend 0 (edge) is saturated, task should overflow to backend 1");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test TieredDelayScheduler picks the lowest-drain-time backend within a tier.
 */
class TieredDelaySchedulerIntraTierLowestUtilTestCase : public TestCase
{
  public:
    TieredDelaySchedulerIntraTierLowestUtilTestCase()
        : TestCase("TieredDelayScheduler picks lowest-drain-time backend within a tier")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(3);
        InternetStackHelper internet;
        internet.Install(nodes);

        // Backend 0 and 1 share tier 0 (commDelay=0); backend 2 is tier 1.
        Cluster cluster;
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000), "",
                           Seconds(0));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000), "",
                           Seconds(0));
        cluster.AddBackend(nodes.Get(2), InetSocketAddress(Ipv4Address("10.0.0.3"), 9000), "",
                           Seconds(1));

        ClusterState state;
        state.Resize(3);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);
        state.SetBackendCapability(2, 1.0e12);

        // Backend 0 has some queued work (drain=50ms); backend 1 is idle
        // (drain=0), so it should be preferred within tier 0 even though
        // neither is anywhere near overflowing.
        state.NotifyTaskDispatched(0, 5.0e10);

        Ptr<TieredDelayScheduler> scheduler = CreateObject<TieredDelayScheduler>();
        int32_t result = scheduler->ScheduleTask(MakeTask(0, "", 1.0e11), cluster, state);

        NS_TEST_ASSERT_MSG_EQ(result,
                              1,
                              "Backend 1 has a lower drain time than backend 0 within tier 0");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test TieredDelayScheduler breaks exact drain-time ties randomly.
 */
class TieredDelaySchedulerRandomTieBreakTestCase : public TestCase
{
  public:
    TieredDelaySchedulerRandomTieBreakTestCase()
        : TestCase("TieredDelayScheduler breaks exact drain-time ties randomly")
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

        Ptr<TieredDelayScheduler> scheduler = CreateObject<TieredDelayScheduler>();

        // Neither backend is ever dispatched to, so both have zero queued
        // work (drain=0) on every call -- the tie persists across the loop.
        bool sawZero = false;
        bool sawOne = false;
        for (uint64_t i = 0; i < 30; i++)
        {
            int32_t result = scheduler->ScheduleTask(MakeTask(i, "", 1.0e11), cluster, state);
            NS_TEST_ASSERT_MSG_EQ((result == 0 || result == 1),
                                  true,
                                  "Result must be one of the two tied backends");
            sawZero = sawZero || (result == 0);
            sawOne = sawOne || (result == 1);
        }

        NS_TEST_ASSERT_MSG_EQ(sawZero && sawOne,
                              true,
                              "Both tied backends should be chosen at least once across 30 "
                              "calls (probability of a fair coin missing this is 0.5^29)");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test TieredDelayScheduler always uses the last tier, never rejecting a task.
 */
class TieredDelaySchedulerLastTierCatchAllTestCase : public TestCase
{
  public:
    TieredDelaySchedulerLastTierCatchAllTestCase()
        : TestCase("TieredDelayScheduler last tier is a catch-all, never rejects")
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
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000), "",
                           Seconds(0));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000), "",
                           Seconds(1));

        ClusterState state;
        state.Resize(2);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);

        // Saturate both backends independently: drain=150ms each, at/above
        // the default 100ms OverflowQueueDelay.
        state.NotifyTaskDispatched(0, 1.5e11);
        state.NotifyTaskDispatched(1, 1.5e11);

        Ptr<TieredDelayScheduler> scheduler = CreateObject<TieredDelayScheduler>();
        int32_t result = scheduler->ScheduleTask(MakeTask(0, "", 1.0e11), cluster, state);

        NS_TEST_ASSERT_MSG_EQ(result,
                              1,
                              "Last tier (backend 1) must still be used even at full saturation");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test TieredDelayScheduler filters by accelerator type before tiering.
 */
class TieredDelaySchedulerTypeFilterTestCase : public TestCase
{
  public:
    TieredDelaySchedulerTypeFilterTestCase()
        : TestCase("TieredDelayScheduler filters by accelerator type before tiering")
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
        // Backend 0: GPU, tier 0 (commDelay=0). Backend 1: TPU, tier 0 (commDelay=0),
        // must never be a candidate for a GPU-only task. Backend 2: GPU, tier 1.
        cluster.AddBackend(nodes.Get(0),
                           InetSocketAddress(Ipv4Address("10.0.0.1"), 9000),
                           "GPU",
                           Seconds(0));
        cluster.AddBackend(nodes.Get(1),
                           InetSocketAddress(Ipv4Address("10.0.0.2"), 9000),
                           "TPU",
                           Seconds(0));
        cluster.AddBackend(nodes.Get(2),
                           InetSocketAddress(Ipv4Address("10.0.0.3"), 9000),
                           "GPU",
                           Seconds(1));

        ClusterState state;
        state.Resize(3);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);
        state.SetBackendCapability(2, 1.0e12);

        Ptr<TieredDelayScheduler> scheduler = CreateObject<TieredDelayScheduler>();

        int32_t gpuResult =
            scheduler->ScheduleTask(MakeTask(1, "GPU", 1.0e11), cluster, state);
        NS_TEST_ASSERT_MSG_EQ(gpuResult,
                              0,
                              "Lowest-delay GPU backend should be chosen; TPU backend excluded");

        int32_t noMatchResult =
            scheduler->ScheduleTask(MakeTask(2, "FPGA", 1.0e11), cluster, state);
        NS_TEST_ASSERT_MSG_EQ(noMatchResult, -1, "No FPGA backend exists");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test TieredDelayScheduler latches a backend into overflow until its
 * queue drains down to RecoverQueueDelay, rather than snapping back to
 * "preferred" the instant it dips under OverflowQueueDelay.
 */
class TieredDelaySchedulerHysteresisTestCase : public TestCase
{
  public:
    TieredDelaySchedulerHysteresisTestCase()
        : TestCase("TieredDelayScheduler latches overflow until load drops to RecoverQueueDelay")
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
        cluster.AddBackend(nodes.Get(0), InetSocketAddress(Ipv4Address("10.0.0.1"), 9000), "",
                           Seconds(0));
        cluster.AddBackend(nodes.Get(1), InetSocketAddress(Ipv4Address("10.0.0.2"), 9000), "",
                           Seconds(1));

        ClusterState state;
        state.Resize(2);
        state.SetBackendCapability(0, 1.0e12);
        state.SetBackendCapability(1, 1.0e12);

        Ptr<TieredDelayScheduler> scheduler = CreateObject<TieredDelayScheduler>();

        // Push backend 0 into overflow with three separately-tracked tasks
        // (1.3e11 + 3e10 + 4e10 = 2e11 total, drain=200ms), at/above the
        // default 100ms OverflowQueueDelay. Kept as separate dispatch calls
        // (rather than one lump sum) so each can be completed individually
        // below, matching NotifyTaskDispatched/NotifyTaskCompleted's 1:1
        // activeTasks bookkeeping.
        state.NotifyTaskDispatched(0, 1.3e11);
        state.NotifyTaskDispatched(0, 3.0e10);
        state.NotifyTaskDispatched(0, 4.0e10);
        int32_t saturated = scheduler->ScheduleTask(MakeTask(0, "", 1.0e11), cluster, state);
        NS_TEST_ASSERT_MSG_EQ(saturated, 1, "Backend 0 saturated, task should overflow to backend 1");

        // Complete the 1.3e11 task, leaving 7e10 queued (drain=70ms) --
        // below OverflowQueueDelay (100ms) but still above RecoverQueueDelay
        // (50ms). The hysteresis latch should keep it marked overflowing.
        state.NotifyTaskCompleted(0, 1.3e11);
        int32_t stillLatched = scheduler->ScheduleTask(MakeTask(1, "", 1.0e11), cluster, state);
        NS_TEST_ASSERT_MSG_EQ(stillLatched,
                              1,
                              "Backend 0's drain time (70ms) sits between RecoverQueueDelay and "
                              "OverflowQueueDelay -- hysteresis should keep it latched into "
                              "overflow rather than snapping back immediately");

        // Complete the 3e10 task, leaving 4e10 queued (drain=40ms) --
        // at/below RecoverQueueDelay (50ms), so it should now recover.
        state.NotifyTaskCompleted(0, 3.0e10);
        int32_t recovered = scheduler->ScheduleTask(MakeTask(2, "", 1.0e11), cluster, state);
        NS_TEST_ASSERT_MSG_EQ(recovered,
                              0,
                              "Backend 0's drain time (40ms) has dropped to/below "
                              "RecoverQueueDelay, so it should recover and be preferred again");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test TieredDelayScheduler::GetName().
 */
class TieredDelaySchedulerNameTestCase : public TestCase
{
  public:
    TieredDelaySchedulerNameTestCase()
        : TestCase("TieredDelayScheduler::GetName() returns TieredDelay")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<TieredDelayScheduler> scheduler = CreateObject<TieredDelayScheduler>();
        NS_TEST_ASSERT_MSG_EQ(scheduler->GetName(),
                              "TieredDelay",
                              "Scheduler name should be TieredDelay");
    }
};

} // namespace

TestCase*
CreateTieredDelaySchedulerStaysUnderThresholdTestCase()
{
    return new TieredDelaySchedulerStaysUnderThresholdTestCase;
}

TestCase*
CreateTieredDelaySchedulerOverflowsAtThresholdTestCase()
{
    return new TieredDelaySchedulerOverflowsAtThresholdTestCase;
}

TestCase*
CreateTieredDelaySchedulerIntraTierLowestUtilTestCase()
{
    return new TieredDelaySchedulerIntraTierLowestUtilTestCase;
}

TestCase*
CreateTieredDelaySchedulerRandomTieBreakTestCase()
{
    return new TieredDelaySchedulerRandomTieBreakTestCase;
}

TestCase*
CreateTieredDelaySchedulerLastTierCatchAllTestCase()
{
    return new TieredDelaySchedulerLastTierCatchAllTestCase;
}

TestCase*
CreateTieredDelaySchedulerTypeFilterTestCase()
{
    return new TieredDelaySchedulerTypeFilterTestCase;
}

TestCase*
CreateTieredDelaySchedulerHysteresisTestCase()
{
    return new TieredDelaySchedulerHysteresisTestCase;
}

TestCase*
CreateTieredDelaySchedulerNameTestCase()
{
    return new TieredDelaySchedulerNameTestCase;
}

} // namespace ns3
