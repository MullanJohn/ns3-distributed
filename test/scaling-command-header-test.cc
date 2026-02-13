/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/packet.h"
#include "ns3/scaling-command-header.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test ScalingCommandHeader serialization roundtrip
 */
class ScalingCommandHeaderTestCase : public TestCase
{
  public:
    ScalingCommandHeaderTestCase()
        : TestCase("Test ScalingCommandHeader serialization roundtrip")
    {
    }

  private:
    void DoRun() override
    {
        ScalingCommandHeader original;
        original.SetMessageType(ScalingCommandHeader::SCALING_COMMAND);
        original.SetTargetFrequency(750e6);
        original.SetTargetVoltage(0.7);

        // Verify serialized size
        NS_TEST_ASSERT_MSG_EQ(original.GetSerializedSize(),
                              ScalingCommandHeader::SERIALIZED_SIZE,
                              "Serialized size should be 17 bytes");

        // Serialize and deserialize
        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        ScalingCommandHeader deserialized;
        packet->RemoveHeader(deserialized);

        // Verify all fields
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              ScalingCommandHeader::SCALING_COMMAND,
                              "Message type should be SCALING_COMMAND");
        NS_TEST_ASSERT_MSG_EQ_TOL(deserialized.GetTargetFrequency(), 750e6, 1e-9,
                                  "Target frequency should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(deserialized.GetTargetVoltage(), 0.7, 1e-9,
                                  "Target voltage should match");
    }
};

} // namespace

TestCase*
CreateScalingCommandHeaderTestCase()
{
    return new ScalingCommandHeaderTestCase;
}

} // namespace ns3
