/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DEVICE_PROTOCOL_H
#define DEVICE_PROTOCOL_H

#include "scaling-policy.h"

#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"


namespace ns3
{

class Accelerator;

/**
 * @ingroup distributed
 * @brief Abstract base class for device management wire protocols.
 *
 * DeviceProtocol encapsulates how accelerator metrics are serialized into
 * packets and how scaling commands are deserialized and applied. Each
 * accelerator type provides its own concrete protocol implementation.
 *
 * Used by:
 * - The server application to create metrics packets and apply incoming commands.
 * - The DeviceManager to parse metrics and create command packets.
 */
class DeviceProtocol : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ~DeviceProtocol() override;

    /**
     * @brief Serialize accelerator state into a metrics packet.
     *
     * Called by the server application on task lifecycle events.
     *
     * @param accel The accelerator whose state is read.
     * @return A packet containing the serialized metrics header.
     */
    virtual Ptr<Packet> CreateMetricsPacket(Ptr<const Accelerator> accel) = 0;

    /**
     * @brief Parse a metrics packet into a DeviceMetrics object.
     *
     * Called by the DeviceManager when a type-4 packet arrives.
     *
     * @param packet The packet containing the metrics header.
     * @return The parsed DeviceMetrics.
     */
    virtual Ptr<DeviceMetrics> ParseMetrics(Ptr<Packet> packet) = 0;

    /**
     * @brief Serialize a scaling decision into a command packet.
     *
     * Called by the DeviceManager when a scaling decision is made.
     *
     * @param decision The scaling decision to serialize.
     * @return A packet containing the serialized command header.
     */
    virtual Ptr<Packet> CreateCommandPacket(Ptr<ScalingDecision> decision) = 0;

    /**
     * @brief Parse and apply a command packet to an accelerator.
     *
     * Called by the server application when a type-5 packet arrives.
     *
     * @param packet The packet containing the command header.
     * @param accel The accelerator to which the command is applied.
     */
    virtual void ApplyCommand(Ptr<Packet> packet, Ptr<Accelerator> accel) = 0;
};

} // namespace ns3

#endif // DEVICE_PROTOCOL_H
