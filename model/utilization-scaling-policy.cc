/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "utilization-scaling-policy.h"

#include "ns3/double.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UtilizationScalingPolicy");

NS_OBJECT_ENSURE_REGISTERED(UtilizationScalingPolicy);

TypeId
UtilizationScalingPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::UtilizationScalingPolicy")
            .SetParent<ScalingPolicy>()
            .SetGroupName("Distributed")
            .AddConstructor<UtilizationScalingPolicy>()
            .AddAttribute("MinFrequency",
                          "Lower frequency bound in Hz",
                          DoubleValue(500e6),
                          MakeDoubleAccessor(&UtilizationScalingPolicy::m_minFrequency),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("MaxFrequency",
                          "Upper frequency bound in Hz",
                          DoubleValue(1.5e9),
                          MakeDoubleAccessor(&UtilizationScalingPolicy::m_maxFrequency),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

UtilizationScalingPolicy::UtilizationScalingPolicy()
    : m_minFrequency(500e6),
      m_maxFrequency(1.5e9)
{
    NS_LOG_FUNCTION(this);
}

UtilizationScalingPolicy::~UtilizationScalingPolicy()
{
    NS_LOG_FUNCTION(this);
}

Ptr<ScalingDecision>
UtilizationScalingPolicy::Decide(const ClusterState::BackendState& backend)
{
    NS_LOG_FUNCTION(this);

    bool busy;
    double currentFreq;
    double currentVoltage;

    Ptr<DeviceMetrics> metrics = backend.deviceMetrics;
    if (metrics)
    {
        busy = metrics->busy || metrics->queueLength > 0;
        currentFreq = metrics->frequency;
        currentVoltage = metrics->voltage;
    }
    else
    {
        busy = backend.activeTasks > 0;
        currentFreq = 0.0;
        currentVoltage = 0.0;
    }

    double targetFreq = busy ? m_maxFrequency : m_minFrequency;

    if (targetFreq == currentFreq)
    {
        return nullptr; // No change needed
    }

    Ptr<ScalingDecision> decision = Create<ScalingDecision>();
    decision->targetFrequency = targetFreq;
    decision->targetVoltage = currentVoltage;
    return decision;
}

} // namespace ns3
