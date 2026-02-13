/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "utilization-scaling-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UtilizationScalingPolicy");

NS_OBJECT_ENSURE_REGISTERED(UtilizationScalingPolicy);

TypeId
UtilizationScalingPolicy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::UtilizationScalingPolicy")
                            .SetParent<ScalingPolicy>()
                            .SetGroupName("Distributed")
                            .AddConstructor<UtilizationScalingPolicy>();
    return tid;
}

UtilizationScalingPolicy::UtilizationScalingPolicy()
{
    NS_LOG_FUNCTION(this);
}

UtilizationScalingPolicy::~UtilizationScalingPolicy()
{
    NS_LOG_FUNCTION(this);
}

Ptr<ScalingDecision>
UtilizationScalingPolicy::Decide(Ptr<DeviceMetrics> metrics,
                                  double minFrequency,
                                  double maxFrequency)
{
    NS_LOG_FUNCTION(this << metrics->frequency << metrics->busy << metrics->queueLength);

    double targetFreq = (metrics->busy || metrics->queueLength > 0) ? maxFrequency : minFrequency;

    if (targetFreq == metrics->frequency)
    {
        return nullptr; // No change needed
    }

    Ptr<ScalingDecision> decision = Create<ScalingDecision>();
    decision->targetFrequency = targetFreq;
    decision->targetVoltage = metrics->voltage; // Pass through unchanged
    return decision;
}

std::string
UtilizationScalingPolicy::GetName() const
{
    return "UtilizationScaling";
}

} // namespace ns3
