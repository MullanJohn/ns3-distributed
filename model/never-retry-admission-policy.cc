/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "never-retry-admission-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NeverRetryAdmissionPolicy");
NS_OBJECT_ENSURE_REGISTERED(NeverRetryAdmissionPolicy);

TypeId
NeverRetryAdmissionPolicy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NeverRetryAdmissionPolicy")
                            .SetParent<AdmissionRetryPolicy>()
                            .SetGroupName("Continuum")
                            .AddConstructor<NeverRetryAdmissionPolicy>();
    return tid;
}

NeverRetryAdmissionPolicy::NeverRetryAdmissionPolicy()
{
    NS_LOG_FUNCTION(this);
}

NeverRetryAdmissionPolicy::~NeverRetryAdmissionPolicy()
{
    NS_LOG_FUNCTION(this);
}

bool
NeverRetryAdmissionPolicy::ShouldRetry(uint32_t /*rejectionCount*/) const
{
    return false;
}

std::string
NeverRetryAdmissionPolicy::GetName() const
{
    return "NeverRetry";
}

} // namespace ns3
