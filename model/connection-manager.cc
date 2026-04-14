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

NS_LOG_COMPONENT_DEFINE("ContinuumConnectionManager");

NS_OBJECT_ENSURE_REGISTERED(ConnectionManager);

TypeId
ConnectionManager::GetTypeId()
{
    // Uses ns3::continuum prefix to avoid collision with src/wimax/model/connection-manager.cc
    static TypeId tid =
        TypeId("ns3::continuum::ConnectionManager")
            .SetParent<Object>()
            .SetGroupName("Continuum")
            .AddTraceSource("Tx",
                            "A packet has been sent successfully",
                            MakeTraceSourceAccessor(&ConnectionManager::m_txTrace),
                            "ns3::ConnectionManager::PacketAddressTracedCallback")
            .AddTraceSource("Rx",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&ConnectionManager::m_rxTrace),
                            "ns3::ConnectionManager::PacketAddressTracedCallback")
            .AddTraceSource("TxDrop",
                            "A packet send has failed",
                            MakeTraceSourceAccessor(&ConnectionManager::m_txDropTrace),
                            "ns3::ConnectionManager::PacketAddressTracedCallback");
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
