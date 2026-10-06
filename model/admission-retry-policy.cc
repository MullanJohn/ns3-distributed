/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "admission-retry-policy.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AdmissionRetryPolicy");
NS_OBJECT_ENSURE_REGISTERED(AdmissionRetryPolicy);

TypeId
AdmissionRetryPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AdmissionRetryPolicy").SetParent<Object>().SetGroupName("Continuum");
    return tid;
}

void
AdmissionRetryPolicy::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

} // namespace ns3
