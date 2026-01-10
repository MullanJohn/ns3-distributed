/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "queue-scheduler.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QueueScheduler");

NS_OBJECT_ENSURE_REGISTERED(QueueScheduler);

TypeId
QueueScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::QueueScheduler")
                            .SetParent<Object>()
                            .SetGroupName("Distributed");
    return tid;
}

QueueScheduler::QueueScheduler()
{
    NS_LOG_FUNCTION(this);
}

QueueScheduler::~QueueScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
QueueScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

} // namespace ns3
