/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster-state.h"
#include "ns3/conservative-scaling-policy.h"
#include "ns3/scaling-policy.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
 * @brief Test ConservativeScalingPolicy steps up by one state.
 */
class ConservativeStepUpTestCase : public TestCase
{
  public:
    ConservativeStepUpTestCase()
        : TestCase("ConservativeScalingPolicy steps up by one state")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<ConservativeScalingPolicy> policy = CreateObject<ConservativeScalingPolicy>();

        ClusterState::BackendState backend;
        backend.device.numPerformanceStates = 3;
        backend.device.currentStateIdx = 1;
        backend.device.nominalComputeRate = 1e12;
        backend.orchestration.activeTasks = 1;
        backend.orchestration.estimatedRemainingComputeWork = 1e12;

        Ptr<ScalingDecision> decision = policy->Decide(0, backend);
        NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should produce a scaling decision");
        NS_TEST_ASSERT_MSG_EQ(decision->targetStateIdx, 2, "Should step up to next state");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test ConservativeScalingPolicy steps down by one state.
 */
class ConservativeStepDownTestCase : public TestCase
{
  public:
    ConservativeStepDownTestCase()
        : TestCase("ConservativeScalingPolicy steps down by one state")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<ConservativeScalingPolicy> policy = CreateObject<ConservativeScalingPolicy>();

        ClusterState::BackendState backend;
        backend.device.numPerformanceStates = 3;
        backend.device.currentStateIdx = 1;
        backend.device.nominalComputeRate = 1e12;
        backend.orchestration.activeTasks = 0;

        Ptr<ScalingDecision> decision = policy->Decide(0, backend);
        NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should produce a scaling decision");
        NS_TEST_ASSERT_MSG_EQ(decision->targetStateIdx, 0, "Should step down to previous state");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test ConservativeScalingPolicy returns nullptr when already at the target state.
 */
class ConservativeBoundaryStateTestCase : public TestCase
{
  public:
    ConservativeBoundaryStateTestCase()
        : TestCase("ConservativeScalingPolicy returns nullptr at boundary state")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<ConservativeScalingPolicy> policy = CreateObject<ConservativeScalingPolicy>();

        ClusterState::BackendState backend;
        backend.device.numPerformanceStates = 3;
        backend.device.currentStateIdx = 2;
        backend.device.nominalComputeRate = 1e12;
        backend.orchestration.activeTasks = 1;
        backend.orchestration.estimatedRemainingComputeWork = 1e12;

        Ptr<ScalingDecision> decision = policy->Decide(0, backend);
        NS_TEST_ASSERT_MSG_EQ(decision, nullptr, "Should return nullptr at max state");
    }
};

} // namespace

TestCase*
CreateConservativeStepUpTestCase()
{
    return new ConservativeStepUpTestCase;
}

TestCase*
CreateConservativeStepDownTestCase()
{
    return new ConservativeStepDownTestCase;
}

TestCase*
CreateConservativeBoundaryStateTestCase()
{
    return new ConservativeBoundaryStateTestCase;
}

} // namespace ns3
