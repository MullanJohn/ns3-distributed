/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "client-submission-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ClientSubmissionPolicy");
NS_OBJECT_ENSURE_REGISTERED(ClientSubmissionPolicy);

TypeId
ClientSubmissionPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ClientSubmissionPolicy").SetParent<Object>().SetGroupName("Continuum");
    return tid;
}

void
ClientSubmissionPolicy::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

} // namespace ns3
