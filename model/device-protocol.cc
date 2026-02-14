/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "device-protocol.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DeviceProtocol");

NS_OBJECT_ENSURE_REGISTERED(DeviceProtocol);

TypeId
DeviceProtocol::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DeviceProtocol").SetParent<Object>().SetGroupName("Distributed");
    return tid;
}

DeviceProtocol::~DeviceProtocol()
{
    NS_LOG_FUNCTION(this);
}

} // namespace ns3
