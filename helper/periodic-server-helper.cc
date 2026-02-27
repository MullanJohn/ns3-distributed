/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "periodic-server-helper.h"

#include "ns3/uinteger.h"

namespace ns3
{

PeriodicServerHelper::PeriodicServerHelper()
    : ApplicationHelper("ns3::PeriodicServer")
{
}

PeriodicServerHelper::PeriodicServerHelper(uint16_t port)
    : ApplicationHelper("ns3::PeriodicServer")
{
    m_factory.Set("Port", UintegerValue(port));
}

void
PeriodicServerHelper::SetPort(uint16_t port)
{
    m_factory.Set("Port", UintegerValue(port));
}

} // namespace ns3
