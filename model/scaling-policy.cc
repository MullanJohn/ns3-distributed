/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "scaling-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ScalingPolicy");

NS_OBJECT_ENSURE_REGISTERED(ScalingPolicy);

TypeId
ScalingPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ScalingPolicy").SetParent<Object>().SetGroupName("Distributed");
    return tid;
}

ScalingPolicy::~ScalingPolicy()
{
    NS_LOG_FUNCTION(this);
}

} // namespace ns3
