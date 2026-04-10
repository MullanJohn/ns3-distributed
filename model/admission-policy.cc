/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "admission-policy.h"

#include "dag-task.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AdmissionPolicy");
NS_OBJECT_ENSURE_REGISTERED(AdmissionPolicy);

TypeId
AdmissionPolicy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AdmissionPolicy").SetParent<Object>().SetGroupName("Continuum");
    // Note: No AddConstructor because this is an abstract class
    return tid;
}

AdmissionPolicy::AdmissionPolicy()
{
    NS_LOG_FUNCTION(this);
}

AdmissionPolicy::~AdmissionPolicy()
{
    NS_LOG_FUNCTION(this);
}

std::map<std::string, uint32_t>
AdmissionPolicy::GetReservationTaskCounts(Ptr<DagTask> dag) const
{
    NS_LOG_FUNCTION(this << dag);

    std::map<std::string, uint32_t> counts;
    if (dag)
    {
        counts[""] = dag->GetTaskCount();
    }
    return counts;
}

void
AdmissionPolicy::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

} // namespace ns3
