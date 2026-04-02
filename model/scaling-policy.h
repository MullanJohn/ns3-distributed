/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef SCALING_POLICY_H
#define SCALING_POLICY_H

#include "accelerator.h"
#include "cluster-state.h"

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"

#include <cstdint>
#include <vector>

namespace ns3
{

class DeviceMetrics : public SimpleRefCount<DeviceMetrics>
{
  public:
    virtual ~DeviceMetrics() = default;

    double frequency{0};     //!< Current operating frequency in Hz
    double voltage{0};       //!< Current operating voltage in Volts
    bool busy{false};        //!< Whether the device is currently executing a task
    double utilization{0};   //!< Rolling compute utilization [0.0, 1.0]
    uint32_t queueLength{0}; //!< Number of queued or running tasks
    double currentPower{0};  //!< Current total power draw in Watts
};

class ScalingDecision : public SimpleRefCount<ScalingDecision>
{
  public:
    virtual ~ScalingDecision() = default;

    double targetFrequency{0}; //!< Target operating frequency in Hz
    double targetVoltage{0};   //!< Target operating voltage in Volts
};

class ScalingPolicy : public Object
{
  public:
    static TypeId GetTypeId();
    ~ScalingPolicy() override;
    virtual Ptr<ScalingDecision> Decide(uint32_t backendIdx,
                                        const ClusterState::BackendState& backend,
                                        const std::vector<OperatingPoint>& opps) = 0;
    virtual std::string GetName() const = 0;
};

} // namespace ns3

#endif // SCALING_POLICY_H
