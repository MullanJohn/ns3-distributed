/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster-state.h"
#include "ns3/conservative-scaling-policy.h"
#include "ns3/double.h"
#include "ns3/scaling-policy.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test that ConservativeScalingPolicy steps up by one step (not to max).
 */
class ConservativeStepUpTestCase : public TestCase
{
  public:
    ConservativeStepUpTestCase()
        : TestCase("ConservativeScalingPolicy steps frequency up by one step")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<ConservativeScalingPolicy> policy = CreateObject<ConservativeScalingPolicy>();
        policy->SetAttribute("FrequencyStep", DoubleValue(50e6));

        ClusterState::BackendState backend;
        Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
        metrics->busy = true;
        metrics->frequency = 1.0e9;
        metrics->voltage = 1.0;
        backend.deviceMetrics = metrics;

        Ptr<ScalingDecision> decision = policy->Decide(backend);
        NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should produce a scaling decision");
        NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetFrequency,
                                  1.05e9,
                                  1e-3,
                                  "Should step up by 50 MHz, not jump to max");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test that ConservativeScalingPolicy steps down by one step (not to min).
 */
class ConservativeStepDownTestCase : public TestCase
{
  public:
    ConservativeStepDownTestCase()
        : TestCase("ConservativeScalingPolicy steps frequency down by one step")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<ConservativeScalingPolicy> policy = CreateObject<ConservativeScalingPolicy>();
        policy->SetAttribute("FrequencyStep", DoubleValue(50e6));

        ClusterState::BackendState backend;
        Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
        metrics->busy = false;
        metrics->queueLength = 0;
        metrics->frequency = 1.0e9;
        metrics->voltage = 1.0;
        backend.deviceMetrics = metrics;

        Ptr<ScalingDecision> decision = policy->Decide(backend);
        NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should produce a scaling decision");
        NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetFrequency,
                                  0.95e9,
                                  1e-3,
                                  "Should step down by 50 MHz, not jump to min");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test that ConservativeScalingPolicy computes correct voltage via linear V-F mapping.
 */
class ConservativeVoltageScalingTestCase : public TestCase
{
  public:
    ConservativeVoltageScalingTestCase()
        : TestCase("ConservativeScalingPolicy computes voltage via linear V-F mapping")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<ConservativeScalingPolicy> policy = CreateObject<ConservativeScalingPolicy>();
        policy->SetAttribute("FrequencyStep", DoubleValue(50e6));

        // Backend at min frequency (500 MHz), busy → step to 550 MHz
        ClusterState::BackendState backend;
        Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
        metrics->busy = true;
        metrics->frequency = 500e6;
        metrics->voltage = 0.8;
        backend.deviceMetrics = metrics;

        Ptr<ScalingDecision> decision = policy->Decide(backend);
        NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should produce a scaling decision");
        NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetFrequency, 550e6, 1e-3, "Should step to 550 MHz");
        // V = 0.8 + (550e6 - 500e6) / (1.5e9 - 500e6) * (1.1 - 0.8) = 0.815
        NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetVoltage,
                                  0.815,
                                  1e-6,
                                  "Voltage should be linearly mapped");
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
