/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "accelerator.h"

#include "energy-model.h"

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Accelerator");

NS_OBJECT_ENSURE_REGISTERED(Accelerator);

TypeId
Accelerator::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Accelerator")
            .SetParent<Object>()
            .SetGroupName("Distributed")
            .AddAttribute("EnergyModel",
                          "Energy model for power consumption calculation",
                          PointerValue(),
                          MakePointerAccessor(&Accelerator::m_energyModel),
                          MakePointerChecker<EnergyModel>())
            .AddTraceSource("TaskStarted",
                            "Trace fired when a task starts execution.",
                            MakeTraceSourceAccessor(&Accelerator::m_taskStartedTrace),
                            "ns3::Accelerator::TaskTracedCallback")
            .AddTraceSource("TaskCompleted",
                            "Trace fired when a task completes execution.",
                            MakeTraceSourceAccessor(&Accelerator::m_taskCompletedTrace),
                            "ns3::Accelerator::TaskCompletedTracedCallback")
            .AddTraceSource("TaskFailed",
                            "Trace fired when a task fails to process.",
                            MakeTraceSourceAccessor(&Accelerator::m_taskFailedTrace),
                            "ns3::Accelerator::TaskFailedTracedCallback")
            .AddTraceSource("CurrentPower",
                            "Trace fired when power state changes.",
                            MakeTraceSourceAccessor(&Accelerator::m_powerTrace),
                            "ns3::Accelerator::PowerTracedCallback")
            .AddTraceSource("TotalEnergy",
                            "Trace fired when total energy is updated.",
                            MakeTraceSourceAccessor(&Accelerator::m_energyTrace),
                            "ns3::Accelerator::EnergyTracedCallback")
            .AddTraceSource("TaskEnergy",
                            "Trace fired when a task completes with its energy consumption.",
                            MakeTraceSourceAccessor(&Accelerator::m_taskEnergyTrace),
                            "ns3::Accelerator::TaskEnergyTracedCallback");
    return tid;
}

Accelerator::Accelerator()
    : m_node(nullptr),
      m_energyModel(nullptr),
      m_lastEnergyUpdateTime(Seconds(0)),
      m_totalEnergy(0.0),
      m_currentPower(0.0),
      m_taskStartEnergy(0.0)
{
    NS_LOG_FUNCTION(this);
}

Accelerator::~Accelerator()
{
    NS_LOG_FUNCTION(this);
}

uint32_t
Accelerator::GetQueueLength() const
{
    return 0;
}

bool
Accelerator::IsBusy() const
{
    return false;
}

double
Accelerator::GetVoltage() const
{
    return 1.0;
}

double
Accelerator::GetFrequency() const
{
    return 1.0;
}

Ptr<Node>
Accelerator::GetNode() const
{
    return m_node;
}

double
Accelerator::GetCurrentPower() const
{
    return m_currentPower;
}

double
Accelerator::GetTotalEnergy() const
{
    return m_totalEnergy;
}

void
Accelerator::UpdateEnergyState(bool active, double utilization)
{
    NS_LOG_FUNCTION(this << active << utilization);

    if (!m_energyModel)
    {
        // No energy model configured - nothing to do
        return;
    }

    Time now = Simulator::Now();

    // Accumulate energy from previous state
    if (m_lastEnergyUpdateTime < now)
    {
        double elapsed = (now - m_lastEnergyUpdateTime).GetSeconds();
        if (elapsed > 0)
        {
            m_totalEnergy += m_currentPower * elapsed;
        }
    }

    // Calculate new power state
    EnergyModel::PowerState powerState;
    if (active)
    {
        powerState = m_energyModel->CalculateActivePower(this, utilization);
    }
    else
    {
        powerState = m_energyModel->CalculateIdlePower(this);
    }

    if (powerState.valid)
    {
        m_currentPower = powerState.GetTotalPower();
        m_powerTrace(m_currentPower);
        m_energyTrace(m_totalEnergy);

        NS_LOG_DEBUG("Energy state updated: power=" << m_currentPower << "W, totalEnergy="
                                                    << m_totalEnergy << "J");
    }

    m_lastEnergyUpdateTime = now;
}

void
Accelerator::RecordTaskStartEnergy()
{
    NS_LOG_FUNCTION(this);
    m_taskStartEnergy = m_totalEnergy;
}

double
Accelerator::GetTaskEnergy() const
{
    return m_totalEnergy - m_taskStartEnergy;
}

void
Accelerator::DoDispose()
{
    NS_LOG_FUNCTION(this);

    // Final energy update if we have an active energy model
    if (m_energyModel)
    {
        // Accumulate any remaining energy before disposal
        Time now = Simulator::Now();
        if (m_lastEnergyUpdateTime < now)
        {
            double elapsed = (now - m_lastEnergyUpdateTime).GetSeconds();
            if (elapsed > 0)
            {
                m_totalEnergy += m_currentPower * elapsed;
            }
        }
        m_energyModel = nullptr;
    }

    m_node = nullptr;
    Object::DoDispose();
}

void
Accelerator::NotifyNewAggregate()
{
    NS_LOG_FUNCTION(this);
    if (!m_node)
    {
        Ptr<Node> node = GetObject<Node>();
        if (node)
        {
            m_node = node;
            NS_LOG_DEBUG("Accelerator aggregated to node " << m_node->GetId());
        }
    }
    Object::NotifyNewAggregate();
}

} // namespace ns3
