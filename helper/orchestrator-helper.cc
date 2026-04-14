/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "orchestrator-helper.h"

#include "ns3/orchestrator.h"
#include "ns3/uinteger.h"

namespace ns3
{

OrchestratorHelper::OrchestratorHelper()
    : ApplicationHelper("ns3::Orchestrator")
{
}

OrchestratorHelper::OrchestratorHelper(uint16_t port)
    : ApplicationHelper("ns3::Orchestrator")
{
    m_factory.Set("Port", UintegerValue(port));
}

void
OrchestratorHelper::SetPort(uint16_t port)
{
    m_factory.Set("Port", UintegerValue(port));
}

void
OrchestratorHelper::SetCluster(const Cluster& cluster)
{
    m_cluster = cluster;
}

Ptr<Application>
OrchestratorHelper::DoInstall(Ptr<Node> node)
{
    Ptr<Application> app = ApplicationHelper::DoInstall(node);
    Ptr<Orchestrator> orchestrator = DynamicCast<Orchestrator>(app);
    orchestrator->SetCluster(m_cluster);
    return app;
}

} // namespace ns3
