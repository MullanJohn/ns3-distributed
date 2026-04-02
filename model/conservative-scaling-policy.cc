/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "conservative-scaling-policy.h"

#include "ns3/double.h"
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
    static TypeId tid =
        TypeId("ns3::ConservativeScalingPolicy")
            .SetParent<ScalingPolicy>()
            .SetGroupName("Distributed")
            .AddConstructor<ConservativeScalingPolicy>()
            .AddAttribute("TargetDrainTime",
                          "Estimated drain time (seconds at nominal rate) that maps to the "
                          "highest OPP.",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&ConservativeScalingPolicy::m_targetDrainTime),
                          MakeDoubleChecker<double>(0.0));
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
ConservativeScalingPolicy::Decide(uint32_t /* backendIdx */,
                                  const ClusterState::BackendState& backend,
                                  const std::vector<OperatingPoint>& opps)
{
    NS_LOG_FUNCTION(this);

    if (opps.size() < 2)
    {
        return nullptr;
    }

    double currentFreq = backend.device.commandedFrequency;

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

    size_t targetIdx = 0;
    if (backend.device.nominalComputeRate > 0.0 && m_targetDrainTime > 0.0)
    {
        double drainTime = backend.orchestration.estimatedRemainingComputeWork /
                           backend.device.nominalComputeRate;
        double loadRatio = std::min(drainTime / m_targetDrainTime, 1.0);
        targetIdx = static_cast<size_t>(std::round(loadRatio * (opps.size() - 1)));
    }

    size_t nextIdx;
    if (currentIdx < targetIdx)
    {
        nextIdx = currentIdx + 1;
    }
    else if (currentIdx > targetIdx)
    {
        nextIdx = currentIdx - 1;
    }
    else
    {
        return nullptr;
    }

    Ptr<ScalingDecision> decision = Create<ScalingDecision>();
    decision->targetFrequency = opps[nextIdx].frequency;
    decision->targetVoltage = opps[nextIdx].voltage;
    return decision;
}

std::string
ConservativeScalingPolicy::GetName() const
{
    return "ConservativeScaling";
}

} // namespace ns3
