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
UtilizationScalingPolicy::Decide(const ClusterState::BackendState& backend,
                                 const std::vector<OperatingPoint>& opps)
{
    NS_LOG_FUNCTION(this);

    if (opps.size() < 2)
    {
        return nullptr;
    }

    bool busy = backend.activeTasks > 0;
    double currentFreq = backend.commandedFrequency;

    const OperatingPoint& target = busy ? opps.back() : opps.front();

    if (target.frequency == currentFreq)
    {
        return nullptr;
    }

    Ptr<ScalingDecision> decision = Create<ScalingDecision>();
    decision->targetFrequency = target.frequency;
    decision->targetVoltage = target.voltage;
    return decision;
}

} // namespace ns3
