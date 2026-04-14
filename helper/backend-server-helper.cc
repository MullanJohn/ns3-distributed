/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "backend-server-helper.h"

#include "ns3/uinteger.h"

namespace ns3
{

BackendServerHelper::BackendServerHelper()
    : ApplicationHelper("ns3::BackendServer")
{
}

BackendServerHelper::BackendServerHelper(uint16_t port)
    : ApplicationHelper("ns3::BackendServer")
{
    m_factory.Set("Port", UintegerValue(port));
}

void
BackendServerHelper::SetPort(uint16_t port)
{
    m_factory.Set("Port", UintegerValue(port));
}

} // namespace ns3
