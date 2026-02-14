/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef CLUSTER_H
#define CLUSTER_H

#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/ptr.h"

#include <map>
#include <string>
#include <vector>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Represents a cluster of backend server nodes for distributed computing.
 *
 * A Cluster holds references to server nodes and their addresses. It provides
 * iteration and access patterns similar to NodeContainer. The Cluster is used
 * by ClusterScheduler implementations to select which backend should handle
 * incoming tasks.
 *
 * Each backend in the cluster is represented by a Backend struct containing:
 * - A pointer to the server Node (which may have a GpuAccelerator aggregated)
 * - The network Address (InetSocketAddress with IP and port) for TCP connections
 *
 * Example usage:
 * @code
 * Cluster cluster;
 * cluster.AddBackend(serverNode1, InetSocketAddress(addr1, 9000));
 * cluster.AddBackend(serverNode2, InetSocketAddress(addr2, 9000));
 * cluster.AddBackend(serverNode3, InetSocketAddress(addr3, 9000));
 *
 * for (auto it = cluster.Begin(); it != cluster.End(); ++it)
 * {
 *     Ptr<Node> node = it->node;
 *     Address addr = it->address;
 * }
 * @endcode
 */
class Cluster
{
  public:
    /**
     * @brief Information about a backend server in the cluster.
     */
    struct Backend
    {
        Ptr<Node> node;  //!< The backend server node (may have GpuAccelerator aggregated)
        Address address; //!< Server address (InetSocketAddress with IP and port)
        std::string acceleratorType; //!< Type of accelerator (e.g., "GPU", "TPU"). Empty = any.
    };

    /// Iterator type for traversing backends
    typedef std::vector<Backend>::const_iterator Iterator;

    /**
     * @brief Create an empty cluster.
     */
    Cluster();

    /**
     * @brief Add a backend server to the cluster.
     *
     * @param node The server node. This node should have an OffloadServer
     *             application installed and optionally a GpuAccelerator aggregated.
     * @param address The address clients should connect to (typically
     *                InetSocketAddress with the server's IP and port).
     * @param acceleratorType The type of accelerator on this backend (e.g., "GPU", "TPU").
     *                        Empty string means any/unspecified.
     */
    void AddBackend(Ptr<Node> node,
                    const Address& address,
                    const std::string& acceleratorType = "");

    /**
     * @brief Get the number of backends in the cluster.
     *
     * @return The number of backend servers.
     */
    uint32_t GetN() const;

    /**
     * @brief Get backend at the specified index.
     *
     * @param i The index of the backend (0 to GetN()-1).
     * @return Reference to the Backend struct at the given index.
     */
    const Backend& Get(uint32_t i) const;

    /**
     * @brief Get an iterator to the first backend.
     *
     * @return Iterator pointing to the first backend, or End() if empty.
     */
    Iterator Begin() const;

    /**
     * @brief Get an iterator past the last backend.
     *
     * @return Iterator pointing past the last backend.
     */
    Iterator End() const;

    /**
     * @brief Get an iterator to the first backend (lowercase for range-based for).
     *
     * @return Iterator pointing to the first backend, or end() if empty.
     */
    Iterator begin() const;

    /**
     * @brief Get an iterator past the last backend (lowercase for range-based for).
     *
     * @return Iterator pointing past the last backend.
     */
    Iterator end() const;

    /**
     * @brief Check if the cluster is empty.
     *
     * @return true if the cluster has no backends, false otherwise.
     */
    bool IsEmpty() const;

    /**
     * @brief Remove all backends from the cluster.
     */
    void Clear();

    /**
     * @brief Get backend indices for a specific accelerator type.
     *
     * Returns a vector of indices into the cluster for backends matching
     * the specified accelerator type. This enables efficient scheduling
     * decisions without iterating through all backends.
     *
     * @param acceleratorType The accelerator type to filter by (e.g., "GPU", "TPU").
     * @return Vector of backend indices matching the type. Empty if none match.
     */
    const std::vector<uint32_t>& GetBackendsByType(const std::string& acceleratorType) const;

    /**
     * @brief Check if the cluster has backends of a specific accelerator type.
     *
     * @param acceleratorType The accelerator type to check for.
     * @return true if at least one backend has this accelerator type.
     */
    bool HasAcceleratorType(const std::string& acceleratorType) const;

    /**
     * @brief Look up a backend index by its network address.
     *
     * @param address The backend address to search for.
     * @return Backend index (0 to GetN()-1), or -1 if not found.
     */
    int32_t GetBackendIndex(const Address& address) const;

  private:
    std::vector<Backend> m_backends; //!< The collection of backend servers
    std::map<std::string, std::vector<uint32_t>>
        m_typeIndex;                         //!< accelerator type → backend indices
    std::map<Address, uint32_t> m_addrIndex; //!< address → backend index
};

} // namespace ns3

#endif // CLUSTER_H
