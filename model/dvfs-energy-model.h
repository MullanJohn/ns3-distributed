/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DVFS_ENERGY_MODEL_H
#define DVFS_ENERGY_MODEL_H

#include "energy-model.h"

#include "ns3/ptr.h"

namespace ns3
{

class Accelerator;

/**
 * @ingroup distributed
 * @brief DVFS-based energy model for accelerators.
 *
 * DvfsEnergyModel implements the standard DVFS power equation:
 * P_dynamic = C * V^2 * f * utilization
 *
 * Where:
 * - C is the effective capacitance (switching capacitance)
 * - V is the operating voltage
 * - f is the operating frequency
 * - utilization is the fraction of time the accelerator is active [0.0, 1.0]
 *
 * Total power = P_static + P_dynamic
 *
 * Example usage:
 * @code
 * Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
 * energy->SetAttribute("StaticPower", DoubleValue(30.0));     // 30W static
 * energy->SetAttribute("EffectiveCapacitance", DoubleValue(2e-9));  // 2nF
 *
 * // For GPU with 1.5GHz, 0.9V:
 * // P_dynamic = 2e-9 * 0.81 * 1.5e9 = 2.43W
 * // Total when active: 30 + 2.43 = 32.43W
 * @endcode
 */
class DvfsEnergyModel : public EnergyModel
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    DvfsEnergyModel();
    ~DvfsEnergyModel() override;

    // EnergyModel interface implementation
    PowerState CalculateIdlePower(Ptr<Accelerator> accelerator) override;
    PowerState CalculateActivePower(Ptr<Accelerator> accelerator, double utilization) override;
    std::string GetName() const override;

    /**
     * @brief Get the effective capacitance.
     * @return Effective capacitance in Farads.
     */
    double GetEffectiveCapacitance() const;

    /**
     * @brief Get the static power.
     * @return Static power in Watts.
     */
    double GetStaticPower() const;

  protected:
    void DoDispose() override;

  private:
    double m_effectiveCapacitance; //!< Effective capacitance in Farads
    double m_staticPower;          //!< Static/leakage power in Watts
};

} // namespace ns3

#endif // DVFS_ENERGY_MODEL_H
