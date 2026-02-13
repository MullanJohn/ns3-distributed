/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "first-fit-scheduler.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FirstFitScheduler");
NS_OBJECT_ENSURE_REGISTERED(FirstFitScheduler);

TypeId
FirstFitScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::FirstFitScheduler")
                            .SetParent<ClusterScheduler>()
                            .SetGroupName("Distributed")
                            .AddConstructor<FirstFitScheduler>();
    return tid;
}

FirstFitScheduler::FirstFitScheduler()
{
    NS_LOG_FUNCTION(this);
}

FirstFitScheduler::~FirstFitScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
FirstFitScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_nextIndexByType.clear();
    ClusterScheduler::DoDispose();
}

int32_t
FirstFitScheduler::ScheduleTask(Ptr<Task> task,
                                const Cluster& cluster,
                                const ClusterState& /* state */)
{
    NS_LOG_FUNCTION(this << task);

    std::string required = task->GetRequiredAcceleratorType();

    if (required.empty())
    {
        // No accelerator requirement - round-robin across all backends
        uint32_t n = cluster.GetN();
        if (n == 0)
        {
            NS_LOG_DEBUG("FirstFit: no backends in cluster");
            return -1;
        }
        uint32_t& nextIdx = m_nextIndexByType[""];
        uint32_t idx = nextIdx % n;
        nextIdx = (idx + 1) % n;
        NS_LOG_DEBUG("FirstFit: scheduled task " << task->GetTaskId() << " to backend " << idx);
        return static_cast<int32_t>(idx);
    }

    // Has accelerator requirement - use type index for efficient lookup
    const std::vector<uint32_t>& candidates = cluster.GetBackendsByType(required);
    if (candidates.empty())
    {
        NS_LOG_DEBUG("FirstFit: no backend matches required accelerator '" << required << "'");
        return -1;
    }

    // Round-robin within matching backends using per-type index
    uint32_t& nextIdx = m_nextIndexByType[required];
    uint32_t candidateIdx = nextIdx % candidates.size();
    nextIdx = (candidateIdx + 1) % candidates.size();
    uint32_t backendIdx = candidates[candidateIdx];
    NS_LOG_DEBUG("FirstFit: scheduled task " << task->GetTaskId() << " to backend " << backendIdx
                                             << " (accelerator: " << required << ")");
    return static_cast<int32_t>(backendIdx);
}

std::string
FirstFitScheduler::GetName() const
{
    return "FirstFit";
}

} // namespace ns3
