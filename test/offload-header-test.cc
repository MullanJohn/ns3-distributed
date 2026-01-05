/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/offload-header.h"
#include "ns3/packet.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test OffloadHeader serialization and deserialization
 */
class OffloadHeaderTestCase : public TestCase
{
  public:
    OffloadHeaderTestCase()
        : TestCase("Test OffloadHeader serialization roundtrip")
    {
    }

  private:
    void DoRun() override
    {
        // Create and configure original header
        OffloadHeader original;
        original.SetMessageType(OffloadHeader::TASK_REQUEST);
        original.SetTaskId(12345);
        original.SetComputeDemand(5.5e9);
        original.SetInputSize(1024 * 1024);
        original.SetOutputSize(512 * 1024);

        // Verify serialized size
        uint32_t expectedSize = sizeof(uint8_t) +   // messageType
                                sizeof(uint64_t) +  // taskId
                                sizeof(uint64_t) +  // computeDemand (as double)
                                sizeof(uint64_t) +  // inputSize
                                sizeof(uint64_t);   // outputSize
        NS_TEST_ASSERT_MSG_EQ(original.GetSerializedSize(),
                              expectedSize,
                              "Serialized size should be 33 bytes");

        // Create packet with header
        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        // Deserialize header
        OffloadHeader deserialized;
        packet->RemoveHeader(deserialized);

        // Verify all fields match
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              OffloadHeader::TASK_REQUEST,
                              "Message type should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetTaskId(), 12345, "Task ID should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(deserialized.GetComputeDemand(),
                                  5.5e9,
                                  1e-6,
                                  "Compute demand should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetInputSize(),
                              1024 * 1024,
                              "Input size should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetOutputSize(),
                              512 * 1024,
                              "Output size should match");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test OffloadHeader with TASK_RESPONSE message type
 */
class OffloadHeaderResponseTestCase : public TestCase
{
  public:
    OffloadHeaderResponseTestCase()
        : TestCase("Test OffloadHeader with TASK_RESPONSE type")
    {
    }

  private:
    void DoRun() override
    {
        OffloadHeader original;
        original.SetMessageType(OffloadHeader::TASK_RESPONSE);
        original.SetTaskId(999);
        original.SetComputeDemand(1e12);
        original.SetInputSize(0);
        original.SetOutputSize(2048);

        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        OffloadHeader deserialized;
        packet->RemoveHeader(deserialized);

        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              OffloadHeader::TASK_RESPONSE,
                              "Message type should be TASK_RESPONSE");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetTaskId(), 999, "Task ID should match");
    }
};

} // namespace

TestCase*
CreateOffloadHeaderTestCase()
{
    return new OffloadHeaderTestCase;
}

TestCase*
CreateOffloadHeaderResponseTestCase()
{
    return new OffloadHeaderResponseTestCase;
}

} // namespace ns3
