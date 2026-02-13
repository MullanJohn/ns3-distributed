/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef SCALING_POLICY_H
#define SCALING_POLICY_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"

#include <cstdint>
#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Polymorphic base for accelerator metrics reported by backends.
 *
 * Standard fields are defined here; subclasses add accelerator-specific fields.
 * Uses SimpleRefCount for Ptr<> support without full ns-3 Object overhead.
 */
class DeviceMetrics : public SimpleRefCount<DeviceMetrics>
{
  public:
    virtual ~DeviceMetrics() = default;

    double frequency{0};     //!< Current frequency in Hz
    double voltage{0};       //!< Current voltage in Volts
    bool busy{false};        //!< Currently processing a task?
    uint32_t queueLength{0}; //!< Tasks in queue (including current)
    double currentPower{0};  //!< Current power consumption in Watts
};

/**
 * @ingroup distributed
 * @brief Polymorphic base for scaling decisions.
 *
 * Standard fields are defined here; subclasses add accelerator-specific fields.
 */
class ScalingDecision : public SimpleRefCount<ScalingDecision>
{
  public:
    virtual ~ScalingDecision() = default;

    double targetFrequency{0}; //!< Target frequency in Hz
    double targetVoltage{0};   //!< Target voltage in Volts
};

/**
 * @ingroup distributed
 * @brief Abstract base class for DVFS scaling policies.
 *
 * ScalingPolicy decides target frequency/voltage for a backend accelerator
 * based on reported DeviceMetrics. The DeviceManager calls Decide() for
 * each backend when evaluating scaling.
 */
class ScalingPolicy : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ~ScalingPolicy() override;

    /**
     * @brief Decide on a scaling action based on device metrics.
     *
     * @param metrics Current device metrics from the backend.
     * @param minFrequency Lower frequency bound in Hz.
     * @param maxFrequency Upper frequency bound in Hz.
     * @return A scaling decision, or nullptr if no change needed.
     */
    virtual Ptr<ScalingDecision> Decide(Ptr<DeviceMetrics> metrics,
                                        double minFrequency,
                                        double maxFrequency) = 0;

    /**
     * @brief Get the name of this scaling policy.
     * @return A string identifying the policy.
     */
    virtual std::string GetName() const = 0;
};

} // namespace ns3

#endif // SCALING_POLICY_H
