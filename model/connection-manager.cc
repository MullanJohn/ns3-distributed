/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "connection-manager.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DistributedConnectionManager");

NS_OBJECT_ENSURE_REGISTERED(ConnectionManager);

TypeId
ConnectionManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::distributed::ConnectionManager")
                            .SetParent<Object>()
                            .SetGroupName("Distributed");
    return tid;
}

ConnectionManager::ConnectionManager()
{
    NS_LOG_FUNCTION(this);
}

ConnectionManager::~ConnectionManager()
{
    NS_LOG_FUNCTION(this);
}

void
ConnectionManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

} // namespace ns3
