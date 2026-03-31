/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "cluster-state.h"

#include "scaling-policy.h"

#include "ns3/log.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ClusterState");

void
ClusterState::Resize(uint32_t n)
{
    NS_LOG_FUNCTION(this << n);
    m_backends.resize(n);
}

uint32_t
ClusterState::GetN() const
{
    return static_cast<uint32_t>(m_backends.size());
}

const ClusterState::BackendState&
ClusterState::Get(uint32_t idx) const
{
    NS_ASSERT_MSG(idx < m_backends.size(),
                  "Backend index " << idx << " out of range (size=" << m_backends.size() << ")");
    return m_backends[idx];
}

void
ClusterState::NotifyTaskDispatched(uint32_t backendIdx, double computeDemand)
{
    NS_LOG_FUNCTION(this << backendIdx << computeDemand);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    m_backends[backendIdx].activeTasks++;
    m_backends[backendIdx].totalDispatched++;
    m_backends[backendIdx].estimatedRemainingComputeWork += std::max(0.0, computeDemand);
}

void
ClusterState::UndoTaskDispatched(uint32_t backendIdx, double computeDemand)
{
    NS_LOG_FUNCTION(this << backendIdx << computeDemand);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    NS_ASSERT_MSG(m_backends[backendIdx].activeTasks > 0,
                  "activeTasks underflow for backend " << backendIdx);
    NS_ASSERT_MSG(m_backends[backendIdx].totalDispatched > 0,
                  "totalDispatched underflow for backend " << backendIdx);
    m_backends[backendIdx].activeTasks--;
    m_backends[backendIdx].totalDispatched--;
    m_backends[backendIdx].estimatedRemainingComputeWork =
        std::max(0.0,
                 m_backends[backendIdx].estimatedRemainingComputeWork -
                     std::max(0.0, computeDemand));
}

void
ClusterState::NotifyTaskCompleted(uint32_t backendIdx, double computeDemand)
{
    NS_LOG_FUNCTION(this << backendIdx << computeDemand);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    NS_ASSERT_MSG(m_backends[backendIdx].activeTasks > 0,
                  "activeTasks underflow for backend " << backendIdx);
    m_backends[backendIdx].activeTasks--;
    m_backends[backendIdx].totalCompleted++;
    m_backends[backendIdx].estimatedRemainingComputeWork =
        std::max(0.0,
                 m_backends[backendIdx].estimatedRemainingComputeWork -
                     std::max(0.0, computeDemand));
}

void
ClusterState::SetBackendCapability(uint32_t backendIdx,
                                   double nominalComputeRate,
                                   double baseFrequency)
{
    NS_LOG_FUNCTION(this << backendIdx << nominalComputeRate << baseFrequency);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    m_backends[backendIdx].nominalComputeRate = std::max(0.0, nominalComputeRate);
    m_backends[backendIdx].baseFrequency = std::max(0.0, baseFrequency);
    UpdateEffectiveComputeRate(backendIdx);
}

void
ClusterState::SetDeviceMetrics(uint32_t backendIdx, Ptr<DeviceMetrics> metrics)
{
    NS_LOG_FUNCTION(this << backendIdx);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    m_backends[backendIdx].deviceMetrics = metrics;
}

void
ClusterState::SetCommandedFrequency(uint32_t backendIdx, double frequency)
{
    NS_LOG_FUNCTION(this << backendIdx << frequency);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    m_backends[backendIdx].commandedFrequency = frequency;
    UpdateEffectiveComputeRate(backendIdx);
}

void
ClusterState::UpdateEffectiveComputeRate(uint32_t backendIdx)
{
    BackendState& backend = m_backends[backendIdx];

    if (backend.nominalComputeRate <= 0.0)
    {
        backend.effectiveComputeRate = 0.0;
        return;
    }

    if (backend.baseFrequency <= 0.0)
    {
        backend.effectiveComputeRate = backend.nominalComputeRate;
        return;
    }

    double frequency = backend.commandedFrequency > 0.0 ? backend.commandedFrequency
                                                        : backend.baseFrequency;
    backend.effectiveComputeRate = backend.nominalComputeRate * (frequency / backend.baseFrequency);
}

void
ClusterState::SetActiveWorkloadCount(uint32_t count)
{
    NS_LOG_FUNCTION(this << count);
    m_activeWorkloads = count;
}

uint32_t
ClusterState::GetActiveWorkloadCount() const
{
    return m_activeWorkloads;
}

void
ClusterState::ReserveTasks(uint32_t count, const std::string& acceleratorType)
{
    NS_LOG_FUNCTION(this << count << acceleratorType);
    m_pendingAdmissionTasks += count;
    m_pendingAdmissionTasksByType[acceleratorType] += count;
}

void
ClusterState::ReleaseTasks(uint32_t count, const std::string& acceleratorType)
{
    NS_LOG_FUNCTION(this << count << acceleratorType);
    NS_ASSERT_MSG(m_pendingAdmissionTasks >= count,
                  "pendingAdmissionTasks underflow: have " << m_pendingAdmissionTasks
                                                           << " releasing " << count);

    auto it = m_pendingAdmissionTasksByType.find(acceleratorType);
    NS_ASSERT_MSG(it != m_pendingAdmissionTasksByType.end() && it->second >= count,
                  "pendingAdmissionTasksByType underflow for type '"
                      << (acceleratorType.empty() ? "any" : acceleratorType) << "'");

    m_pendingAdmissionTasks -= count;
    it->second -= count;
    if (it->second == 0)
    {
        m_pendingAdmissionTasksByType.erase(it);
    }
}

uint32_t
ClusterState::GetPendingAdmissionTasks() const
{
    return m_pendingAdmissionTasks;
}

uint32_t
ClusterState::GetPendingAdmissionTasksForType(const std::string& acceleratorType) const
{
    auto it = m_pendingAdmissionTasksByType.find(acceleratorType);
    return it != m_pendingAdmissionTasksByType.end() ? it->second : 0;
}

void
ClusterState::Clear()
{
    NS_LOG_FUNCTION(this);
    m_backends.clear();
    m_activeWorkloads = 0;
    m_pendingAdmissionTasks = 0;
    m_pendingAdmissionTasksByType.clear();
}

} // namespace ns3
