/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "device-manager.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/pointer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DeviceManager");

NS_OBJECT_ENSURE_REGISTERED(DeviceManager);

TypeId
DeviceManager::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DeviceManager")
            .SetParent<Object>()
            .SetGroupName("Distributed")
            .AddConstructor<DeviceManager>()
            .AddAttribute("ScalingPolicy",
                          "Pluggable scaling strategy",
                          PointerValue(),
                          MakePointerAccessor(&DeviceManager::m_scalingPolicy),
                          MakePointerChecker<ScalingPolicy>())
            .AddAttribute("DeviceProtocol",
                          "Protocol for metrics/command serialization",
                          PointerValue(),
                          MakePointerAccessor(&DeviceManager::m_deviceProtocol),
                          MakePointerChecker<DeviceProtocol>())
            .AddAttribute("MinFrequency",
                          "Lower frequency bound in Hz",
                          DoubleValue(500e6),
                          MakeDoubleAccessor(&DeviceManager::m_minFrequency),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("MaxFrequency",
                          "Upper frequency bound in Hz",
                          DoubleValue(1.5e9),
                          MakeDoubleAccessor(&DeviceManager::m_maxFrequency),
                          MakeDoubleChecker<double>(0.0))
            .AddTraceSource("FrequencyChanged",
                            "Trace fired when a backend frequency is changed",
                            MakeTraceSourceAccessor(&DeviceManager::m_frequencyChangedTrace),
                            "ns3::DeviceManager::FrequencyChangedTracedCallback");
    return tid;
}

DeviceManager::DeviceManager()
    : m_scalingPolicy(nullptr),
      m_deviceProtocol(nullptr),
      m_minFrequency(500e6),
      m_maxFrequency(1.5e9)
{
    NS_LOG_FUNCTION(this);
}

DeviceManager::~DeviceManager()
{
    NS_LOG_FUNCTION(this);
}

void
DeviceManager::Start(const Cluster& cluster)
{
    NS_LOG_FUNCTION(this);
    m_cluster = cluster;
    m_backendMetrics.resize(cluster.GetN());
}

void
DeviceManager::HandleMetrics(Ptr<Packet> packet, uint32_t backendIdx)
{
    NS_LOG_FUNCTION(this << packet << backendIdx);

    if (backendIdx >= m_backendMetrics.size())
    {
        NS_LOG_WARN("Backend index " << backendIdx << " out of range");
        return;
    }

    m_backendMetrics[backendIdx] = m_deviceProtocol->ParseMetrics(packet);

    NS_LOG_DEBUG("Stored metrics for backend " << backendIdx
                 << ": freq=" << m_backendMetrics[backendIdx]->frequency
                 << " busy=" << m_backendMetrics[backendIdx]->busy);
}

void
DeviceManager::EvaluateScaling(Ptr<ConnectionManager> workerCm)
{
    NS_LOG_FUNCTION(this);

    if (!m_scalingPolicy || !m_deviceProtocol)
    {
        return;
    }

    for (uint32_t i = 0; i < m_backendMetrics.size(); i++)
    {
        Ptr<DeviceMetrics> metrics = m_backendMetrics[i];
        if (!metrics)
        {
            continue; // No metrics received yet for this backend
        }

        Ptr<ScalingDecision> decision =
            m_scalingPolicy->Decide(metrics, m_minFrequency, m_maxFrequency);

        if (!decision)
        {
            continue; // No change needed
        }

        NS_LOG_INFO("Scaling backend " << i << ": freq " << metrics->frequency
                     << " -> " << decision->targetFrequency);

        double oldFreq = metrics->frequency;
        m_frequencyChangedTrace(i, oldFreq, decision->targetFrequency);

        Ptr<Packet> cmdPacket = m_deviceProtocol->CreateCommandPacket(decision);
        workerCm->Send(cmdPacket, m_cluster.Get(i).address);
    }
}

void
DeviceManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_scalingPolicy = nullptr;
    m_deviceProtocol = nullptr;
    m_backendMetrics.clear();
    m_cluster.Clear();
    Object::DoDispose();
}

} // namespace ns3
