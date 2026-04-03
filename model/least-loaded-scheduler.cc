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

namespace
{

bool
IsBetterCandidate(double score,
                  uint32_t activeTasks,
                  uint32_t backendIdx,
                  double bestScore,
                  uint32_t bestActiveTasks,
                  int32_t bestBackendIdx)
{
    if (bestBackendIdx < 0)
    {
        return true;
    }
    if (score < bestScore)
    {
        return true;
    }
    if (score > bestScore)
    {
        return false;
    }
    if (activeTasks < bestActiveTasks)
    {
        return true;
    }
    if (activeTasks > bestActiveTasks)
    {
        return false;
    }
    return backendIdx < static_cast<uint32_t>(bestBackendIdx);
}

} // namespace

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

void
LeastLoadedScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
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

    double incomingWork = task->GetComputeDemand();
    double bestScore = 0.0;
    uint32_t bestActiveTasks = 0;
    int32_t bestIdx = -1;

    for (uint32_t idx : pool)
    {
        const ClusterState::BackendState& backend = state.Get(idx);
        double queuedWork = backend.orchestration.estimatedRemainingComputeWork;
        double totalWork = queuedWork + incomingWork;
        double effectiveRate = backend.device.effectiveComputeRate > 0.0
                                   ? backend.device.effectiveComputeRate
                                   : backend.device.nominalComputeRate;
        double score = effectiveRate > 0.0 ? (totalWork / effectiveRate) : totalWork;

        if (IsBetterCandidate(score,
                              backend.orchestration.activeTasks,
                              idx,
                              bestScore,
                              bestActiveTasks,
                              bestIdx))
        {
            bestScore = score;
            bestActiveTasks = backend.orchestration.activeTasks;
            bestIdx = static_cast<int32_t>(idx);
        }

        NS_LOG_DEBUG("LeastLoaded: backend "
                     << idx << " work=" << totalWork << " rate=" << effectiveRate
                     << " score=" << score << " active="
                     << backend.orchestration.activeTasks);
    }

    NS_LOG_DEBUG("LeastLoaded: scheduled task " << task->GetTaskId() << " to backend " << bestIdx
                                                << " (score=" << bestScore << ", active="
                                                << bestActiveTasks << ")");
    return bestIdx;
}

std::string
LeastLoadedScheduler::GetName() const
{
    return "LeastLoaded";
}

} // namespace ns3
