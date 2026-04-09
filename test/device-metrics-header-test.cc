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
 * @brief Test DeviceMetricsHeader serialization round-trip.
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
        original.SetUtilization(0.42);
        original.SetCurrentPower(150.5);

        NS_TEST_ASSERT_MSG_EQ(original.GetSerializedSize(),
                              DeviceMetricsHeader::SERIALIZED_SIZE,
                              "Serialized size should be 17 bytes");

        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        DeviceMetricsHeader deserialized;
        packet->RemoveHeader(deserialized);

        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              DeviceMetricsHeader::DEVICE_METRICS,
                              "Message type should be DEVICE_METRICS");
        NS_TEST_ASSERT_MSG_EQ_TOL(deserialized.GetUtilization(),
                                  0.42,
                                  1e-9,
                                  "Utilization should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(deserialized.GetCurrentPower(),
                                  150.5,
                                  1e-9,
                                  "Current power should match");
    }
};

} // namespace

TestCase* CreateDeviceMetricsHeaderTestCase() { return new DeviceMetricsHeaderTestCase; }

} // namespace ns3
