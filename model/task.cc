/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "task.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Task");
NS_OBJECT_ENSURE_REGISTERED(Task);

TypeId
Task::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Task")
                            .SetParent<Object>()
                            .SetGroupName("Distributed");
    // Note: No AddConstructor because this is an abstract class
    return tid;
}

Task::Task()
{
    NS_LOG_FUNCTION(this);
}

Task::~Task()
{
    NS_LOG_FUNCTION(this);
}

void
Task::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_arrivalTime = Seconds(0);
    Object::DoDispose();
}

Time
Task::GetArrivalTime() const
{
    return m_arrivalTime;
}

void
Task::SetArrivalTime(Time time)
{
    NS_LOG_FUNCTION(this << time);
    m_arrivalTime = time;
}

} // namespace ns3
