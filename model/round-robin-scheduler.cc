/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "round-robin-scheduler.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoundRobinScheduler");

NS_OBJECT_ENSURE_REGISTERED(RoundRobinScheduler);

TypeId
RoundRobinScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoundRobinScheduler")
                            .SetParent<NodeScheduler>()
                            .SetGroupName("Distributed")
                            .AddConstructor<RoundRobinScheduler>();
    return tid;
}

RoundRobinScheduler::RoundRobinScheduler()
    : m_nextIndex(0)
{
    NS_LOG_FUNCTION(this);
}

RoundRobinScheduler::RoundRobinScheduler(const RoundRobinScheduler& other)
    : NodeScheduler(other),
      m_nextIndex(other.m_nextIndex)
{
    NS_LOG_FUNCTION(this);
}

RoundRobinScheduler::~RoundRobinScheduler()
{
    NS_LOG_FUNCTION(this);
}

std::string
RoundRobinScheduler::GetName() const
{
    return "RoundRobin";
}

void
RoundRobinScheduler::Initialize(const Cluster& cluster)
{
    NS_LOG_FUNCTION(this << cluster.GetN());
    NodeScheduler::Initialize(cluster);
    m_nextIndex = 0;
}

int32_t
RoundRobinScheduler::SelectBackend(const OffloadHeader& header, const Cluster& cluster)
{
    NS_LOG_FUNCTION(this << header.GetTaskId());

    if (cluster.GetN() == 0)
    {
        NS_LOG_WARN("No backends available in cluster");
        return -1;
    }

    uint32_t selected = m_nextIndex;
    m_nextIndex = (m_nextIndex + 1) % cluster.GetN();

    NS_LOG_DEBUG("Selected backend " << selected << " for task " << header.GetTaskId()
                                     << " (next will be " << m_nextIndex << ")");
    return static_cast<int32_t>(selected);
}

Ptr<NodeScheduler>
RoundRobinScheduler::Fork()
{
    NS_LOG_FUNCTION(this);
    return CopyObject<RoundRobinScheduler>(this);
}

} // namespace ns3
