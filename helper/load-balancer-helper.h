/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef LOAD_BALANCER_HELPER_H
#define LOAD_BALANCER_HELPER_H

#include "ns3/application-helper.h"
#include "ns3/cluster.h"
#include "ns3/node-scheduler.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Helper to create and configure LoadBalancer applications.
 *
 * This helper simplifies the creation and configuration of LoadBalancer
 * applications for distributed computing simulations. It provides methods
 * to configure the listening port, cluster of backend servers, and
 * scheduling policy.
 *
 * Example usage:
 * @code
 * // Create cluster of backend servers
 * Cluster cluster;
 * cluster.AddBackend(serverNode1, InetSocketAddress(addr1, 9000));
 * cluster.AddBackend(serverNode2, InetSocketAddress(addr2, 9000));
 *
 * // Create and configure helper
 * LoadBalancerHelper lbHelper(8000);
 * lbHelper.SetCluster(cluster);
 * lbHelper.SetScheduler("ns3::RoundRobinScheduler");
 *
 * // Install on load balancer node
 * ApplicationContainer lbApps = lbHelper.Install(lbNode);
 * lbApps.Start(Seconds(0.0));
 * @endcode
 */
class LoadBalancerHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default LoadBalancer type.
     */
    LoadBalancerHelper();

    /**
     * @brief Construct helper with specified port.
     * @param port Port number to listen on for client connections.
     */
    explicit LoadBalancerHelper(uint16_t port);

    /**
     * @brief Set the port to listen on.
     * @param port Port number.
     */
    void SetPort(uint16_t port);

    /**
     * @brief Set the cluster of backend servers.
     *
     * The cluster must be set before Install() is called.
     *
     * @param cluster The cluster to load balance across.
     */
    void SetCluster(const Cluster& cluster);

    /**
     * @brief Set the scheduler type.
     *
     * @param schedulerTypeId The TypeId of the scheduler (e.g., "ns3::RoundRobinScheduler").
     */
    void SetScheduler(const std::string& schedulerTypeId);

    /**
     * @brief Set the scheduler directly.
     *
     * @param scheduler The scheduler object to use.
     */
    void SetScheduler(Ptr<NodeScheduler> scheduler);

  private:
    /**
     * @brief Install applications on a node.
     *
     * Overridden to set the cluster on each installed application.
     *
     * @param node The node to install on.
     * @return The installed application.
     */
    Ptr<Application> DoInstall(Ptr<Node> node) override;

    Cluster m_cluster; //!< Backend server cluster
};

} // namespace ns3

#endif // LOAD_BALANCER_HELPER_H
