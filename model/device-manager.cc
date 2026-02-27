/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "device-manager.h"

#include "device-metrics-header.h"

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
            .AddTraceSource("FrequencyChanged",
                            "Trace fired when a backend frequency is changed",
                            MakeTraceSourceAccessor(&DeviceManager::m_frequencyChangedTrace),
                            "ns3::DeviceManager::FrequencyChangedTracedCallback");
    return tid;
}

DeviceManager::DeviceManager()
    : m_scalingPolicy(nullptr),
      m_deviceProtocol(nullptr)
{
    NS_LOG_FUNCTION(this);
}

DeviceManager::~DeviceManager()
{
    NS_LOG_FUNCTION(this);
}

void
DeviceManager::Start(const Cluster& cluster, Ptr<ConnectionManager> workerCm)
{
    NS_LOG_FUNCTION(this);
    m_cluster = cluster;
    m_workerConnMgr = workerCm;
    m_commandedFrequency.resize(cluster.GetN(), 0.0);

    m_operatingPoints.resize(cluster.GetN());
    for (uint32_t i = 0; i < cluster.GetN(); i++)
    {
        Ptr<Accelerator> accel = cluster.Get(i).node->GetObject<Accelerator>();
        if (accel)
        {
            m_operatingPoints[i] = accel->GetOperatingPoints();
        }
    }
}

bool
DeviceManager::TryConsumeMetrics(Ptr<Packet> buffer, const Address& from, ClusterState& state)
{
    NS_LOG_FUNCTION(this << buffer << from);

    uint8_t firstByte;
    buffer->CopyData(&firstByte, 1);

    if (firstByte != DeviceMetricsHeader::DEVICE_METRICS)
    {
        return false;
    }

    if (buffer->GetSize() < DeviceMetricsHeader::SERIALIZED_SIZE)
    {
        return false;
    }

    int32_t idx = m_cluster.GetBackendIndex(from);
    if (idx < 0)
    {
        return false;
    }
    uint32_t backendIdx = static_cast<uint32_t>(idx);

    Ptr<Packet> metricsPacket = buffer->CreateFragment(0, DeviceMetricsHeader::SERIALIZED_SIZE);
    buffer->RemoveAtStart(DeviceMetricsHeader::SERIALIZED_SIZE);
    HandleMetrics(metricsPacket, backendIdx, state);
    return true;
}

void
DeviceManager::HandleMetrics(Ptr<Packet> packet, uint32_t backendIdx, ClusterState& state)
{
    NS_LOG_FUNCTION(this << packet << backendIdx);

    Ptr<DeviceMetrics> metrics = m_deviceProtocol->ParseMetrics(packet);
    state.SetDeviceMetrics(backendIdx, metrics);
    m_commandedFrequency[backendIdx] = metrics->frequency;

    NS_LOG_DEBUG("Stored metrics for backend " << backendIdx << ": freq=" << metrics->frequency
                                               << " busy=" << metrics->busy);
}

void
DeviceManager::EvaluateScaling(const ClusterState& state)
{
    NS_LOG_FUNCTION(this);

    if (!m_scalingPolicy || !m_deviceProtocol)
    {
        return;
    }

    for (uint32_t i = 0; i < m_commandedFrequency.size(); i++)
    {
        const ClusterState::BackendState& backend = state.Get(i);

        Ptr<ScalingDecision> decision = m_scalingPolicy->Decide(backend, m_operatingPoints[i]);

        if (!decision)
        {
            continue;
        }

        double oldFreq = m_commandedFrequency[i];

        NS_LOG_INFO("Scaling backend " << i << ": freq " << oldFreq << " -> "
                                       << decision->targetFrequency);

        m_frequencyChangedTrace(i, oldFreq, decision->targetFrequency);
        m_commandedFrequency[i] = decision->targetFrequency;

        Ptr<Packet> cmdPacket = m_deviceProtocol->CreateCommandPacket(decision);
        m_workerConnMgr->Send(cmdPacket, m_cluster.Get(i).address);
    }
}

void
DeviceManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_scalingPolicy = nullptr;
    m_deviceProtocol = nullptr;
    m_workerConnMgr = nullptr;
    m_commandedFrequency.clear();
    m_operatingPoints.clear();
    m_cluster.Clear();
    Object::DoDispose();
}

} // namespace ns3
