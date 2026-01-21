/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "energy-model.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("EnergyModel");

NS_OBJECT_ENSURE_REGISTERED(EnergyModel);

TypeId
EnergyModel::GetTypeId()
{
    static TypeId tid = TypeId("ns3::EnergyModel").SetParent<Object>().SetGroupName("Distributed");
    return tid;
}

EnergyModel::EnergyModel()
{
    NS_LOG_FUNCTION(this);
}

EnergyModel::~EnergyModel()
{
    NS_LOG_FUNCTION(this);
}

void
EnergyModel::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Object::DoDispose();
}

} // namespace ns3
