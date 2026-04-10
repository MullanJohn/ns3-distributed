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
            .SetGroupName("Continuum")
            .AddConstructor<ConservativeScalingPolicy>()
            .AddAttribute("TargetDrainTime",
                          "Estimated drain time (seconds at nominal rate) that maps to the "
                          "highest performance state.",
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
                                  const ClusterState::BackendState& backend)
{
    NS_LOG_FUNCTION(this);

    if (backend.device.numPerformanceStates < 2)
    {
        return nullptr;
    }

    uint32_t currentIdx = backend.device.currentStateIdx;

    uint32_t targetIdx = 0;
    if (backend.device.nominalComputeRate > 0.0 && m_targetDrainTime > 0.0)
    {
        double drainTime = backend.orchestration.estimatedRemainingComputeWork /
                           backend.device.nominalComputeRate;
        double loadRatio = std::min(drainTime / m_targetDrainTime, 1.0);
        targetIdx = static_cast<uint32_t>(
            std::round(loadRatio * (backend.device.numPerformanceStates - 1)));
    }

    if (currentIdx == targetIdx)
    {
        return nullptr;
    }

    uint32_t nextIdx = (currentIdx < targetIdx) ? currentIdx + 1 : currentIdx - 1;

    Ptr<ScalingDecision> decision = Create<ScalingDecision>();
    decision->targetStateIdx = nextIdx;
    return decision;
}

std::string
ConservativeScalingPolicy::GetName() const
{
    return "ConservativeScaling";
}

} // namespace ns3
