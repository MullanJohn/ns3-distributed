/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "always-submit-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AlwaysSubmitPolicy");
NS_OBJECT_ENSURE_REGISTERED(AlwaysSubmitPolicy);

TypeId
AlwaysSubmitPolicy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::AlwaysSubmitPolicy")
                            .SetParent<ClientSubmissionPolicy>()
                            .SetGroupName("Continuum")
                            .AddConstructor<AlwaysSubmitPolicy>();
    return tid;
}

AlwaysSubmitPolicy::AlwaysSubmitPolicy()
{
    NS_LOG_FUNCTION(this);
}

AlwaysSubmitPolicy::~AlwaysSubmitPolicy()
{
    NS_LOG_FUNCTION(this);
}

bool
AlwaysSubmitPolicy::ShouldSubmit(const ClientState& /*state*/)
{
    return true;
}

std::string
AlwaysSubmitPolicy::GetName() const
{
    return "AlwaysSubmit";
}

} // namespace ns3
