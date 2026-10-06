/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "tiered-delay-scheduler.h"

#include "cluster-state.h"
#include "cluster.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TieredDelayScheduler");
NS_OBJECT_ENSURE_REGISTERED(TieredDelayScheduler);

TypeId
TieredDelayScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TieredDelayScheduler")
            .SetParent<ClusterScheduler>()
            .SetGroupName("Continuum")
            .AddConstructor<TieredDelayScheduler>()
            .AddAttribute("OverflowQueueDelay",
                          "Queue drain time (queued compute / effective rate) at/above "
                          "which a tier is considered saturated and the task overflows "
                          "to the next-higher-commDelay tier (not applied to the last "
                          "tier, which is always used as a catch-all).",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&TieredDelayScheduler::m_overflowQueueDelay),
                          MakeTimeChecker())
            .AddAttribute("RecoverQueueDelay",
                          "Queue drain time at/below which a backend that was "
                          "overflowing is considered recovered and becomes preferred "
                          "again. Should be <= OverflowQueueDelay; the gap between the "
                          "two is a hysteresis band that prevents rapid tier flapping "
                          "when load hovers near the threshold.",
                          TimeValue(MilliSeconds(50)),
                          MakeTimeAccessor(&TieredDelayScheduler::m_recoverQueueDelay),
                          MakeTimeChecker());
    return tid;
}

TieredDelayScheduler::TieredDelayScheduler()
    : m_overflowQueueDelay(MilliSeconds(100)),
      m_recoverQueueDelay(MilliSeconds(50))
{
    NS_LOG_FUNCTION(this);
    m_rand = CreateObject<UniformRandomVariable>();
}

TieredDelayScheduler::~TieredDelayScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
TieredDelayScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_overflowing.clear();
    m_rand = nullptr;
    ClusterScheduler::DoDispose();
}

Time
TieredDelayScheduler::GetDrainTime(uint32_t idx, const ClusterState& state) const
{
    const ClusterState::BackendState& backend = state.Get(idx);
    double capacity = backend.device.effectiveComputeRate > 0.0
                          ? backend.device.effectiveComputeRate
                          : backend.device.nominalComputeRate;
    double queuedWork = backend.orchestration.estimatedRemainingComputeWork;
    if (capacity > 0.0)
    {
        return Seconds(queuedWork / capacity);
    }
    return queuedWork > 0.0 ? Time::Max() : Seconds(0);
}

int32_t
TieredDelayScheduler::ScheduleTask(Ptr<Task> task, const Cluster& cluster, const ClusterState& state)
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
        NS_LOG_DEBUG("TieredDelay: no suitable backends");
        return -1;
    }

    // Group the candidate pool into tiers by static commDelay. std::map's
    // ordering by Time::operator< gives ascending tiers (lowest delay first)
    // as a side effect of insertion.
    std::map<Time, std::vector<uint32_t>> tiers;
    for (uint32_t idx : pool)
    {
        tiers[cluster.Get(idx).commDelay].push_back(idx);
    }

    uint32_t tierPosition = 0;
    for (const auto& tierEntry : tiers)
    {
        tierPosition++;
        bool isLastTier = (tierPosition == tiers.size());
        const std::vector<uint32_t>& tierIndices = tierEntry.second;

        Time minDrain = Seconds(0);
        std::vector<uint32_t> tied;
        for (uint32_t idx : tierIndices)
        {
            Time drain = GetDrainTime(idx, state);

            if (tied.empty() || drain < minDrain)
            {
                minDrain = drain;
                tied.clear();
                tied.push_back(idx);
            }
            else if (drain == minDrain)
            {
                tied.push_back(idx);
            }
        }

        uint32_t chosen =
            tied.size() == 1 ? tied[0] : tied[m_rand->GetInteger(0, tied.size() - 1)];

        bool wasOverflowing = m_overflowing[chosen]; // defaults false on first access
        bool nowOverflowing = wasOverflowing ? (minDrain > m_recoverQueueDelay)
                                             : (minDrain >= m_overflowQueueDelay);
        m_overflowing[chosen] = nowOverflowing;

        NS_LOG_DEBUG("TieredDelay: tier " << tierPosition << " best backend " << chosen
                                          << " drain=" << minDrain.As(Time::MS)
                                          << " overflowing=" << nowOverflowing
                                          << " (tied candidates=" << tied.size() << ")");

        if (!nowOverflowing || isLastTier)
        {
            NS_LOG_DEBUG("TieredDelay: scheduled task " << task->GetTaskId() << " to backend "
                                                        << chosen);
            return static_cast<int32_t>(chosen);
        }
        // Tier saturated (latched via hysteresis) and not the last tier: overflow
        // to the next tier.
    }

    // Unreachable: pool non-empty implies tiers non-empty, and the loop always
    // returns on its last iteration via the isLastTier catch-all.
    return -1;
}

std::string
TieredDelayScheduler::GetName() const
{
    return "TieredDelay";
}

} // namespace ns3
