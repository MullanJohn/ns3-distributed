/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "cluster.h"

#include "ns3/assert.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Cluster");

Cluster::Cluster()
{
    NS_LOG_FUNCTION(this);
}

void
Cluster::AddBackend(Ptr<Node> node, const Address& address)
{
    NS_LOG_FUNCTION(this << node << address);
    Backend backend;
    backend.node = node;
    backend.address = address;
    m_backends.push_back(backend);
    NS_LOG_DEBUG("Added backend " << (m_backends.size() - 1) << " to cluster");
}

uint32_t
Cluster::GetN() const
{
    return static_cast<uint32_t>(m_backends.size());
}

const Cluster::Backend&
Cluster::Get(uint32_t i) const
{
    NS_ASSERT_MSG(i < m_backends.size(),
                  "Index " << i << " out of range (size=" << m_backends.size() << ")");
    return m_backends[i];
}

Cluster::Iterator
Cluster::Begin() const
{
    return m_backends.begin();
}

Cluster::Iterator
Cluster::End() const
{
    return m_backends.end();
}

Cluster::Iterator
Cluster::begin() const
{
    return m_backends.begin();
}

Cluster::Iterator
Cluster::end() const
{
    return m_backends.end();
}

bool
Cluster::IsEmpty() const
{
    return m_backends.empty();
}

void
Cluster::Clear()
{
    NS_LOG_FUNCTION(this);
    m_backends.clear();
}

} // namespace ns3
