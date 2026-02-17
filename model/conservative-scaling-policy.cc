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
            .AddAttribute("MinFrequency",
                          "Lower frequency bound in Hz",
                          DoubleValue(500e6),
                          MakeDoubleAccessor(&ConservativeScalingPolicy::m_minFrequency),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("MaxFrequency",
                          "Upper frequency bound in Hz",
                          DoubleValue(1.5e9),
                          MakeDoubleAccessor(&ConservativeScalingPolicy::m_maxFrequency),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("MinVoltage",
                          "Lower voltage bound in V",
                          DoubleValue(0.8),
                          MakeDoubleAccessor(&ConservativeScalingPolicy::m_minVoltage),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("MaxVoltage",
                          "Upper voltage bound in V",
                          DoubleValue(1.1),
                          MakeDoubleAccessor(&ConservativeScalingPolicy::m_maxVoltage),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("FrequencyStep",
                          "Frequency step size per scaling decision in Hz",
                          DoubleValue(50e6),
                          MakeDoubleAccessor(&ConservativeScalingPolicy::m_frequencyStep),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

ConservativeScalingPolicy::ConservativeScalingPolicy()
    : m_minFrequency(500e6),
      m_maxFrequency(1.5e9),
      m_minVoltage(0.8),
      m_maxVoltage(1.1),
      m_frequencyStep(50e6)
{
    NS_LOG_FUNCTION(this);
}

ConservativeScalingPolicy::~ConservativeScalingPolicy()
{
    NS_LOG_FUNCTION(this);
}

Ptr<ScalingDecision>
ConservativeScalingPolicy::Decide(const ClusterState::BackendState& backend)
{
    NS_LOG_FUNCTION(this);

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
        currentFreq = m_minFrequency;
    }

    double targetFreq;

    if (busy && currentFreq < m_maxFrequency)
    {
        targetFreq = std::min(currentFreq + m_frequencyStep, m_maxFrequency);
    }
    else if (!busy && currentFreq > m_minFrequency)
    {
        targetFreq = std::max(currentFreq - m_frequencyStep, m_minFrequency);
    }
    else
    {
        return nullptr; // No change needed
    }

    double targetVoltage =
        m_minVoltage +
        (targetFreq - m_minFrequency) / (m_maxFrequency - m_minFrequency) * (m_maxVoltage - m_minVoltage);

    Ptr<ScalingDecision> decision = Create<ScalingDecision>();
    decision->targetFrequency = targetFreq;
    decision->targetVoltage = targetVoltage;
    return decision;
}

} // namespace ns3
