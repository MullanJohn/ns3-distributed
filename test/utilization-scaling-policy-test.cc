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
#include "ns3/utilization-scaling-policy.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test UtilizationScalingPolicy maps activeTasks to the min or max performance state.
 */
class UtilizationScalingOppTestCase : public TestCase
{
  public:
    UtilizationScalingOppTestCase()
        : TestCase("UtilizationScalingPolicy selects min-max OPP with voltage from table")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<UtilizationScalingPolicy> policy = CreateObject<UtilizationScalingPolicy>();

        {
            ClusterState::BackendState backend;
            backend.device.numPerformanceStates = 3;
            backend.device.currentStateIdx = 0;
            backend.orchestration.activeTasks = 1;

            Ptr<ScalingDecision> decision = policy->Decide(0, backend);
            NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should scale up when busy");
            NS_TEST_ASSERT_MSG_EQ(decision->targetStateIdx,
                                  2,
                                  "Should select max state index");
        }

        {
            ClusterState::BackendState backend;
            backend.device.numPerformanceStates = 3;
            backend.device.currentStateIdx = 2;
            backend.orchestration.activeTasks = 0;

            Ptr<ScalingDecision> decision = policy->Decide(0, backend);
            NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should scale down when idle");
            NS_TEST_ASSERT_MSG_EQ(decision->targetStateIdx,
                                  0,
                                  "Should select min state index");
        }

        {
            ClusterState::BackendState backend;
            backend.device.numPerformanceStates = 3;
            backend.device.currentStateIdx = 2;
            backend.orchestration.activeTasks = 1;

            Ptr<ScalingDecision> decision = policy->Decide(0, backend);
            NS_TEST_ASSERT_MSG_EQ(decision,
                                  nullptr,
                                  "Should return nullptr when already at target");
        }
    }
};

} // namespace

TestCase*
CreateUtilizationScalingOppTestCase()
{
    return new UtilizationScalingOppTestCase;
}

} // namespace ns3
