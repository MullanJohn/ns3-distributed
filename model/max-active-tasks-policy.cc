/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "max-active-tasks-policy.h"

#include "cluster-state.h"
#include "dag-task.h"
#include "task.h"

#include "ns3/log.h"
#include "ns3/uinteger.h"

#include <map>
#include <string>

namespace ns3
{
namespace
{

std::map<std::string, uint32_t>
CountTasksByType(Ptr<DagTask> dag)
{
    std::map<std::string, uint32_t> counts;
    for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
    {
        Ptr<Task> task = dag->GetTask(i);
        if (task)
        {
            counts[task->GetRequiredAcceleratorType()]++;
        }
    }
    return counts;
}

} // namespace

NS_LOG_COMPONENT_DEFINE("MaxActiveTasksPolicy");
NS_OBJECT_ENSURE_REGISTERED(MaxActiveTasksPolicy);

TypeId
MaxActiveTasksPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::MaxActiveTasksPolicy")
            .SetParent<AdmissionPolicy>()
            .SetGroupName("Continuum")
            .AddConstructor<MaxActiveTasksPolicy>()
            .AddAttribute("MaxActiveTasks",
                          "Maximum active tasks per backend before rejection",
                          UintegerValue(10),
                          MakeUintegerAccessor(&MaxActiveTasksPolicy::m_maxActiveTasks),
                          MakeUintegerChecker<uint32_t>(1));
    return tid;
}

MaxActiveTasksPolicy::MaxActiveTasksPolicy()
    : m_maxActiveTasks(10)
{
    NS_LOG_FUNCTION(this);
}

MaxActiveTasksPolicy::~MaxActiveTasksPolicy()
{
    NS_LOG_FUNCTION(this);
}

bool
MaxActiveTasksPolicy::ShouldAdmit(Ptr<DagTask> dag,
                                  const Cluster& cluster,
                                  const ClusterState& state)
{
    NS_LOG_FUNCTION(this << dag->GetTaskCount() << state.GetActiveWorkloadCount());

    std::map<std::string, uint32_t> requiredCounts = CountTasksByType(dag);

    uint32_t pendingAny = state.GetPendingAdmissionTasksForType("");
    uint32_t pendingTotal = state.GetPendingAdmissionTasks();

    for (const auto& [type, requiredCount] : requiredCounts)
    {
        uint32_t totalAvailable = 0;

        if (type.empty())
        {
            for (uint32_t i = 0; i < state.GetN(); i++)
            {
                if (state.Get(i).orchestration.activeTasks < m_maxActiveTasks)
                {
                    totalAvailable +=
                        m_maxActiveTasks - state.Get(i).orchestration.activeTasks;
                }
            }
        }
        else
        {
            for (uint32_t idx : cluster.GetBackendsByType(type))
            {
                if (state.Get(idx).orchestration.activeTasks < m_maxActiveTasks)
                {
                    totalAvailable +=
                        m_maxActiveTasks - state.Get(idx).orchestration.activeTasks;
                }
            }
        }

        uint32_t pendingReserved =
            type.empty() ? pendingTotal : pendingAny + state.GetPendingAdmissionTasksForType(type);

        if (requiredCount + pendingReserved > totalAvailable)
        {
            NS_LOG_DEBUG("MaxActiveTasks: no capacity for type '"
                         << (type.empty() ? "any" : type) << "'"
                         << " (required=" << requiredCount << ", pending=" << pendingReserved
                         << ", available=" << totalAvailable << ")");
            return false;
        }
    }

    return true;
}

std::map<std::string, uint32_t>
MaxActiveTasksPolicy::GetReservationTaskCounts(Ptr<DagTask> dag) const
{
    NS_LOG_FUNCTION(this << dag);
    return CountTasksByType(dag);
}

std::string
MaxActiveTasksPolicy::GetName() const
{
    return "MaxActiveTasks";
}

} // namespace ns3
