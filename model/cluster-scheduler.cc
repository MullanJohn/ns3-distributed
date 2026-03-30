/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "cluster-scheduler.h"

#include "cluster-state.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ClusterScheduler");
NS_OBJECT_ENSURE_REGISTERED(ClusterScheduler);

TypeId
ClusterScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ClusterScheduler").SetParent<Object>().SetGroupName("Distributed");
    // Note: No AddConstructor because this is an abstract class
    return tid;
}

ClusterScheduler::ClusterScheduler()
{
    NS_LOG_FUNCTION(this);
}

ClusterScheduler::~ClusterScheduler()
{
    NS_LOG_FUNCTION(this);
}

bool
ClusterScheduler::CanScheduleTask(Ptr<Task> task,
                                  const Cluster& cluster,
                                  const ClusterState& state) const
{
    NS_LOG_FUNCTION(this << task);
    std::string required = task->GetRequiredAcceleratorType();
    if (required.empty())
    {
        return cluster.GetN() > 0;
    }
    return !cluster.GetBackendsByType(required).empty();
}

void
ClusterScheduler::NotifyTaskCompleted(uint32_t backendIdx, Ptr<Task> task)
{
    NS_LOG_FUNCTION(this << backendIdx << task);
    // Default: no-op. Stateful schedulers override this.
}

void
ClusterScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

} // namespace ns3
