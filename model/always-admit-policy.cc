/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "always-admit-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AlwaysAdmitPolicy");
NS_OBJECT_ENSURE_REGISTERED(AlwaysAdmitPolicy);

TypeId
AlwaysAdmitPolicy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::AlwaysAdmitPolicy")
                            .SetParent<AdmissionPolicy>()
                            .SetGroupName("Distributed")
                            .AddConstructor<AlwaysAdmitPolicy>();
    return tid;
}

AlwaysAdmitPolicy::AlwaysAdmitPolicy()
{
    NS_LOG_FUNCTION(this);
}

AlwaysAdmitPolicy::~AlwaysAdmitPolicy()
{
    NS_LOG_FUNCTION(this);
}

bool
AlwaysAdmitPolicy::ShouldAdmit(const WorkloadMetrics& metrics,
                               const Cluster& cluster,
                               uint32_t activeWorkloadCount)
{
    NS_LOG_FUNCTION(this << metrics.taskCount << activeWorkloadCount);
    NS_LOG_DEBUG("AlwaysAdmit: admitting workload with " << metrics.taskCount << " tasks");
    return true;
}

std::string
AlwaysAdmitPolicy::GetName() const
{
    return "AlwaysAdmit";
}

} // namespace ns3
