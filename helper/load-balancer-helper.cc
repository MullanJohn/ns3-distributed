/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "load-balancer-helper.h"

#include "ns3/load-balancer.h"
#include "ns3/pointer.h"
#include "ns3/uinteger.h"

namespace ns3
{

LoadBalancerHelper::LoadBalancerHelper()
    : ApplicationHelper("ns3::LoadBalancer")
{
}

LoadBalancerHelper::LoadBalancerHelper(uint16_t port)
    : ApplicationHelper("ns3::LoadBalancer")
{
    m_factory.Set("Port", UintegerValue(port));
}

void
LoadBalancerHelper::SetPort(uint16_t port)
{
    m_factory.Set("Port", UintegerValue(port));
}

void
LoadBalancerHelper::SetCluster(const Cluster& cluster)
{
    m_cluster = cluster;
}

void
LoadBalancerHelper::SetScheduler(const std::string& schedulerTypeId)
{
    ObjectFactory factory;
    factory.SetTypeId(schedulerTypeId);
    Ptr<NodeScheduler> scheduler = factory.Create<NodeScheduler>();
    m_factory.Set("Scheduler", PointerValue(scheduler));
}

void
LoadBalancerHelper::SetScheduler(Ptr<NodeScheduler> scheduler)
{
    m_factory.Set("Scheduler", PointerValue(scheduler));
}

void
LoadBalancerHelper::SetFrontendConnectionManager(Ptr<ConnectionManager> connMgr)
{
    m_factory.Set("FrontendConnectionManager", PointerValue(connMgr));
}

void
LoadBalancerHelper::SetBackendConnectionManager(Ptr<ConnectionManager> connMgr)
{
    m_factory.Set("BackendConnectionManager", PointerValue(connMgr));
}

Ptr<Application>
LoadBalancerHelper::DoInstall(Ptr<Node> node)
{
    Ptr<Application> app = ApplicationHelper::DoInstall(node);
    Ptr<LoadBalancer> lb = DynamicCast<LoadBalancer>(app);
    if (lb)
    {
        lb->SetCluster(m_cluster);
    }
    return app;
}

} // namespace ns3
