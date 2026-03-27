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
ClusterState::NotifyTaskDispatched(uint32_t backendIdx)
{
    NS_LOG_FUNCTION(this << backendIdx);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    m_backends[backendIdx].activeTasks++;
    m_backends[backendIdx].totalDispatched++;
}

void
ClusterState::NotifyTaskCompleted(uint32_t backendIdx)
{
    NS_LOG_FUNCTION(this << backendIdx);
    NS_ASSERT_MSG(backendIdx < m_backends.size(),
                  "Backend index " << backendIdx << " out of range (size=" << m_backends.size()
                                   << ")");
    NS_ASSERT_MSG(m_backends[backendIdx].activeTasks > 0,
                  "activeTasks underflow for backend " << backendIdx);
    m_backends[backendIdx].activeTasks--;
    m_backends[backendIdx].totalCompleted++;
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
ClusterState::Clear()
{
    NS_LOG_FUNCTION(this);
    m_backends.clear();
    m_activeWorkloads = 0;
}

} // namespace ns3
