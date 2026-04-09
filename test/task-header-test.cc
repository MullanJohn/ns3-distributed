/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/packet.h"
#include "ns3/task-header.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test TaskHeader serialization round-trip.
 */
class TaskHeaderSerializationTestCase : public TestCase
{
  public:
    TaskHeaderSerializationTestCase()
        : TestCase("Test TaskHeader serialization roundtrip")
    {
    }

  private:
    void DoRun() override
    {
        TaskHeader original;
        original.SetMessageType(TaskHeader::TASK_REQUEST);
        original.SetTaskId(12345);
        original.SetComputeDemand(5.5e9);
        original.SetInputSize(1024 * 1024);
        original.SetOutputSize(512 * 1024);
        original.SetDeadlineNs(1000000000);
        original.SetPriority(5);
        original.SetAcceleratorType("GPU");

        NS_TEST_ASSERT_MSG_EQ(original.GetSerializedSize(),
                              TaskHeader::SERIALIZED_SIZE,
                              "Serialized size should be 61 bytes");

        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        TaskHeader deserialized;
        packet->RemoveHeader(deserialized);

        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              TaskHeader::TASK_REQUEST,
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
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetPriority(), 5, "Priority should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetAcceleratorType(),
                              "GPU",
                              "Accelerator type should match");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test TaskHeader payload size calculation.
 */
class TaskHeaderPayloadSizeTestCase : public TestCase
{
  public:
    TaskHeaderPayloadSizeTestCase()
        : TestCase("Test TaskHeader payload size calculation")
    {
    }

  private:
    void DoRun() override
    {
        TaskHeader header;
        header.SetInputSize(1000);
        header.SetOutputSize(500);

        NS_TEST_ASSERT_MSG_EQ(header.GetRequestPayloadSize(),
                              1000,
                              "Request payload size should equal input size");
        NS_TEST_ASSERT_MSG_EQ(header.GetResponsePayloadSize(), 500, "Response payload size");

        header.SetInputSize(10);
        NS_TEST_ASSERT_MSG_EQ(header.GetRequestPayloadSize(),
                              10,
                              "Request payload size should equal input size");
    }
};

} // namespace

TestCase*
CreateTaskHeaderSerializationTestCase()
{
    return new TaskHeaderSerializationTestCase;
}

TestCase*
CreateTaskHeaderPayloadSizeTestCase()
{
    return new TaskHeaderPayloadSizeTestCase;
}

} // namespace ns3
