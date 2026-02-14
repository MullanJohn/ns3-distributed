/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/device-metrics-header.h"
#include "ns3/packet.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test DeviceMetricsHeader serialization roundtrip
 */
class DeviceMetricsHeaderTestCase : public TestCase
{
  public:
    DeviceMetricsHeaderTestCase()
        : TestCase("Test DeviceMetricsHeader serialization roundtrip")
    {
    }

  private:
    void DoRun() override
    {
        DeviceMetricsHeader original;
        original.SetMessageType(DeviceMetricsHeader::DEVICE_METRICS);
        original.SetFrequency(1.5e9);
        original.SetVoltage(0.85);
        original.SetBusy(true);
        original.SetQueueLength(3);
        original.SetCurrentPower(150.5);

        // Verify serialized size
        NS_TEST_ASSERT_MSG_EQ(original.GetSerializedSize(),
                              DeviceMetricsHeader::SERIALIZED_SIZE,
                              "Serialized size should be 30 bytes");

        // Serialize and deserialize
        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        DeviceMetricsHeader deserialized;
        packet->RemoveHeader(deserialized);

        // Verify all fields
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              DeviceMetricsHeader::DEVICE_METRICS,
                              "Message type should be DEVICE_METRICS");
        NS_TEST_ASSERT_MSG_EQ_TOL(deserialized.GetFrequency(),
                                  1.5e9,
                                  1e-9,
                                  "Frequency should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(deserialized.GetVoltage(), 0.85, 1e-9, "Voltage should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetBusy(), true, "Busy should be true");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetQueueLength(), 3, "Queue length should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(deserialized.GetCurrentPower(),
                                  150.5,
                                  1e-9,
                                  "Current power should match");
    }
};

} // namespace

TestCase*
CreateDeviceMetricsHeaderTestCase()
{
    return new DeviceMetricsHeaderTestCase;
}

} // namespace ns3
