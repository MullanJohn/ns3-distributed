/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "distributed-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DistributedHeader");

NS_OBJECT_ENSURE_REGISTERED(DistributedHeader);

TypeId
DistributedHeader::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DistributedHeader").SetParent<Header>().SetGroupName("Distributed");
    return tid;
}

DistributedHeader::DistributedHeader()
{
    NS_LOG_FUNCTION(this);
}

DistributedHeader::~DistributedHeader()
{
    NS_LOG_FUNCTION(this);
}

bool
DistributedHeader::IsRequest() const
{
    return GetMessageType() == DISTRIBUTED_REQUEST;
}

bool
DistributedHeader::IsResponse() const
{
    return GetMessageType() == DISTRIBUTED_RESPONSE;
}

} // namespace ns3
