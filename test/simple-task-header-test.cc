/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/packet.h"
#include "ns3/simple-task-header.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test SimpleTaskHeader serialization and deserialization
 */
class SimpleTaskHeaderTestCase : public TestCase
{
  public:
    SimpleTaskHeaderTestCase()
        : TestCase("Test SimpleTaskHeader serialization roundtrip")
    {
    }

  private:
    void DoRun() override
    {
        // Create and configure original header
        SimpleTaskHeader original;
        original.SetMessageType(SimpleTaskHeader::TASK_REQUEST);
        original.SetTaskId(12345);
        original.SetComputeDemand(5.5e9);
        original.SetInputSize(1024 * 1024);
        original.SetOutputSize(512 * 1024);
        original.SetDeadlineNs(1000000000); // 1 second deadline
        original.SetAcceleratorType("GPU");

        // Verify serialized size
        uint32_t expectedSize = sizeof(uint8_t) +  // messageType
                                sizeof(uint64_t) + // taskId
                                sizeof(uint64_t) + // computeDemand (as double)
                                sizeof(uint64_t) + // inputSize
                                sizeof(uint64_t) + // outputSize
                                sizeof(int64_t) +  // deadline
                                SimpleTaskHeader::ACCEL_TYPE_SIZE; // acceleratorType
        NS_TEST_ASSERT_MSG_EQ(original.GetSerializedSize(),
                              expectedSize,
                              "Serialized size should be 57 bytes");

        // Create packet with header
        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        // Deserialize header
        SimpleTaskHeader deserialized;
        packet->RemoveHeader(deserialized);

        // Verify all fields match
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              SimpleTaskHeader::TASK_REQUEST,
                              "Message type should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetTaskId(), 12345, "Task ID should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(deserialized.GetComputeDemand(),
                                  5.5e9,
                                  1e-6,
                                  "Compute demand should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetInputSize(), 1024 * 1024, "Input size should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetOutputSize(), 512 * 1024, "Output size should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.HasDeadline(), true, "Should have deadline");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetDeadlineNs(), 1000000000, "Deadline should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetAcceleratorType(), "GPU", "Accelerator type should match");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test SimpleTaskHeader with TASK_RESPONSE message type
 */
class SimpleTaskHeaderResponseTestCase : public TestCase
{
  public:
    SimpleTaskHeaderResponseTestCase()
        : TestCase("Test SimpleTaskHeader with TASK_RESPONSE type")
    {
    }

  private:
    void DoRun() override
    {
        SimpleTaskHeader original;
        original.SetMessageType(SimpleTaskHeader::TASK_RESPONSE);
        original.SetTaskId(999);
        original.SetComputeDemand(1e12);
        original.SetInputSize(0);
        original.SetOutputSize(2048);

        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        SimpleTaskHeader deserialized;
        packet->RemoveHeader(deserialized);

        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              SimpleTaskHeader::TASK_RESPONSE,
                              "Message type should be TASK_RESPONSE");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetTaskId(), 999, "Task ID should match");
    }
};

} // namespace

TestCase*
CreateSimpleTaskHeaderTestCase()
{
    return new SimpleTaskHeaderTestCase;
}

TestCase*
CreateSimpleTaskHeaderResponseTestCase()
{
    return new SimpleTaskHeaderResponseTestCase;
}

} // namespace ns3
