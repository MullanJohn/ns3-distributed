/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "always-retry-admission-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AlwaysRetryAdmissionPolicy");
NS_OBJECT_ENSURE_REGISTERED(AlwaysRetryAdmissionPolicy);

TypeId
AlwaysRetryAdmissionPolicy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::AlwaysRetryAdmissionPolicy")
                            .SetParent<AdmissionRetryPolicy>()
                            .SetGroupName("Continuum")
                            .AddConstructor<AlwaysRetryAdmissionPolicy>();
    return tid;
}

AlwaysRetryAdmissionPolicy::AlwaysRetryAdmissionPolicy()
{
    NS_LOG_FUNCTION(this);
}

AlwaysRetryAdmissionPolicy::~AlwaysRetryAdmissionPolicy()
{
    NS_LOG_FUNCTION(this);
}

bool
AlwaysRetryAdmissionPolicy::ShouldRetry(uint32_t /*rejectionCount*/) const
{
    return true;
}

std::string
AlwaysRetryAdmissionPolicy::GetName() const
{
    return "AlwaysRetry";
}

} // namespace ns3
