/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "cluster-scheduler.h"

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
