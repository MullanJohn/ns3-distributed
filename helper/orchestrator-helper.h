/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ORCHESTRATOR_HELPER_H
#define ORCHESTRATOR_HELPER_H

#include "ns3/application-helper.h"
#include "ns3/cluster.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Helper to create and configure Orchestrator applications.
 *
 * This helper simplifies the creation and configuration of Orchestrator
 * applications. It extends ApplicationHelper with cluster configuration
 * that is applied during installation.
 */
class OrchestratorHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default Orchestrator type.
     */
    OrchestratorHelper();

    /**
     * @brief Construct helper with specified port.
     * @param port Port number to listen on.
     */
    explicit OrchestratorHelper(uint16_t port);

    /**
     * @brief Set the port to listen on.
     * @param port Port number.
     */
    void SetPort(uint16_t port);

    /**
     * @brief Set the cluster of backend nodes.
     * @param cluster The cluster configuration.
     */
    void SetCluster(const Cluster& cluster);

  protected:
    Ptr<Application> DoInstall(Ptr<Node> node) override;

  private:
    Cluster m_cluster; //!< Backend cluster configuration
};

} // namespace ns3

#endif // ORCHESTRATOR_HELPER_H
