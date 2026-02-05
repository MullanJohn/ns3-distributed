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

/**
 * @ingroup distributed-tests
 * @brief Test DagTask metadata serialization roundtrip
 *
 * Creates a diamond DAG with mixed control and data dependencies,
 * serializes metadata only, deserializes, and verifies structure.
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
        // Build diamond DAG: A -> B -> D
        //                    A -> C -> D
        // A->B is data dependency, A->C is control, B->D is data, C->D is control
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

        dag->AddDataDependency(a, b); // data
        dag->AddDependency(a, c);     // control only
        dag->AddDataDependency(b, d); // data
        dag->AddDependency(c, d);     // control only

        // Serialize metadata
        Ptr<Packet> packet = dag->SerializeMetadata();
        NS_TEST_ASSERT_MSG_GT(packet->GetSize(), 0, "Serialized packet should not be empty");

        // Deserialize metadata
        uint64_t consumedBytes = 0;
        Ptr<DagTask> restored = DagTask::DeserializeMetadata(
            packet,
            MakeCallback(&SimpleTask::DeserializeHeader),
            consumedBytes);

        NS_TEST_ASSERT_MSG_NE(restored, nullptr, "Deserialized DAG should not be null");
        NS_TEST_ASSERT_MSG_EQ(consumedBytes,
                              packet->GetSize(),
                              "Should consume entire packet");

        // Verify task count
        NS_TEST_ASSERT_MSG_EQ(restored->GetTaskCount(), 4, "Should have 4 tasks");

        // Verify task A metadata
        Ptr<Task> restoredA = restored->GetTask(a);
        NS_TEST_ASSERT_MSG_NE(restoredA, nullptr, "Task A should exist");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetTaskId(), 10, "Task A ID should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(restoredA->GetComputeDemand(), 1e9, 1.0, "Task A compute");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetInputSize(), 1000, "Task A input size");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetOutputSize(), 500, "Task A output size");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetRequiredAcceleratorType(),
                              "GPU",
                              "Task A accelerator type");

        // Verify task D metadata
        Ptr<Task> restoredD = restored->GetTask(d);
        NS_TEST_ASSERT_MSG_EQ(restoredD->GetTaskId(), 40, "Task D ID should match");
        NS_TEST_ASSERT_MSG_EQ_TOL(restoredD->GetComputeDemand(), 4e9, 1.0, "Task D compute");

        // Verify DAG structure via ready tasks
        // Only A should be ready (B,C depend on A; D depends on B,C)
        std::vector<uint32_t> ready = restored->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 1, "Only task A should be ready");
        NS_TEST_ASSERT_MSG_EQ(ready[0], a, "Ready task should be A");

        // Verify DAG is valid
        NS_TEST_ASSERT_MSG_EQ(restored->Validate(), true, "Restored DAG should be valid");

        // Verify sink tasks (only D)
        std::vector<uint32_t> sinks = restored->GetSinkTasks();
        NS_TEST_ASSERT_MSG_EQ(sinks.size(), 1, "Should have one sink task");
        NS_TEST_ASSERT_MSG_EQ(sinks[0], d, "Sink should be D");

        // Verify data dependency preserved: mark A completed, check data propagation to B
        restored->MarkCompleted(a);
        Ptr<Task> restoredB = restored->GetTask(b);
        NS_TEST_ASSERT_MSG_EQ(restoredB->GetInputSize(),
                              2000 + 500,
                              "B's input should include A's output via data dependency");
        // C should not get data propagation (control-only edge)
        Ptr<Task> restoredC = restored->GetTask(c);
        NS_TEST_ASSERT_MSG_EQ(restoredC->GetInputSize(),
                              3000,
                              "C's input should be unchanged (control dep only)");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test DagTask full data serialization roundtrip
 *
 * Creates a linear DAG with data dependencies, serializes full data,
 * deserializes, and verifies all fields including consumedBytes.
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
        // Linear DAG: A -> B -> C (all data dependencies)
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

        // Serialize full data
        Ptr<Packet> packet = dag->SerializeFullData();
        NS_TEST_ASSERT_MSG_GT(packet->GetSize(), 0, "Serialized packet should not be empty");

        // Deserialize full data
        uint64_t consumedBytes = 0;
        Ptr<DagTask> restored = DagTask::DeserializeFullData(
            packet,
            MakeCallback(&SimpleTask::Deserialize),
            consumedBytes);

        NS_TEST_ASSERT_MSG_NE(restored, nullptr, "Deserialized DAG should not be null");
        NS_TEST_ASSERT_MSG_EQ(consumedBytes,
                              packet->GetSize(),
                              "Should consume entire packet");

        // Verify task count
        NS_TEST_ASSERT_MSG_EQ(restored->GetTaskCount(), 3, "Should have 3 tasks");

        // Verify metadata preserved
        Ptr<Task> restoredA = restored->GetTask(a);
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetTaskId(), 100, "Task A ID");
        NS_TEST_ASSERT_MSG_EQ_TOL(restoredA->GetComputeDemand(), 5.5e9, 1.0, "Task A compute");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetInputSize(), 10000, "Task A input size");
        NS_TEST_ASSERT_MSG_EQ(restoredA->GetOutputSize(), 5000, "Task A output size");

        // Verify edge structure via ready tasks
        std::vector<uint32_t> ready = restored->GetReadyTasks();
        NS_TEST_ASSERT_MSG_EQ(ready.size(), 1, "Only task A should be ready");

        NS_TEST_ASSERT_MSG_EQ(restored->Validate(), true, "Restored DAG should be valid");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test DagTask deserialization failure cases
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
        Ptr<DagTask> result = DagTask::DeserializeMetadata(
            empty,
            MakeCallback(&SimpleTask::DeserializeHeader),
            consumedBytes);
        NS_TEST_ASSERT_MSG_EQ(result, nullptr, "Empty packet should return nullptr");
        NS_TEST_ASSERT_MSG_EQ(consumedBytes, 0, "Empty packet should consume 0 bytes");

        // Truncated packet (task count says 1 but no task data follows)
        uint8_t truncBuf[4] = {0, 0, 0, 1}; // taskCount = 1
        Ptr<Packet> truncated = Create<Packet>(truncBuf, 4);
        consumedBytes = 0;
        result = DagTask::DeserializeMetadata(
            truncated,
            MakeCallback(&SimpleTask::DeserializeHeader),
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
