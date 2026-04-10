/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "round-robin-scheduler.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoundRobinScheduler");
NS_OBJECT_ENSURE_REGISTERED(RoundRobinScheduler);

TypeId
RoundRobinScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoundRobinScheduler")
                            .SetParent<ClusterScheduler>()
                            .SetGroupName("Continuum")
                            .AddConstructor<RoundRobinScheduler>();
    return tid;
}

RoundRobinScheduler::RoundRobinScheduler()
{
    NS_LOG_FUNCTION(this);
}

RoundRobinScheduler::~RoundRobinScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
RoundRobinScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_nextIndexByType.clear();
    ClusterScheduler::DoDispose();
}

int32_t
RoundRobinScheduler::ScheduleTask(Ptr<Task> task,
                                  const Cluster& cluster,
                                  const ClusterState& /* state */)
{
    NS_LOG_FUNCTION(this << task);

    std::string required = task->GetRequiredAcceleratorType();

    if (required.empty())
    {
        uint32_t n = cluster.GetN();
        if (n == 0)
        {
            NS_LOG_DEBUG("RoundRobin: no backends in cluster");
            return -1;
        }
        uint32_t& nextIdx = m_nextIndexByType[""];
        uint32_t idx = nextIdx % n;
        nextIdx = (idx + 1) % n;
        NS_LOG_DEBUG("RoundRobin: scheduled task " << task->GetTaskId() << " to backend " << idx);
        return static_cast<int32_t>(idx);
    }

    const std::vector<uint32_t>& candidates = cluster.GetBackendsByType(required);
    if (candidates.empty())
    {
        NS_LOG_DEBUG("RoundRobin: no backend matches required accelerator '" << required << "'");
        return -1;
    }

    uint32_t& nextIdx = m_nextIndexByType[required];
    uint32_t candidateIdx = nextIdx % candidates.size();
    nextIdx = (candidateIdx + 1) % candidates.size();
    uint32_t backendIdx = candidates[candidateIdx];
    NS_LOG_DEBUG("RoundRobin: scheduled task " << task->GetTaskId() << " to backend " << backendIdx
                                               << " (accelerator: " << required << ")");
    return static_cast<int32_t>(backendIdx);
}

std::string
RoundRobinScheduler::GetName() const
{
    return "RoundRobin";
}

} // namespace ns3
