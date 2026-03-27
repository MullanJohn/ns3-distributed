/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "least-loaded-scheduler.h"

#include "cluster-state.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("LeastLoadedScheduler");
NS_OBJECT_ENSURE_REGISTERED(LeastLoadedScheduler);

TypeId
LeastLoadedScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::LeastLoadedScheduler")
                            .SetParent<ClusterScheduler>()
                            .SetGroupName("Distributed")
                            .AddConstructor<LeastLoadedScheduler>();
    return tid;
}

LeastLoadedScheduler::LeastLoadedScheduler()
{
    NS_LOG_FUNCTION(this);
    m_tiebreaker = CreateObject<UniformRandomVariable>();
}

LeastLoadedScheduler::~LeastLoadedScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
LeastLoadedScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_tiebreaker = nullptr;
    ClusterScheduler::DoDispose();
}

int32_t
LeastLoadedScheduler::ScheduleTask(Ptr<Task> task,
                                   const Cluster& cluster,
                                   const ClusterState& state)
{
    NS_LOG_FUNCTION(this << task);

    std::string required = task->GetRequiredAcceleratorType();

    std::vector<uint32_t> pool;
    if (required.empty())
    {
        uint32_t n = cluster.GetN();
        pool.reserve(n);
        for (uint32_t i = 0; i < n; i++)
        {
            pool.push_back(i);
        }
    }
    else
    {
        pool = cluster.GetBackendsByType(required);
    }

    if (pool.empty())
    {
        NS_LOG_DEBUG("LeastLoaded: no suitable backends");
        return -1;
    }

    uint32_t minLoad = UINT32_MAX;
    for (uint32_t idx : pool)
    {
        uint32_t load = state.Get(idx).activeTasks;
        if (load < minLoad)
        {
            minLoad = load;
        }
    }

    std::vector<uint32_t> tied;
    for (uint32_t idx : pool)
    {
        if (state.Get(idx).activeTasks == minLoad)
        {
            tied.push_back(idx);
        }
    }

    uint32_t pick = m_tiebreaker->GetInteger(0, tied.size() - 1);
    int32_t bestIdx = static_cast<int32_t>(tied[pick]);

    NS_LOG_DEBUG("LeastLoaded: scheduled task " << task->GetTaskId() << " to backend " << bestIdx
                                                << " (load=" << minLoad << ", tied="
                                                << tied.size() << ")");
    return bestIdx;
}

std::string
LeastLoadedScheduler::GetName() const
{
    return "LeastLoaded";
}

} // namespace ns3
