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

#include "ns3/log.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("MaxActiveTasksPolicy");
NS_OBJECT_ENSURE_REGISTERED(MaxActiveTasksPolicy);

TypeId
MaxActiveTasksPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::MaxActiveTasksPolicy")
            .SetParent<AdmissionPolicy>()
            .SetGroupName("Distributed")
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

    for (uint32_t i = 0; i < state.GetN(); i++)
    {
        if (state.Get(i).activeTasks < m_maxActiveTasks)
        {
            NS_LOG_DEBUG("MaxActiveTasks: backend " << i << " has capacity ("
                                                    << state.Get(i).activeTasks << "/"
                                                    << m_maxActiveTasks << ")");
            return true;
        }
    }

    NS_LOG_DEBUG("MaxActiveTasks: all "
                 << state.GetN() << " backends at capacity (threshold=" << m_maxActiveTasks << ")");
    return false;
}

std::string
MaxActiveTasksPolicy::GetName() const
{
    return "MaxActiveTasks";
}

} // namespace ns3
