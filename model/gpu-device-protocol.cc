/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "gpu-device-protocol.h"

#include "accelerator.h"
#include "device-metrics-header.h"
#include "scaling-command-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("GpuDeviceProtocol");

NS_OBJECT_ENSURE_REGISTERED(GpuDeviceProtocol);

TypeId
GpuDeviceProtocol::GetTypeId()
{
    static TypeId tid = TypeId("ns3::GpuDeviceProtocol")
                            .SetParent<DeviceProtocol>()
                            .SetGroupName("Distributed")
                            .AddConstructor<GpuDeviceProtocol>();
    return tid;
}

GpuDeviceProtocol::GpuDeviceProtocol()
{
    NS_LOG_FUNCTION(this);
}

GpuDeviceProtocol::~GpuDeviceProtocol()
{
    NS_LOG_FUNCTION(this);
}

Ptr<Packet>
GpuDeviceProtocol::CreateMetricsPacket(Ptr<const Accelerator> accel)
{
    NS_LOG_FUNCTION(this << accel);

    DeviceMetricsHeader header;
    header.SetMessageType(DeviceMetricsHeader::DEVICE_METRICS);
    header.SetUtilization(accel->GetUtilization());
    header.SetCurrentPower(accel->GetCurrentPower());

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(header);
    return packet;
}

Ptr<DeviceMetrics>
GpuDeviceProtocol::ParseMetrics(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    DeviceMetricsHeader header;
    packet->RemoveHeader(header);

    Ptr<DeviceMetrics> metrics = Create<DeviceMetrics>();
    metrics->utilization = header.GetUtilization();
    metrics->currentPower = header.GetCurrentPower();

    return metrics;
}

Ptr<Packet>
GpuDeviceProtocol::CreateCommandPacket(Ptr<ScalingDecision> decision)
{
    NS_LOG_FUNCTION(this);

    ScalingCommandHeader header;
    header.SetMessageType(ScalingCommandHeader::SCALING_COMMAND);
    header.SetTargetStateIdx(decision->targetStateIdx);

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(header);
    return packet;
}

void
GpuDeviceProtocol::ApplyCommand(Ptr<Packet> packet, Ptr<Accelerator> accel)
{
    NS_LOG_FUNCTION(this << packet << accel);

    ScalingCommandHeader header;
    packet->RemoveHeader(header);

    NS_LOG_INFO("Applying scaling command: stateIdx=" << header.GetTargetStateIdx());

    accel->ApplyState(header.GetTargetStateIdx());
}

std::string
GpuDeviceProtocol::GetName() const
{
    return "GpuDevice";
}

} // namespace ns3
