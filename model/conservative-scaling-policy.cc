/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "conservative-scaling-policy.h"

#include "ns3/log.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ConservativeScalingPolicy");

NS_OBJECT_ENSURE_REGISTERED(ConservativeScalingPolicy);

TypeId
ConservativeScalingPolicy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ConservativeScalingPolicy")
                            .SetParent<ScalingPolicy>()
                            .SetGroupName("Distributed")
                            .AddConstructor<ConservativeScalingPolicy>();
    return tid;
}

ConservativeScalingPolicy::ConservativeScalingPolicy()
{
    NS_LOG_FUNCTION(this);
}

ConservativeScalingPolicy::~ConservativeScalingPolicy()
{
    NS_LOG_FUNCTION(this);
}

Ptr<ScalingDecision>
ConservativeScalingPolicy::Decide(const ClusterState::BackendState& backend,
                                  const std::vector<OperatingPoint>& opps)
{
    NS_LOG_FUNCTION(this);

    if (opps.size() < 2)
    {
        return nullptr;
    }

    bool busy;
    double currentFreq;

    Ptr<DeviceMetrics> metrics = backend.deviceMetrics;
    if (metrics)
    {
        busy = metrics->busy || metrics->queueLength > 0;
        currentFreq = metrics->frequency;
    }
    else
    {
        busy = backend.activeTasks > 0;
        currentFreq = opps.front().frequency;
    }

    size_t currentIdx = 0;
    double minDist = std::abs(opps[0].frequency - currentFreq);
    for (size_t i = 1; i < opps.size(); i++)
    {
        double dist = std::abs(opps[i].frequency - currentFreq);
        if (dist < minDist)
        {
            minDist = dist;
            currentIdx = i;
        }
    }

    size_t targetIdx;

    if (busy && currentIdx < opps.size() - 1)
    {
        targetIdx = currentIdx + 1;
    }
    else if (!busy && currentIdx > 0)
    {
        targetIdx = currentIdx - 1;
    }
    else
    {
        return nullptr;
    }

    Ptr<ScalingDecision> decision = Create<ScalingDecision>();
    decision->targetFrequency = opps[targetIdx].frequency;
    decision->targetVoltage = opps[targetIdx].voltage;
    return decision;
}

} // namespace ns3
