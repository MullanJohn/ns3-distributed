/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "deadline-aware-admission-policy.h"

#include "cluster-state.h"
#include "dag-task.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DeadlineAwareAdmissionPolicy");
NS_OBJECT_ENSURE_REGISTERED(DeadlineAwareAdmissionPolicy);

TypeId
DeadlineAwareAdmissionPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DeadlineAwareAdmissionPolicy")
            .SetParent<AdmissionPolicy>()
            .SetGroupName("Distributed")
            .AddConstructor<DeadlineAwareAdmissionPolicy>()
            .AddAttribute("ComputeRate",
                          "Assumed backend processing rate in FLOPS",
                          DoubleValue(1e12),
                          MakeDoubleAccessor(&DeadlineAwareAdmissionPolicy::m_computeRate),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

DeadlineAwareAdmissionPolicy::DeadlineAwareAdmissionPolicy()
    : m_computeRate(1e12)
{
    NS_LOG_FUNCTION(this);
}

DeadlineAwareAdmissionPolicy::~DeadlineAwareAdmissionPolicy()
{
    NS_LOG_FUNCTION(this);
}

bool
DeadlineAwareAdmissionPolicy::ShouldAdmit(Ptr<DagTask> dag,
                                          const Cluster& cluster,
                                          const ClusterState& state)
{
    NS_LOG_FUNCTION(this << dag->GetTaskCount() << state.GetActiveWorkloadCount());

    uint32_t n = dag->GetTaskCount();

    std::vector<Time> earliestStart(n, Simulator::Now());
    std::vector<uint32_t> topoOrder = dag->GetTopologicalOrder();

    for (uint32_t curr : topoOrder)
    {
        double execTime = dag->GetTask(curr)->GetComputeDemand() / m_computeRate;
        Time currCompletion = earliestStart[curr] + Seconds(execTime);

        for (uint32_t s : dag->GetSuccessors(curr))
        {
            if (currCompletion > earliestStart[s])
            {
                earliestStart[s] = currCompletion;
            }
        }
    }

    for (uint32_t i = 0; i < n; ++i)
    {
        Ptr<Task> task = dag->GetTask(i);

        if (!task->HasDeadline())
        {
            continue;
        }

        std::string reqType = task->GetRequiredAcceleratorType();
        bool feasible = false;

        if (reqType.empty())
        {
            for (uint32_t b = 0; b < state.GetN(); ++b)
            {
                if (CanMeetDeadline(task, state.Get(b), earliestStart[i]))
                {
                    feasible = true;
                    break;
                }
            }
        }
        else
        {
            const auto& indices = cluster.GetBackendsByType(reqType);
            for (uint32_t idx : indices)
            {
                if (CanMeetDeadline(task, state.Get(idx), earliestStart[i]))
                {
                    feasible = true;
                    break;
                }
            }
        }

        if (!feasible)
        {
            NS_LOG_DEBUG("DeadlineAware: rejecting workload, task " << task->GetTaskId()
                                                                    << " cannot meet deadline");
            return false;
        }
    }

    NS_LOG_DEBUG("DeadlineAware: admitting workload with " << dag->GetTaskCount() << " tasks");
    return true;
}

std::string
DeadlineAwareAdmissionPolicy::GetName() const
{
    return "DeadlineAware";
}

bool
DeadlineAwareAdmissionPolicy::CanMeetDeadline(Ptr<Task> task,
                                              const ClusterState::BackendState& backend,
                                              Time earliestStart) const
{
    double wait = backend.activeTasks * (task->GetComputeDemand() / m_computeRate);
    double exec = task->GetComputeDemand() / m_computeRate;
    Time estimatedCompletion = earliestStart + Seconds(wait + exec);
    return estimatedCompletion <= task->GetDeadline();
}

} // namespace ns3
