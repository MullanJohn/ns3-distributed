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

#include <string>
#include <vector>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Represents a cluster of backend server nodes for load balancing.
 *
 * A Cluster holds references to server nodes and their addresses. It provides
 * iteration and access patterns similar to NodeContainer. The Cluster is used
 * by NodeScheduler implementations to select which backend should handle
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
        Ptr<Node> node;              //!< The backend server node (may have GpuAccelerator aggregated)
        Address address;             //!< Server address (InetSocketAddress with IP and port)
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

  private:
    std::vector<Backend> m_backends; //!< The collection of backend servers
};

} // namespace ns3

#endif // CLUSTER_H
