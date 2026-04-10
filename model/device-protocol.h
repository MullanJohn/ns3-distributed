/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DEVICE_PROTOCOL_H
#define DEVICE_PROTOCOL_H

#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"

namespace ns3
{

class Accelerator;
class DeviceMetrics;

/**
 * @ingroup continuum
 * @brief Abstract base class for device metrics protocols.
 *
 * DeviceProtocol encapsulates how accelerator metrics are serialized into
 * packets and parsed back into DeviceMetrics objects. Each accelerator type
 * provides its own concrete protocol implementation.
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
     * @brief Get the name of this device protocol.
     * @return A string identifying the protocol.
     */
    virtual std::string GetName() const = 0;
};

} // namespace ns3

#endif // DEVICE_PROTOCOL_H
