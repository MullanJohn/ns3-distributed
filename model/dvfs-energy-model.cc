/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "dvfs-energy-model.h"

#include "accelerator.h"

#include "ns3/double.h"
#include "ns3/log.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DvfsEnergyModel");

NS_OBJECT_ENSURE_REGISTERED(DvfsEnergyModel);

TypeId
DvfsEnergyModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DvfsEnergyModel")
            .SetParent<EnergyModel>()
            .SetGroupName("Distributed")
            .AddConstructor<DvfsEnergyModel>()
            .AddAttribute("EffectiveCapacitance",
                          "Effective capacitance in Farads (C in P = C*V^2*f)",
                          DoubleValue(1e-9),
                          MakeDoubleAccessor(&DvfsEnergyModel::m_effectiveCapacitance),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("StaticPower",
                          "Static/leakage power in Watts",
                          DoubleValue(10.0),
                          MakeDoubleAccessor(&DvfsEnergyModel::m_staticPower),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

DvfsEnergyModel::DvfsEnergyModel()
    : m_effectiveCapacitance(1e-9),
      m_staticPower(10.0)
{
    NS_LOG_FUNCTION(this);
}

DvfsEnergyModel::~DvfsEnergyModel()
{
    NS_LOG_FUNCTION(this);
}

void
DvfsEnergyModel::DoDispose()
{
    NS_LOG_FUNCTION(this);
    EnergyModel::DoDispose();
}

std::string
DvfsEnergyModel::GetName() const
{
    return "DVFS";
}

EnergyModel::PowerState
DvfsEnergyModel::CalculateIdlePower(Ptr<Accelerator> accelerator)
{
    NS_LOG_FUNCTION(this << accelerator);

    // Idle state: only static power, no dynamic power
    return EnergyModel::PowerState(m_staticPower, 0.0);
}

EnergyModel::PowerState
DvfsEnergyModel::CalculateActivePower(Ptr<Accelerator> accelerator, double utilization)
{
    NS_LOG_FUNCTION(this << accelerator << utilization);

    if (!accelerator)
    {
        NS_LOG_WARN("CalculateActivePower called with null accelerator");
        return EnergyModel::PowerState();
    }

    // Clamp utilization to [0.0, 1.0]
    utilization = std::max(0.0, std::min(1.0, utilization));

    // Get voltage and frequency from accelerator
    double voltage = accelerator->GetVoltage();
    double frequency = accelerator->GetFrequency();

    // Calculate dynamic power: P = C * V^2 * f * utilization
    double dynamicPower = m_effectiveCapacitance * voltage * voltage * frequency * utilization;

    NS_LOG_DEBUG("DVFS power calculation: C=" << m_effectiveCapacitance << " V=" << voltage
                                              << " f=" << frequency << " util=" << utilization
                                              << " -> P_dyn=" << dynamicPower);

    return EnergyModel::PowerState(m_staticPower, dynamicPower);
}

double
DvfsEnergyModel::GetEffectiveCapacitance() const
{
    return m_effectiveCapacitance;
}

double
DvfsEnergyModel::GetStaticPower() const
{
    return m_staticPower;
}

} // namespace ns3
