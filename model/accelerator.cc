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
            .SetGroupName("Continuum")
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
      m_lastEnergyUpdateTime(Seconds(-1)),
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
Accelerator::GetUtilization() const
{
    return 0.0;
}

double
Accelerator::GetComputeRate() const
{
    return 0.0;
}

void
Accelerator::ApplyState(uint32_t stateIdx)
{
    NS_LOG_FUNCTION(this << stateIdx);
}

uint32_t
Accelerator::GetNumPerformanceStates() const
{
    return 0;
}

double
Accelerator::GetComputeRateAtState(uint32_t /* stateIdx */) const
{
    return GetComputeRate();
}

Ptr<Node>
Accelerator::GetNode() const
{
    return m_node;
}

double
Accelerator::GetCurrentPower() const
{
    if (!m_energyModel)
    {
        return m_currentPower;
    }

    if (m_lastEnergyUpdateTime.IsStrictlyNegative())
    {
        EnergyModel::PowerState idleState =
            m_energyModel->CalculateIdlePower(const_cast<Accelerator*>(this));
        return idleState.GetTotalPower();
    }

    return m_currentPower;
}

double
Accelerator::GetTotalEnergy() const
{
    if (!m_energyModel)
    {
        return m_totalEnergy;
    }

    Time now = Simulator::Now();

    if (m_lastEnergyUpdateTime.IsStrictlyNegative())
    {
        EnergyModel::PowerState idleState =
            m_energyModel->CalculateIdlePower(const_cast<Accelerator*>(this));
        return (now > Seconds(0)) ? idleState.GetTotalPower() * now.GetSeconds() : 0.0;
    }

    double totalEnergy = m_totalEnergy;
    if (m_lastEnergyUpdateTime < now)
    {
        double elapsed = (now - m_lastEnergyUpdateTime).GetSeconds();
        if (elapsed > 0)
        {
            totalEnergy += m_currentPower * elapsed;
        }
    }
    return totalEnergy;
}

void
Accelerator::UpdateEnergyState(bool active, double utilization)
{
    NS_LOG_FUNCTION(this << active << utilization);

    if (!m_energyModel)
    {
        return;
    }

    Time now = Simulator::Now();

    if (m_lastEnergyUpdateTime.IsStrictlyNegative())
    {
        EnergyModel::PowerState idleState = m_energyModel->CalculateIdlePower(this);
        m_currentPower = idleState.GetTotalPower();
        if (now > Seconds(0))
        {
            m_totalEnergy = m_currentPower * now.GetSeconds();
        }
        m_lastEnergyUpdateTime = now;
    }

    // Accumulate energy from previous state
    if (m_lastEnergyUpdateTime < now)
    {
        double elapsed = (now - m_lastEnergyUpdateTime).GetSeconds();
        if (elapsed > 0)
        {
            m_totalEnergy += m_currentPower * elapsed;
        }
    }

    EnergyModel::PowerState powerState =
        active ? m_energyModel->CalculateActivePower(this, utilization)
               : m_energyModel->CalculateIdlePower(this);
    m_currentPower = powerState.GetTotalPower();
    m_powerTrace(m_currentPower);
    m_energyTrace(m_totalEnergy);
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
        Time now = Simulator::Now();
        if (!m_lastEnergyUpdateTime.IsStrictlyNegative() && m_lastEnergyUpdateTime < now)
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
