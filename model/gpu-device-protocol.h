/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef GPU_DEVICE_PROTOCOL_H
#define GPU_DEVICE_PROTOCOL_H

#include "device-protocol.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Concrete DeviceProtocol for GPU accelerators.
 *
 * Serializes metrics using DeviceMetricsHeader (type 4) and applies
 * commands from ScalingCommandHeader (type 5) by calling
 * SetFrequency() and SetVoltage() on the accelerator.
 */
class GpuDeviceProtocol : public DeviceProtocol
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    GpuDeviceProtocol();
    ~GpuDeviceProtocol() override;

    Ptr<Packet> CreateMetricsPacket(Ptr<const Accelerator> accel) override;
    Ptr<DeviceMetrics> ParseMetrics(Ptr<Packet> packet) override;
    Ptr<Packet> CreateCommandPacket(Ptr<ScalingDecision> decision) override;
    void ApplyCommand(Ptr<Packet> packet, Ptr<Accelerator> accel) override;
    std::string GetName() const override;
};

} // namespace ns3

#endif // GPU_DEVICE_PROTOCOL_H
