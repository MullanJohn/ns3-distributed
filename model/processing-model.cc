/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "processing-model.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ProcessingModel");

NS_OBJECT_ENSURE_REGISTERED(ProcessingModel);

TypeId
ProcessingModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ProcessingModel").SetParent<Object>().SetGroupName("Distributed");
    return tid;
}

ProcessingModel::ProcessingModel()
{
    NS_LOG_FUNCTION(this);
}

ProcessingModel::~ProcessingModel()
{
    NS_LOG_FUNCTION(this);
}

void
ProcessingModel::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

} // namespace ns3
