/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/callback.h"
#include "ns3/dag-task.h"
#include "ns3/nstime.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/// Strip the 1-byte type prefix DagTask prepends to each serialized task.
static Ptr<Task>
StripTypePrefixAndDeserialize(Ptr<Packet> packet,
                              uint64_t& consumedBytes,
                              Callback<Ptr<Task>, Ptr<Packet>, uint64_t&> deserializer)
{
    consumedBytes = 0;
    if (packet->GetSize() < 1)
    {
        return nullptr;
    }
    Ptr<Packet> subPacket = packet->CreateFragment(1, packet->GetSize() - 1);
    uint64_t subConsumed = 0;
    Ptr<Task> task = deserializer(subPacket, subConsumed);
    if (subConsumed > 0)
    {
        consumedBytes = 1 + subConsumed;
    }
    return task;
}

/// Deserialize a type-prefixed full task (header + payload).
static Ptr<Task>
DeserializeWithTypePrefix(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    return StripTypePrefixAndDeserialize(packet,
                                         consumedBytes,
                                         MakeCallback(&SimpleTask::Deserialize));
}

/// Deserialize a type-prefixed metadata-only task (header only).
static Ptr<Task>
DeserializeHeaderWithTypePrefix(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    return StripTypePrefixAndDeserialize(packet,
                                         consumedBytes,
                                         MakeCallback(&SimpleTask::DeserializeHeader));
}

/**
 * @ingroup distributed-tests
 * @brief Test DagTask metadata serialize/deserialize round-trip preserves structure and data edges.
 */
class DagTaskSerializeMetadataTestCase : public TestCase
{
  public:
    DagTaskSerializeMetadataTestCase()
        : TestCase("Test DagTask metadata serialization roundtrip")
    {
    }

  private:
    void DoRun() override
    {
        // Diamond A->B,C; B,C->D. A->B and B->D are data edges; A->C and C->D are control.
        Ptr<DagTask> dag = CreateObject<DagTask>();

        Ptr<SimpleTask> taskA = CreateObject<SimpleTask>();
        taskA->SetTaskId(10);
        taskA->SetComputeDemand(1e9);
        taskA->SetInputSize(1000);
        taskA->SetOutputSize(500);
        taskA->SetDeadline(MilliSeconds(100));
        taskA->SetRequiredAcceleratorType("GPU");

        Ptr<SimpleTask> taskB = CreateObject<SimpleTask>();
        taskB->SetTaskId(20);
        taskB->SetComputeDemand(2e9);
        taskB->SetInputSize(2000);
        taskB->SetOutputSize(1000);

        Ptr<SimpleTask> taskC = CreateObject<SimpleTask>();
        taskC->SetTaskId(30);
        taskC->SetComputeDemand(3e9);
        taskC->SetInputSize(3000);
        taskC->SetOutputSize(1500);

        Ptr<SimpleTask> taskD = CreateObject<SimpleTask>();
        taskD->SetTaskId(40);
        taskD->SetComputeDemand(4e9);
        taskD->SetInputSize(4000);
        taskD->SetOutputSize(2000);

        uint32_t a = dag->AddTask(taskA);
        uint32_t b = dag->AddTask(taskB);
        uint32_t c = dag->AddTask(taskC);
        uint32_t d = dag->AddTask(taskD);

        dag->AddDataDependency(a, b);
        dag->AddDependency(a, c);
        dag->AddDataDependency(b, d);
        dag->AddDependency(c, d);

        Ptr<Packet> packet = dag->SerializeMetadata();
        NS_TEST_ASSERT_MSG_GT(packet->GetSize(), 0, "Serialized packet should not be empty");

        uint64_t consumedBytes = 0;
        Ptr<DagTask> restored =
            DagTask::DeserializeMetadata(packet,
                                         MakeCallback(&DeserializeHeaderWithTypePrefix),
                                         consumedBytes);

        NS_TEST_ASSERT_MSG_NE(restored, nullptr, "Deserialized DAG should not be null");
        NS_TEST_ASSERT_MSG_EQ(consumedBytes, packet->GetSize(), "Should consume entire packet");

        NS_TEST_ASSERT_MSG_EQ(restored->GetTaskCount(), 4, "Should have 4 tasks");

        Ptr<Task> restoredA = restored->GetTask(a);
        NS_TEST_ASSERT_MSG_NE(restoredA, nullptr, "Task A should exist");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetTaskId(), 10, "Task A ID should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(restoredA->GetComputeDemand(), 1e9, 1e-6, "Task A compute");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetInputSize(), 1000, "Task A input size");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetOutputSize(), 500, "Task A output size");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetRequiredAcceleratorType(),
                              "GPU",
                              "Task A accelerator type");

        Ptr<Task> restoredD = restored->GetTask(d);
        NS_TEST_ASSERT_MSG_EQ(restoredD->GetTaskId(), 40, "Task D ID should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(restoredD->GetComputeDemand(), 4e9, 1e-6, "Task D compute");

        std::vector<uint32_t> ready = restored->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 1, "Only task A should be ready");
        NS_TEST_ASSERT_MSG_EQ(ready[0], a, "Ready task should be A");

        NS_TEST_ASSERT_MSG_EQ(restored->Validate(), true, "Restored DAG should be valid");

