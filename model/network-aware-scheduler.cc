/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "network-aware-scheduler.h"

#include "cluster-state.h"
#include "cluster.h"

#include "ns3/double.h"
#include "ns3/log.h"

namespace
{

// Mirrors LeastLoadedScheduler's tie-break: lower score wins; ties broken by
// lower activeTasks, then lower backend index.
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

NS_LOG_COMPONENT_DEFINE("NetworkAwareScheduler");
NS_OBJECT_ENSURE_REGISTERED(NetworkAwareScheduler);

TypeId
NetworkAwareScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NetworkAwareScheduler")
            .SetParent<ClusterScheduler>()
            .SetGroupName("Continuum")
            .AddConstructor<NetworkAwareScheduler>()
            .AddAttribute("NetworkWeight",
                          "Weight applied to the network term (RTT + transmission time, in "
                          "seconds) when combined with the compute-completion-time term. "
                          "0 disables network sensitivity entirely (LeastLoadedScheduler "
                          "behavior).",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&NetworkAwareScheduler::m_networkWeight),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

NetworkAwareScheduler::NetworkAwareScheduler()
    : m_networkWeight(1.0)
{
    NS_LOG_FUNCTION(this);
}

NetworkAwareScheduler::~NetworkAwareScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
NetworkAwareScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    ClusterScheduler::DoDispose();
}

int32_t
NetworkAwareScheduler::ScheduleTask(Ptr<Task> task, const Cluster& cluster, const ClusterState& state)
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
        NS_LOG_DEBUG("NetworkAware: no suitable backends");
        return -1;
    }

    double incomingWork = task->GetComputeDemand();
    double payloadBytes = static_cast<double>(task->GetInputSize() + task->GetOutputSize());
    double bestScore = 0.0;
    uint32_t bestActiveTasks = 0;
    int32_t bestIdx = -1;

    for (uint32_t idx : pool)
    {
        const ClusterState::BackendState& backend = state.Get(idx);
        const Cluster::Backend& clusterBackend = cluster.Get(idx);

        double queuedWork = backend.orchestration.estimatedRemainingComputeWork;
        double totalWork = queuedWork + incomingWork;
        double effectiveRate = backend.device.effectiveComputeRate > 0.0
                                   ? backend.device.effectiveComputeRate
                                   : backend.device.nominalComputeRate;
        double computeScore = effectiveRate > 0.0 ? (totalWork / effectiveRate) : totalWork;

        Time oneWayDelay = backend.network.delayValid ? backend.network.measuredDelay
                                                      : clusterBackend.commDelay;
        double rttSeconds = 2.0 * oneWayDelay.GetSeconds();
        double transmitSeconds =
            clusterBackend.commCapacity > 0.0 ? payloadBytes / clusterBackend.commCapacity : 0.0;
        double networkScore = rttSeconds + transmitSeconds;

        double score = computeScore + m_networkWeight * networkScore;

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

        NS_LOG_DEBUG("NetworkAware: backend " << idx << " compute=" << computeScore
                                              << " network=" << networkScore
                                              << " score=" << score
                                              << " active=" << backend.orchestration.activeTasks);
    }

    NS_LOG_DEBUG("NetworkAware: scheduled task " << task->GetTaskId() << " to backend " << bestIdx
                                                 << " (score=" << bestScore
                                                 << ", active=" << bestActiveTasks << ")");
    return bestIdx;
}

std::string
NetworkAwareScheduler::GetName() const
{
    return "NetworkAware";
}

} // namespace ns3
