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
 * @brief Test ScalingCommandHeader serialization round-trip.
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
        original.SetTargetStateIdx(42);

        NS_TEST_ASSERT_MSG_EQ(original.GetSerializedSize(),
                              ScalingCommandHeader::SERIALIZED_SIZE,
                              "Serialized size should be 5 bytes");

        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        ScalingCommandHeader deserialized;
        packet->RemoveHeader(deserialized);

        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              ScalingCommandHeader::SCALING_COMMAND,
                              "Message type should be SCALING_COMMAND");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetTargetStateIdx(),
                              42,
                              "Target state index should match");
    }
};

} // namespace

TestCase*
CreateScalingCommandHeaderTestCase()
{
    return new ScalingCommandHeaderTestCase;
}

} // namespace ns3
