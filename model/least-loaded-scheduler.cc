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
}

LeastLoadedScheduler::~LeastLoadedScheduler()
{
    NS_LOG_FUNCTION(this);
}

int32_t
LeastLoadedScheduler::ScheduleTask(Ptr<Task> task,
                                   const Cluster& cluster,
                                   const ClusterState& state)
{
    NS_LOG_FUNCTION(this << task);

    std::string required = task->GetRequiredAcceleratorType();

    if (required.empty())
    {
        uint32_t n = cluster.GetN();
        if (n == 0)
        {
            NS_LOG_DEBUG("LeastLoaded: no backends in cluster");
            return -1;
        }

        int32_t bestIdx = -1;
        uint32_t minLoad = UINT32_MAX;
        for (uint32_t i = 0; i < n; i++)
        {
            uint32_t load = state.Get(i).activeTasks;
            if (load < minLoad)
            {
                minLoad = load;
                bestIdx = static_cast<int32_t>(i);
            }
        }

        NS_LOG_DEBUG("LeastLoaded: scheduled task " << task->GetTaskId() << " to backend "
                                                    << bestIdx << " (load=" << minLoad << ")");
        return bestIdx;
    }

    const std::vector<uint32_t>& candidates = cluster.GetBackendsByType(required);
    if (candidates.empty())
    {
        NS_LOG_DEBUG("LeastLoaded: no backend matches required accelerator '" << required << "'");
        return -1;
    }

    int32_t bestIdx = -1;
    uint32_t minLoad = UINT32_MAX;
    for (uint32_t candidateIdx : candidates)
    {
        uint32_t load = state.Get(candidateIdx).activeTasks;
        if (load < minLoad)
        {
            minLoad = load;
            bestIdx = static_cast<int32_t>(candidateIdx);
        }
    }

    NS_LOG_DEBUG("LeastLoaded: scheduled task " << task->GetTaskId() << " to backend " << bestIdx
                                                << " (accelerator: " << required
                                                << ", load=" << minLoad << ")");
    return bestIdx;
}

std::string
LeastLoadedScheduler::GetName() const
{
    return "LeastLoaded";
}

} // namespace ns3
