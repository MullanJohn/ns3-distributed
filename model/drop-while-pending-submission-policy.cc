/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "drop-while-pending-submission-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DropWhilePendingSubmissionPolicy");
NS_OBJECT_ENSURE_REGISTERED(DropWhilePendingSubmissionPolicy);

TypeId
DropWhilePendingSubmissionPolicy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DropWhilePendingSubmissionPolicy")
                            .SetParent<ClientSubmissionPolicy>()
                            .SetGroupName("Continuum")
                            .AddConstructor<DropWhilePendingSubmissionPolicy>();
    return tid;
}

DropWhilePendingSubmissionPolicy::DropWhilePendingSubmissionPolicy()
{
    NS_LOG_FUNCTION(this);
}

DropWhilePendingSubmissionPolicy::~DropWhilePendingSubmissionPolicy()
{
    NS_LOG_FUNCTION(this);
}

bool
DropWhilePendingSubmissionPolicy::ShouldSubmit(const ClientState& state)
{
    return state.pendingCount == 0;
}

std::string
DropWhilePendingSubmissionPolicy::GetName() const
{
    return "DropWhilePending";
}

} // namespace ns3
