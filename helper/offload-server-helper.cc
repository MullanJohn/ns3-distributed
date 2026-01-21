/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-server-helper.h"

#include "ns3/address.h"
#include "ns3/uinteger.h"

namespace ns3
{

OffloadServerHelper::OffloadServerHelper()
    : ApplicationHelper("ns3::OffloadServer")
{
}

OffloadServerHelper::OffloadServerHelper(uint16_t port)
    : ApplicationHelper("ns3::OffloadServer")
{
    m_factory.Set("Port", UintegerValue(port));
}

OffloadServerHelper::OffloadServerHelper(const Address& localAddress)
    : ApplicationHelper("ns3::OffloadServer")
{
    m_factory.Set("Local", AddressValue(localAddress));
}

void
OffloadServerHelper::SetPort(uint16_t port)
{
    m_factory.Set("Port", UintegerValue(port));
}

void
OffloadServerHelper::SetLocalAddress(const Address& localAddress)
{
    m_factory.Set("Local", AddressValue(localAddress));
}

} // namespace ns3
