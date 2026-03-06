/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/accelerator.h"
#include "ns3/cluster-state.h"
#include "ns3/conservative-scaling-policy.h"
#include "ns3/scaling-policy.h"
#include "ns3/test.h"

#include <vector>

namespace ns3
{
namespace
{

static std::vector<OperatingPoint>
MakeTestOpps()
{
    return {{500e6, 0.65}, {1.0e9, 0.85}, {1.5e9, 1.05}};
}

/**
 * @ingroup distributed-tests
 * @brief Test that ConservativeScalingPolicy steps up by one OPP.
 */
class ConservativeStepUpTestCase : public TestCase
{
  public:
    ConservativeStepUpTestCase()
        : TestCase("ConservativeScalingPolicy steps frequency up by one OPP")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<ConservativeScalingPolicy> policy = CreateObject<ConservativeScalingPolicy>();
        std::vector<OperatingPoint> opps = MakeTestOpps();

        ClusterState::BackendState backend;
        Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
        metrics->busy = true;
        metrics->frequency = 1.0e9;
        metrics->voltage = 0.85;
        backend.deviceMetrics = metrics;

        Ptr<ScalingDecision> decision = policy->Decide(backend, opps);
        NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should produce a scaling decision");
        NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetFrequency,
                                  1.5e9,
                                  1e-3,
                                  "Should step up to next OPP");
        NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetVoltage,
                                  1.05,
                                  1e-6,
                                  "Voltage should come from OPP table");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test that ConservativeScalingPolicy steps down by one OPP.
 */
class ConservativeStepDownTestCase : public TestCase
{
  public:
    ConservativeStepDownTestCase()
        : TestCase("ConservativeScalingPolicy steps frequency down by one OPP")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<ConservativeScalingPolicy> policy = CreateObject<ConservativeScalingPolicy>();
        std::vector<OperatingPoint> opps = MakeTestOpps();

        ClusterState::BackendState backend;
        Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
        metrics->busy = false;
        metrics->queueLength = 0;
        metrics->frequency = 1.0e9;
        metrics->voltage = 0.85;
        backend.deviceMetrics = metrics;

        Ptr<ScalingDecision> decision = policy->Decide(backend, opps);
        NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should produce a scaling decision");
        NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetFrequency,
                                  500e6,
                                  1e-3,
                                  "Should step down to previous OPP");
        NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetVoltage,
                                  0.65,
                                  1e-6,
                                  "Voltage should come from OPP table");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test that ConservativeScalingPolicy returns nullptr at max OPP when busy.
 */
class ConservativeVoltageScalingTestCase : public TestCase
{
  public:
    ConservativeVoltageScalingTestCase()
        : TestCase("ConservativeScalingPolicy returns nullptr at boundary OPP")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<ConservativeScalingPolicy> policy = CreateObject<ConservativeScalingPolicy>();
        std::vector<OperatingPoint> opps = MakeTestOpps();

        ClusterState::BackendState backend;
        Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
        metrics->busy = true;
        metrics->frequency = 1.5e9;
        metrics->voltage = 1.05;
        backend.deviceMetrics = metrics;

        Ptr<ScalingDecision> decision = policy->Decide(backend, opps);
        NS_TEST_ASSERT_MSG_EQ(decision, nullptr, "Should return nullptr at max OPP");
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
CreateConservativeVoltageScalingTestCase()
{
    return new ConservativeVoltageScalingTestCase;
}

} // namespace ns3