        std::vector<uint32_t> sinks = restored->GetSinkTasks();
        NS_TEST_ASSERT_MSG_EQ(sinks.size(), 1, "Should have one sink task");
        NS_TEST_ASSERT_MSG_EQ(sinks[0], d, "Sink should be D");

        restored->MarkCompleted(a);
        Ptr<Task> restoredB = restored->GetTask(b);
        NS_TEST_ASSERT_MSG_EQ(restoredB->GetInputSize(),
                              2000 + 500,
                              "B's input should include A's output via data dependency");
        Ptr<Task> restoredC = restored->GetTask(c);
        NS_TEST_ASSERT_MSG_EQ(restoredC->GetInputSize(),
                              3000,
                              "C's input should be unchanged (control-only edge)");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test DagTask full-data serialize/deserialize round-trip on a linear chain.
 */
class DagTaskSerializeFullDataTestCase : public TestCase
{
  public:
    DagTaskSerializeFullDataTestCase()
        : TestCase("Test DagTask full data serialization roundtrip")
    {
    }

  private:
    void DoRun() override
    {
        // Linear chain A -> B -> C with data dependencies on every edge.
        Ptr<DagTask> dag = CreateObject<DagTask>();

        Ptr<SimpleTask> taskA = CreateObject<SimpleTask>();
        taskA->SetTaskId(100);
        taskA->SetComputeDemand(5.5e9);
        taskA->SetInputSize(10000);
        taskA->SetOutputSize(5000);

        Ptr<SimpleTask> taskB = CreateObject<SimpleTask>();
        taskB->SetTaskId(200);
        taskB->SetComputeDemand(7.5e9);
        taskB->SetInputSize(20000);
        taskB->SetOutputSize(10000);

        Ptr<SimpleTask> taskC = CreateObject<SimpleTask>();
        taskC->SetTaskId(300);
        taskC->SetComputeDemand(3.0e9);
        taskC->SetInputSize(15000);
        taskC->SetOutputSize(8000);

        uint32_t a = dag->AddTask(taskA);
        uint32_t b = dag->AddTask(taskB);
        uint32_t c = dag->AddTask(taskC);

        dag->AddDataDependency(a, b);
        dag->AddDataDependency(b, c);

        Ptr<Packet> packet = dag->SerializeFullData();
        NS_TEST_ASSERT_MSG_GT(packet->GetSize(), 0, "Serialized packet should not be empty");

        uint64_t consumedBytes = 0;
        Ptr<DagTask> restored =
            DagTask::DeserializeFullData(packet,
                                         MakeCallback(&DeserializeWithTypePrefix),
                                         consumedBytes);

        NS_TEST_ASSERT_MSG_NE(restored, nullptr, "Deserialized DAG should not be null");
        NS_TEST_ASSERT_MSG_EQ(consumedBytes, packet->GetSize(), "Should consume entire packet");

        NS_TEST_ASSERT_MSG_EQ(restored->GetTaskCount(), 3, "Should have 3 tasks");

        Ptr<Task> restoredA = restored->GetTask(a);
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetTaskId(), 100, "Task A ID");
        NS_TEST_ASSERT_MSG_EQ_TOL(restoredA->GetComputeDemand(), 5.5e9, 1e-6, "Task A compute");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetInputSize(), 10000, "Task A input size");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetOutputSize(), 5000, "Task A output size");

        std::vector<uint32_t> ready = restored->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 1, "Only task A should be ready");

        NS_TEST_ASSERT_MSG_EQ(restored->Validate(), true, "Restored DAG should be valid");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test DagTask deserialize returns nullptr on empty or truncated packets.
 */
class DagTaskDeserializeFailureTestCase : public TestCase
{
  public:
    DagTaskDeserializeFailureTestCase()
        : TestCase("Test DagTask deserialization failure handling")
    {
    }

  private:
    void DoRun() override
    {
        uint64_t consumedBytes = 0;

        // Empty packet
        Ptr<Packet> empty = Create<Packet>();
        Ptr<DagTask> result =
            DagTask::DeserializeMetadata(empty,
                                         MakeCallback(&DeserializeHeaderWithTypePrefix),
                                         consumedBytes);
        NS_TEST_ASSERT_MSG_EQ(result, nullptr, "Empty packet should return nullptr");
        NS_TEST_ASSERT_MSG_EQ(consumedBytes, 0, "Empty packet should consume 0 bytes");

        // Truncated packet (task count says 1 but no task data follows)
        uint8_t truncBuf[4] = {0, 0, 0, 1}; // taskCount = 1
        Ptr<Packet> truncated = Create<Packet>(truncBuf, 4);
        consumedBytes = 0;
        result = DagTask::DeserializeMetadata(truncated,
                                              MakeCallback(&DeserializeHeaderWithTypePrefix),
                                              consumedBytes);
        NS_TEST_ASSERT_MSG_EQ(result, nullptr, "Truncated packet should return nullptr");
        NS_TEST_ASSERT_MSG_EQ(consumedBytes, 0, "Truncated packet should consume 0 bytes");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateDagTaskSerializeMetadataTestCase()
{
    return new DagTaskSerializeMetadataTestCase;
}

TestCase*
CreateDagTaskSerializeFullDataTestCase()
{
    return new DagTaskSerializeFullDataTestCase;
}

TestCase*
CreateDagTaskDeserializeFailureTestCase()
{
    return new DagTaskDeserializeFailureTestCase;
}

} // namespace ns3
