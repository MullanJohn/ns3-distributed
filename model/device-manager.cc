/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "device-manager.h"

#include "accelerator.h"
#include "device-metrics-header.h"
#include "scaling-command-header.h"

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
                          "Protocol for metrics serialization",
                          PointerValue(),
                          MakePointerAccessor(&DeviceManager::m_deviceProtocol),
                          MakePointerChecker<DeviceProtocol>())
            .AddTraceSource("StateChanged",
                            "Trace fired when a backend performance state is changed",
                            MakeTraceSourceAccessor(&DeviceManager::m_stateChangedTrace),
                            "ns3::DeviceManager::StateChangedTracedCallback");
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
DeviceManager::Start(const Cluster& cluster, Ptr<ConnectionManager> backendCm, ClusterState& state)
{
    NS_LOG_FUNCTION(this);
    m_cluster = cluster;
    m_backendConnMgr = backendCm;

    m_stateComputeRates.resize(cluster.GetN());
    for (uint32_t i = 0; i < cluster.GetN(); i++)
    {
        Ptr<Accelerator> accel = cluster.Get(i).node->GetObject<Accelerator>();
        if (!accel)
        {
            continue;
        }

        double baseRate = accel->GetComputeRate();
        state.SetBackendCapability(i, baseRate);

        uint32_t n = accel->GetNumPerformanceStates();
        if (n > 0)
        {
            m_stateComputeRates[i].resize(n);
            for (uint32_t j = 0; j < n; j++)
            {
                m_stateComputeRates[i][j] = accel->GetComputeRateAtState(j);
            }
            state.SetPerformanceStates(i, n, m_stateComputeRates[i].back());
        }

        state.SetCurrentState(i, 0, baseRate);
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

    NS_LOG_DEBUG("Stored metrics for backend " << backendIdx << ": util=" << metrics->utilization
                                               << " power=" << metrics->currentPower);
}

void
DeviceManager::EvaluateScaling(ClusterState& state)
{
    NS_LOG_FUNCTION(this);

    if (!m_scalingPolicy)
    {
        return;
    }

    for (uint32_t i = 0; i < state.GetN(); i++)
    {
        const ClusterState::BackendState& backend = state.Get(i);

        Ptr<ScalingDecision> decision = m_scalingPolicy->Decide(i, backend);

        if (!decision)
        {
            continue;
        }

        if (decision->targetStateIdx == backend.device.currentStateIdx)
        {
            continue;
        }

        ScalingCommandHeader header;
        header.SetMessageType(ScalingCommandHeader::SCALING_COMMAND);
        header.SetTargetStateIdx(decision->targetStateIdx);
        Ptr<Packet> cmdPacket = Create<Packet>();
        cmdPacket->AddHeader(header);

        if (!m_backendConnMgr->Send(cmdPacket, m_cluster.Get(i).address))
        {
            NS_LOG_WARN("Failed to send scaling command to backend " << i);
            continue;
        }

        uint32_t oldStateIdx = backend.device.currentStateIdx;

        NS_LOG_INFO("Scaling backend " << i << ": state " << oldStateIdx << " -> "
                                       << decision->targetStateIdx);

        m_stateChangedTrace(i, oldStateIdx, decision->targetStateIdx);

        double newRate = backend.device.nominalComputeRate;
        if (decision->targetStateIdx < m_stateComputeRates[i].size())
        {
            newRate = m_stateComputeRates[i][decision->targetStateIdx];
        }
        state.SetCurrentState(i, decision->targetStateIdx, newRate);
    }
}

void
DeviceManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_scalingPolicy = nullptr;
    m_deviceProtocol = nullptr;
    m_backendConnMgr = nullptr;
    m_stateComputeRates.clear();
    m_cluster.Clear();
    Object::DoDispose();
}

} // namespace ns3
