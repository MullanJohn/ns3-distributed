/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "task-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TaskHeader");

NS_OBJECT_ENSURE_REGISTERED(TaskHeader);

TypeId
TaskHeader::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TaskHeader").SetParent<Header>().SetGroupName("Distributed");
    return tid;
}

TaskHeader::TaskHeader()
{
    NS_LOG_FUNCTION(this);
}

TaskHeader::~TaskHeader()
{
    NS_LOG_FUNCTION(this);
}

bool
TaskHeader::IsRequest() const
{
    return GetMessageType() == TASK_REQUEST;
}

bool
TaskHeader::IsResponse() const
{
    return GetMessageType() == TASK_RESPONSE;
}

} // namespace ns3
