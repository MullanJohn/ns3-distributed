/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/accelerator.h"
#include "ns3/cluster-state.h"
#include "ns3/scaling-policy.h"
#include "ns3/test.h"
#include "ns3/utilization-scaling-policy.h"

#include <vector>

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test that UtilizationScalingPolicy selects min/max OPP with correct voltage.
 */
class UtilizationScalingOppTestCase : public TestCase
{
  public:
    UtilizationScalingOppTestCase()
        : TestCase("UtilizationScalingPolicy selects min/max OPP with voltage from table")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<UtilizationScalingPolicy> policy = CreateObject<UtilizationScalingPolicy>();
        std::vector<OperatingPoint> opps = {{500e6, 0.65}, {1.0e9, 0.85}, {1.5e9, 1.05}};

        {
            ClusterState::BackendState backend;
            Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
            metrics->busy = true;
            metrics->frequency = 500e6;
            metrics->voltage = 0.65;
            backend.deviceMetrics = metrics;

            Ptr<ScalingDecision> decision = policy->Decide(backend, opps);
            NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should scale up when busy");
            NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetFrequency,
                                      1.5e9,
                                      1e-3,
                                      "Should select max OPP frequency");
            NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetVoltage,
                                      1.05,
                                      1e-6,
                                      "Should select max OPP voltage");
        }

        {
            ClusterState::BackendState backend;
            Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
            metrics->busy = false;
            metrics->queueLength = 0;
            metrics->frequency = 1.5e9;
            metrics->voltage = 1.05;
            backend.deviceMetrics = metrics;

            Ptr<ScalingDecision> decision = policy->Decide(backend, opps);
            NS_TEST_ASSERT_MSG_NE(decision, nullptr, "Should scale down when idle");
            NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetFrequency,
                                      500e6,
                                      1e-3,
                                      "Should select min OPP frequency");
            NS_TEST_ASSERT_MSG_EQ_TOL(decision->targetVoltage,
                                      0.65,
                                      1e-6,
                                      "Should select min OPP voltage");
        }

        {
            ClusterState::BackendState backend;
            Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
            metrics->busy = true;
            metrics->frequency = 1.5e9;
            metrics->voltage = 1.05;
            backend.deviceMetrics = metrics;

            Ptr<ScalingDecision> decision = policy->Decide(backend, opps);
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
