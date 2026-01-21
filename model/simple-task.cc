/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "simple-task.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SimpleTask");
NS_OBJECT_ENSURE_REGISTERED(SimpleTask);

TypeId
SimpleTask::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SimpleTask")
                            .SetParent<Task>()
                            .SetGroupName("Distributed")
                            .AddConstructor<SimpleTask>();
    return tid;
}

SimpleTask::SimpleTask()
{
    NS_LOG_FUNCTION(this);
}

SimpleTask::~SimpleTask()
{
    NS_LOG_FUNCTION(this);
}

void
SimpleTask::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Task::DoDispose();
}

std::string
SimpleTask::GetName() const
{
    return "SimpleTask";
}

} // namespace ns3
