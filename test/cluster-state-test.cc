/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster-state.h"
#include "ns3/scaling-policy.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test ClusterState backend bookkeeping, reservation accounting and Clear.
 */
class ClusterStateTestCase : public TestCase
{
  public:
    ClusterStateTestCase()
        : TestCase("ClusterState bookkeeping, reservations, and reset")
    {
    }

  private:
    void DoRun() override
    {
        ClusterState state;

        NS_TEST_ASSERT_MSG_EQ(state.GetN(), 0, "Initially empty");
        state.Resize(2);
        NS_TEST_ASSERT_MSG_EQ(state.GetN(), 2, "Resize(2) gives two backends");

        state.SetBackendCapability(0, 1.0e12);
        state.SetPerformanceStates(0, 4, 2.0e12);
        state.SetCurrentState(0, 2, 1.5e12);
        const ClusterState::BackendState& b0 = state.Get(0);
        NS_TEST_ASSERT_MSG_EQ_TOL(b0.device.nominalComputeRate, 1.0e12, 1e-3, "nominal rate");
        NS_TEST_ASSERT_MSG_EQ(b0.device.numPerformanceStates, 4, "perf states");
        NS_TEST_ASSERT_MSG_EQ(b0.device.currentStateIdx, 2, "current state idx");
        NS_TEST_ASSERT_MSG_EQ_TOL(b0.device.effectiveComputeRate, 1.5e12, 1e-3, "effective rate");

        state.NotifyTaskDispatched(0, 1.0e11);
        state.NotifyTaskDispatched(0, 5.0e10);
        state.NotifyTaskDispatched(1, 2.0e11);
        NS_TEST_ASSERT_MSG_EQ(state.Get(0).orchestration.activeTasks, 2, "two dispatches on 0");
        NS_TEST_ASSERT_MSG_EQ_TOL(state.Get(0).orchestration.estimatedRemainingComputeWork,
                                  1.5e11,
                                  1e-3,
                                  "work accumulates");

        state.NotifyTaskCompleted(0, 1.0e11);
        NS_TEST_ASSERT_MSG_EQ(state.Get(0).orchestration.activeTasks, 1, "complete decrements");
        NS_TEST_ASSERT_MSG_EQ_TOL(state.Get(0).orchestration.estimatedRemainingComputeWork,
                                  5.0e10,
                                  1e-3,
                                  "work decrements by completed demand");

        state.UndoTaskDispatched(1, 2.0e11);
        NS_TEST_ASSERT_MSG_EQ(state.Get(1).orchestration.activeTasks, 0, "undo decrements");
        NS_TEST_ASSERT_MSG_EQ_TOL(state.Get(1).orchestration.estimatedRemainingComputeWork,
                                  0.0,
                                  1e-9,
                                  "undo zeroes work");

        // Completion with demand exceeding tracked work clamps to zero, never negative.
        state.NotifyTaskCompleted(0, 5.0e11);
        NS_TEST_ASSERT_MSG_EQ_TOL(state.Get(0).orchestration.estimatedRemainingComputeWork,
                                  0.0,
                                  1e-9,
                                  "remaining work clamped, never negative");

        // Per-type reservation accounting.
        state.ReserveTasks(2, "GPU");
        state.ReserveTasks(3, "TPU");
        state.ReserveTasks(1);
        NS_TEST_ASSERT_MSG_EQ(state.GetPendingAdmissionTasks(), 6, "global pending");
        NS_TEST_ASSERT_MSG_EQ(state.GetPendingAdmissionTasksForType("GPU"), 2, "GPU pending");
        NS_TEST_ASSERT_MSG_EQ(state.GetPendingAdmissionTasksForType("FPGA"),
                              0,
                              "unknown type returns 0");

        state.ReleaseTasks(2, "GPU");
        NS_TEST_ASSERT_MSG_EQ(state.GetPendingAdmissionTasksForType("GPU"),
                              0,
                              "type entry erased after full release");
        NS_TEST_ASSERT_MSG_EQ(state.GetPendingAdmissionTasks(), 4, "global pending decremented");

        state.SetActiveWorkloadCount(7);
        NS_TEST_ASSERT_MSG_EQ(state.GetActiveWorkloadCount(), 7, "workload count round-trips");

        state.Clear();
        NS_TEST_ASSERT_MSG_EQ(state.GetN(), 0, "Clear empties backends");
        NS_TEST_ASSERT_MSG_EQ(state.GetPendingAdmissionTasks(), 0, "Clear empties reservations");
        NS_TEST_ASSERT_MSG_EQ(state.GetActiveWorkloadCount(), 0, "Clear resets workload count");
    }
};

} // namespace

TestCase*
CreateClusterStateTestCase()
{
    return new ClusterStateTestCase;
}

} // namespace ns3
