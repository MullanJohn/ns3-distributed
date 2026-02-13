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
Cluster::AddBackend(Ptr<Node> node, const Address& address, const std::string& acceleratorType)
{
    NS_LOG_FUNCTION(this << node << address << acceleratorType);
    uint32_t idx = m_backends.size();
    Backend backend;
    backend.node = node;
    backend.address = address;
    backend.acceleratorType = acceleratorType;
    m_backends.push_back(backend);

    // Update indices
    m_typeIndex[acceleratorType].push_back(idx);
    m_addrIndex[address] = idx;

    NS_LOG_DEBUG("Added backend " << idx << " with accelerator type '" << acceleratorType
                                  << "' to cluster");
}

uint32_t
Cluster::GetN() const
{
    return m_backends.size();
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
    m_backends.clear();
    m_typeIndex.clear();
    m_addrIndex.clear();
}

const std::vector<uint32_t>&
Cluster::GetBackendsByType(const std::string& acceleratorType) const
{
    static const std::vector<uint32_t> empty;
    auto it = m_typeIndex.find(acceleratorType);
    if (it != m_typeIndex.end())
    {
        return it->second;
    }
    return empty;
}

bool
Cluster::HasAcceleratorType(const std::string& acceleratorType) const
{
    auto it = m_typeIndex.find(acceleratorType);
    return it != m_typeIndex.end() && !it->second.empty();
}

int32_t
Cluster::GetBackendIndex(const Address& address) const
{
    auto it = m_addrIndex.find(address);
    if (it != m_addrIndex.end())
    {
        return static_cast<int32_t>(it->second);
    }
    return -1;
}

} // namespace ns3
