/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ar-server-helper.h"

#include "ns3/uinteger.h"

namespace ns3
{

ArServerHelper::ArServerHelper()
    : ApplicationHelper("ns3::ArServer")
{
}

ArServerHelper::ArServerHelper(uint16_t port)
    : ApplicationHelper("ns3::ArServer")
{
    m_factory.Set("Port", UintegerValue(port));
}

void
ArServerHelper::SetPort(uint16_t port)
{
    m_factory.Set("Port", UintegerValue(port));
}

} // namespace ns3
