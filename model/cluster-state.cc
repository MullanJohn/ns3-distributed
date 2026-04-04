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
    m_backends[backendIdx].orchestration.activeTasks++;
    m_backends[backendIdx].orchestration.estimatedRemainingComputeWork += computeDemand;
}

void
ClusterState::UndoTaskDispatched(uint32_t backendIdx, double computeDemand)
{
    NS_LOG_FUNCTION(this << backendIdx << computeDemand);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    NS_ASSERT_MSG(m_backends[backendIdx].orchestration.activeTasks > 0,
                  "activeTasks underflow for backend " << backendIdx);
    m_backends[backendIdx].orchestration.activeTasks--;
    m_backends[backendIdx].orchestration.estimatedRemainingComputeWork =
        std::max(0.0,
                 m_backends[backendIdx].orchestration.estimatedRemainingComputeWork - computeDemand);
}

void
ClusterState::NotifyTaskCompleted(uint32_t backendIdx, double computeDemand)
{
    NS_LOG_FUNCTION(this << backendIdx << computeDemand);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    NS_ASSERT_MSG(m_backends[backendIdx].orchestration.activeTasks > 0,
                  "activeTasks underflow for backend " << backendIdx);
    m_backends[backendIdx].orchestration.activeTasks--;
    m_backends[backendIdx].orchestration.estimatedRemainingComputeWork =
        std::max(0.0,
                 m_backends[backendIdx].orchestration.estimatedRemainingComputeWork - computeDemand);
}

void
ClusterState::SetBackendCapability(uint32_t backendIdx, double nominalComputeRate)
{
    NS_LOG_FUNCTION(this << backendIdx << nominalComputeRate);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    m_backends[backendIdx].device.nominalComputeRate = nominalComputeRate;
    m_backends[backendIdx].device.effectiveComputeRate = nominalComputeRate;
}

void
ClusterState::SetDeviceMetrics(uint32_t backendIdx, Ptr<DeviceMetrics> metrics)
{
    NS_LOG_FUNCTION(this << backendIdx);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    m_backends[backendIdx].device.deviceMetrics = metrics;
}

void
ClusterState::SetCurrentState(uint32_t backendIdx,
                              uint32_t stateIdx,
                              double effectiveComputeRate)
{
    NS_LOG_FUNCTION(this << backendIdx << stateIdx << effectiveComputeRate);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    m_backends[backendIdx].device.currentStateIdx = stateIdx;
    m_backends[backendIdx].device.effectiveComputeRate = effectiveComputeRate;
}

void
ClusterState::SetPerformanceStates(uint32_t backendIdx,
                                   uint32_t numStates,
                                   double maxComputeRate)
{
    NS_LOG_FUNCTION(this << backendIdx << numStates << maxComputeRate);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    m_backends[backendIdx].device.numPerformanceStates = numStates;
    m_backends[backendIdx].device.maxComputeRate = maxComputeRate;
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
