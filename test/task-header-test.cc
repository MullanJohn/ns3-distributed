/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/simple-task-header.h"
#include "ns3/packet.h"
#include "ns3/task-header.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test TaskHeader interface through SimpleTaskHeader
 */
class TaskHeaderInterfaceTestCase : public TestCase
{
  public:
    TaskHeaderInterfaceTestCase()
        : TestCase("Test TaskHeader interface through SimpleTaskHeader")
    {
    }

  private:
    void DoRun() override
    {
        // Create SimpleTaskHeader and use via TaskHeader interface
        SimpleTaskHeader concrete;
        concrete.SetMessageType(TaskHeader::TASK_REQUEST);
        concrete.SetTaskId(42);
        concrete.SetInputSize(1000);
        concrete.SetOutputSize(500);

        // Access via base class pointer
        TaskHeader* base = &concrete;

        NS_TEST_ASSERT_MSG_EQ(base->GetMessageType(),
                              TaskHeader::TASK_REQUEST,
                              "Message type via interface");
        NS_TEST_ASSERT_MSG_EQ(base->GetTaskId(), 42, "Task ID via interface");
        NS_TEST_ASSERT_MSG_EQ(base->IsRequest(), true, "IsRequest should be true");
        NS_TEST_ASSERT_MSG_EQ(base->IsResponse(), false, "IsResponse should be false");

        // Test response type
        base->SetMessageType(TaskHeader::TASK_RESPONSE);
        NS_TEST_ASSERT_MSG_EQ(base->IsRequest(), false, "IsRequest should be false");
        NS_TEST_ASSERT_MSG_EQ(base->IsResponse(), true, "IsResponse should be true");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test TaskHeader polymorphism with serialization
 */
class TaskHeaderPolymorphismTestCase : public TestCase
{
  public:
    TaskHeaderPolymorphismTestCase()
        : TestCase("Test TaskHeader polymorphism with serialization")
    {
    }

  private:
    void DoRun() override
    {
        // Create concrete header
        SimpleTaskHeader original;
        original.SetMessageType(TaskHeader::TASK_REQUEST);
        original.SetTaskId(12345);
        original.SetComputeDemand(1e9);
        original.SetInputSize(1024);
        original.SetOutputSize(512);

        // Serialize through base class reference
        const TaskHeader& baseRef = original;
        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(baseRef);

        // Verify serialization worked (packet has correct size)
        NS_TEST_ASSERT_MSG_EQ(packet->GetSize(),
                              SimpleTaskHeader::SERIALIZED_SIZE,
                              "Packet should have header size");

        // Deserialize
        SimpleTaskHeader deserialized;
        packet->RemoveHeader(deserialized);

        // Verify through interface methods
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetTaskId(), 12345, "Task ID should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.IsRequest(), true, "Should be request");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test TaskHeader payload size calculation
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
        SimpleTaskHeader header;
        header.SetInputSize(1000);
        header.SetOutputSize(500);

        // Access via base class reference
        const TaskHeader& base = header;

        // Request payload = inputSize - headerSize (or 0 if inputSize <= headerSize)
        uint64_t expectedRequestPayload = 1000 - SimpleTaskHeader::SERIALIZED_SIZE;
        NS_TEST_ASSERT_MSG_EQ(base.GetRequestPayloadSize(),
                              expectedRequestPayload,
                              "Request payload size");

        // Response payload = outputSize
        NS_TEST_ASSERT_MSG_EQ(base.GetResponsePayloadSize(), 500, "Response payload size");

        // Test edge case: inputSize <= headerSize
        header.SetInputSize(10);
        NS_TEST_ASSERT_MSG_EQ(base.GetRequestPayloadSize(),
                              0,
                              "Request payload should be 0 when input smaller than header");
    }
};

} // namespace

TestCase*
CreateTaskHeaderInterfaceTestCase()
{
    return new TaskHeaderInterfaceTestCase;
}

TestCase*
CreateTaskHeaderPolymorphismTestCase()
{
    return new TaskHeaderPolymorphismTestCase;
}

TestCase*
CreateTaskHeaderPayloadSizeTestCase()
{
    return new TaskHeaderPayloadSizeTestCase;
}

} // namespace ns3
