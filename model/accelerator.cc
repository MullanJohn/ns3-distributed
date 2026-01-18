/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "accelerator.h"

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Accelerator");

NS_OBJECT_ENSURE_REGISTERED(Accelerator);

TypeId
Accelerator::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Accelerator")
            .SetParent<Object>()
            .SetGroupName("Distributed")
            .AddTraceSource("TaskStarted",
                            "Trace fired when a task starts execution.",
                            MakeTraceSourceAccessor(&Accelerator::m_taskStartedTrace),
                            "ns3::Accelerator::TaskTracedCallback")
            .AddTraceSource("TaskCompleted",
                            "Trace fired when a task completes execution.",
                            MakeTraceSourceAccessor(&Accelerator::m_taskCompletedTrace),
                            "ns3::Accelerator::TaskCompletedTracedCallback")
            .AddTraceSource("TaskFailed",
                            "Trace fired when a task fails to process.",
                            MakeTraceSourceAccessor(&Accelerator::m_taskFailedTrace),
                            "ns3::Accelerator::TaskFailedTracedCallback");
    return tid;
}

Accelerator::Accelerator()
    : m_node(nullptr)
{
    NS_LOG_FUNCTION(this);
}

Accelerator::~Accelerator()
{
    NS_LOG_FUNCTION(this);
}

uint32_t
Accelerator::GetQueueLength() const
{
    return 0;
}

bool
Accelerator::IsBusy() const
{
    return false;
}

double
Accelerator::GetVoltage() const
{
    return 1.0;
}

double
Accelerator::GetFrequency() const
{
    return 1.0;
}

Ptr<Node>
Accelerator::GetNode() const
{
    return m_node;
}

void
Accelerator::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_node = nullptr;
    Object::DoDispose();
}

void
Accelerator::NotifyNewAggregate()
{
    NS_LOG_FUNCTION(this);
    if (!m_node)
    {
        Ptr<Node> node = GetObject<Node>();
        if (node)
        {
            m_node = node;
            NS_LOG_DEBUG("Accelerator aggregated to node " << m_node->GetId());
        }
    }
    Object::NotifyNewAggregate();
}

} // namespace ns3
