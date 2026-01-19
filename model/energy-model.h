/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ENERGY_MODEL_H
#define ENERGY_MODEL_H

#include "ns3/object.h"
#include "ns3/ptr.h"

namespace ns3
{

class Accelerator;

/**
 * @ingroup distributed
 * @brief Abstract base class for accelerator energy models.
 *
 * EnergyModel defines the interface for calculating power consumption
 * of computational accelerators. Concrete implementations include
 * DvfsEnergyModel for DVFS-based power modeling.
 *
 * Power consumption is modeled with two components:
 * - Static power: Always consumed when the device is powered on
 * - Dynamic power: Consumed during active computation
 *
 * Example usage:
 * @code
 * Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
 * energy->SetAttribute("StaticPower", DoubleValue(30.0));
 * energy->SetAttribute("EffectiveCapacitance", DoubleValue(2e-9));
 *
 * Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
 * gpu->SetAttribute("EnergyModel", PointerValue(energy));
 * @endcode
 */
class EnergyModel : public Object
{
  public:
    /**
     * @brief Represents the current power state of an accelerator.
     *
     * PowerState encapsulates the static and dynamic power components
     * of an accelerator's power consumption.
     */
    struct PowerState
    {
        double staticPower;  //!< Static/leakage power in Watts
        double dynamicPower; //!< Dynamic/switching power in Watts
        bool valid;          //!< Whether this power state is valid

        /**
         * @brief Default constructor - creates invalid power state.
         */
        PowerState()
            : staticPower(0.0),
              dynamicPower(0.0),
              valid(false)
        {
        }

        /**
         * @brief Constructor with power values.
         * @param staticP Static power in Watts.
         * @param dynamicP Dynamic power in Watts.
         */
        PowerState(double staticP, double dynamicP)
            : staticPower(staticP),
              dynamicPower(dynamicP),
              valid(true)
        {
        }

        /**
         * @brief Get total power consumption.
         * @return Sum of static and dynamic power in Watts.
         */
        double GetTotalPower() const
        {
            return staticPower + dynamicPower;
        }
    };

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Default constructor.
     */
    EnergyModel();

    /**
     * @brief Destructor.
     */
    ~EnergyModel() override;

    /**
     * @brief Calculate power consumption when accelerator is idle.
     *
     * @param accelerator The accelerator to calculate idle power for.
     * @return PowerState with idle power values.
     */
    virtual PowerState CalculateIdlePower(Ptr<Accelerator> accelerator) = 0;

    /**
     * @brief Calculate power consumption when accelerator is active.
     *
     * @param accelerator The accelerator to calculate active power for.
     * @param utilization Current utilization level [0.0, 1.0].
     * @return PowerState with active power values.
     */
    virtual PowerState CalculateActivePower(Ptr<Accelerator> accelerator,
                                            double utilization) = 0;

    /**
     * @brief Get the name of this energy model.
     *
     * @return A string identifying the energy model (e.g., "DVFS", "Fixed").
     */
    virtual std::string GetName() const = 0;

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // ENERGY_MODEL_H
