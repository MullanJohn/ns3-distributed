/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "node-scheduler.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NodeScheduler");

NS_OBJECT_ENSURE_REGISTERED(NodeScheduler);

TypeId
NodeScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NodeScheduler").SetParent<Object>().SetGroupName("Distributed");
    // Note: No AddConstructor because this is an abstract class
    return tid;
}

NodeScheduler::NodeScheduler()
    : m_numBackends(0)
{
    NS_LOG_FUNCTION(this);
}

NodeScheduler::NodeScheduler(const NodeScheduler& other)
    : Object(other),
      m_numBackends(other.m_numBackends)
{
    NS_LOG_FUNCTION(this);
}

NodeScheduler::~NodeScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
NodeScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_numBackends = 0;
    Object::DoDispose();
}

void
NodeScheduler::Initialize(const Cluster& cluster)
{
    NS_LOG_FUNCTION(this << cluster.GetN());
    m_numBackends = cluster.GetN();
    NS_LOG_INFO("Scheduler " << GetName() << " initialized with " << m_numBackends << " backends");
}

void
NodeScheduler::NotifyTaskSent(uint32_t backendIndex, const TaskHeader& header)
{
    NS_LOG_FUNCTION(this << backendIndex << header.GetTaskId());
    // Default implementation does nothing - stateful schedulers override this
}

void
NodeScheduler::NotifyTaskCompleted(uint32_t backendIndex, uint64_t taskId, Time duration)
{
    NS_LOG_FUNCTION(this << backendIndex << taskId << duration);
    // Default implementation does nothing - stateful schedulers override this
}

} // namespace ns3
