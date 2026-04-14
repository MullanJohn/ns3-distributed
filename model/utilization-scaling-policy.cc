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
                            .SetGroupName("Continuum")
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
UtilizationScalingPolicy::Decide(uint32_t /* backendIdx */,
                                 const ClusterState::BackendState& backend)
{
    NS_LOG_FUNCTION(this);

    if (backend.device.numPerformanceStates < 2)
    {
        return nullptr;
    }

    bool busy = backend.orchestration.activeTasks > 0;
    uint32_t targetIdx = busy ? (backend.device.numPerformanceStates - 1) : 0;

    if (targetIdx == backend.device.currentStateIdx)
    {
        return nullptr;
    }

    Ptr<ScalingDecision> decision = Create<ScalingDecision>();
    decision->targetStateIdx = targetIdx;
    return decision;
}

std::string
UtilizationScalingPolicy::GetName() const
{
    return "UtilizationScaling";
}

} // namespace ns3
