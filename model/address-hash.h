/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ADDRESS_HASH_H
#define ADDRESS_HASH_H

#include "ns3/address.h"

#include <cstddef>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Hash function for ns3::Address suitable for use with std::unordered_map.
 *
 * This hash function uses a byte-level approach to ensure all address bytes
 * are included in the hash, including port numbers for socket addresses.
 * This is important for correctly distinguishing multiple connections from
 * the same IP address but different ports.
 *
 * Example usage:
 * @code
 * std::unordered_map<Address, Ptr<Packet>, AddressHash> bufferMap;
 * @endcode
 */
struct AddressHash
{
    /**
     * @brief Compute hash of an Address.
     * @param addr The address to hash.
     * @return Hash value suitable for use in hash containers.
     *
     * Uses a simple polynomial hash over all bytes of the serialized address.
     * This ensures port numbers are included for InetSocketAddress and
     * Inet6SocketAddress types.
     */
    std::size_t operator()(const Address& addr) const
    {
        uint8_t buffer[Address::MAX_SIZE];
        uint32_t len = addr.CopyTo(buffer);
        std::size_t hash = 0;
        for (uint32_t i = 0; i < len; i++)
        {
            hash = hash * 31 + buffer[i];
        }
        return hash;
    }
};

} // namespace ns3

#endif // ADDRESS_HASH_H
