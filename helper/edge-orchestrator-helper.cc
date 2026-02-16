/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "edge-orchestrator-helper.h"

#include "ns3/edge-orchestrator.h"
#include "ns3/uinteger.h"

namespace ns3
{

EdgeOrchestratorHelper::EdgeOrchestratorHelper()
    : ApplicationHelper("ns3::EdgeOrchestrator")
{
}

EdgeOrchestratorHelper::EdgeOrchestratorHelper(uint16_t port)
    : ApplicationHelper("ns3::EdgeOrchestrator")
{
    m_factory.Set("Port", UintegerValue(port));
}

void
EdgeOrchestratorHelper::SetPort(uint16_t port)
{
    m_factory.Set("Port", UintegerValue(port));
}

void
EdgeOrchestratorHelper::SetCluster(const Cluster& cluster)
{
    m_cluster = cluster;
}

Ptr<Application>
EdgeOrchestratorHelper::DoInstall(Ptr<Node> node)
{
    Ptr<Application> app = ApplicationHelper::DoInstall(node);
    Ptr<EdgeOrchestrator> orchestrator = DynamicCast<EdgeOrchestrator>(app);
    orchestrator->SetCluster(m_cluster);
    return app;
}

} // namespace ns3
