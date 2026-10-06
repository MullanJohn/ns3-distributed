/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/backend-server.h"
#include "ns3/cluster.h"
#include "ns3/dag-task.h"
#include "ns3/double.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/offload-client.h"
#include "ns3/orchestrator.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/pointer.h"
#include "ns3/round-robin-scheduler.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace
{

/// Strip the 1-byte type prefix DagTask prepends to each serialized task.
Ptr<Task>
DeserializeWithTypePrefix(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    consumedBytes = 0;
    if (packet->GetSize() < 1)
    {
        return nullptr;
    }
    Ptr<Packet> subPacket = packet->CreateFragment(1, packet->GetSize() - 1);
    uint64_t subConsumed = 0;
    Ptr<Task> task = SimpleTask::Deserialize(subPacket, subConsumed);
    if (subConsumed > 0)
    {
        consumedBytes = 1 + subConsumed;
    }
    return task;
}

/**
 * @ingroup continuum-tests
 * @brief End-to-end check of the WORKLOAD_RESPONSE completion path.
 *
 * Runs real workloads through admission, dispatch, execution and response over
 * the new single-message workload response, verifying that:
 * - the client's WorkloadProcessed trace fires exactly once per workload,
 * - the returned DAG carries real backend and compute times off the wire,
 * - the pending-workload entry is released on arrival, so later submissions are
 *   not dropped by the default DropWhilePendingSubmissionPolicy.
 *
 * @note OffloadClient currently generates single-task DAGs, so this exercises
 * the single-sink case end to end. Multi-sink wire behaviour is covered by
 * MultiSinkDagSerializationTestCase until client-side DAG generation is added.
 */
class WorkloadResponseEndToEndTestCase : public TestCase
{
  public:
    WorkloadResponseEndToEndTestCase()
        : TestCase("Workload response fires one completion event and releases the pending slot"),
          m_workloadsProcessed(0),
          m_tasksSeen(0),
          m_tasksWithBackendTime(0),
          m_tasksWithComputeTime(0)
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(4);
        Ptr<Node> clientNode = nodes.Get(0);
        Ptr<Node> orchNode = nodes.Get(1);
        Ptr<Node> serverNode0 = nodes.Get(2);
        Ptr<Node> serverNode1 = nodes.Get(3);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devClientOrch = p2p.Install(clientNode, orchNode);
        NetDeviceContainer devOrchServer0 = p2p.Install(orchNode, serverNode0);
        NetDeviceContainer devOrchServer1 = p2p.Install(orchNode, serverNode1);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifClientOrch = ipv4.Assign(devClientOrch);

        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer ifOrchServer0 = ipv4.Assign(devOrchServer0);

        ipv4.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer ifOrchServer1 = ipv4.Assign(devOrchServer1);

        uint16_t serverPort = 9000;
        InstallBackend(serverNode0, serverPort);
        InstallBackend(serverNode1, serverPort);

        Cluster cluster;
        cluster.AddBackend(serverNode0, InetSocketAddress(ifOrchServer0.GetAddress(1), serverPort));
        cluster.AddBackend(serverNode1, InetSocketAddress(ifOrchServer1.GetAddress(1), serverPort));

        // Round-robin so the two sink tasks land on different backends.
        Ptr<RoundRobinScheduler> scheduler = CreateObject<RoundRobinScheduler>();

        uint16_t orchPort = 8080;
        Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
        orchestrator->SetAttribute("Port", UintegerValue(orchPort));
        orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
        orchestrator->SetCluster(cluster);
        orchNode->AddApplication(orchestrator);
        orchestrator->SetStartTime(Seconds(0.0));
        orchestrator->SetStopTime(Seconds(10.0));

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        // 1 Hz generation, running to 2.5s: enough ticks for a second submission,
        // which only succeeds if the first workload released its pending slot.
        m_client = CreateObject<OffloadClient>();
        m_client->SetAttribute("Remote",
                               AddressValue(
                                   InetSocketAddress(ifClientOrch.GetAddress(1), orchPort)));
        m_client->SetAttribute("InterArrival",
                               StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));

        Ptr<ConstantRandomVariable> inputSize = CreateObject<ConstantRandomVariable>();
        inputSize->SetAttribute("Constant", DoubleValue(1000));
        Ptr<ConstantRandomVariable> compute = CreateObject<ConstantRandomVariable>();
        compute->SetAttribute("Constant", DoubleValue(1e9));
        Ptr<ConstantRandomVariable> output = CreateObject<ConstantRandomVariable>();
        output->SetAttribute("Constant", DoubleValue(100));

        Ptr<SimpleTask> templateTask = CreateObject<SimpleTask>();
        templateTask->SetAttribute("InputSizeDistribution", PointerValue(inputSize));
        templateTask->SetAttribute("ComputeDemandDistribution", PointerValue(compute));
        templateTask->SetAttribute("OutputSizeDistribution", PointerValue(output));
        Ptr<DagTask> dagTemplate = CreateObject<DagTask>();
        dagTemplate->AddTask(templateTask);
        m_client->SetAttribute("DagTemplate", PointerValue(dagTemplate));

        m_client->TraceConnectWithoutContext(
            "WorkloadProcessed",
            MakeCallback(&WorkloadResponseEndToEndTestCase::OnWorkloadProcessed, this));

        clientNode->AddApplication(m_client);
        m_client->SetStartTime(Seconds(0.1));
        m_client->SetStopTime(Seconds(2.5));

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        uint64_t tasksDropped = m_client->GetTasksDropped();
        uint64_t tasksSent = m_client->GetTasksSent();

        Simulator::Destroy();

        // More than one submission proves the pending slot was released: with the
        // default DropWhilePendingSubmissionPolicy a leaked entry would block
        // every later tick.
        NS_TEST_ASSERT_MSG_GT(tasksSent, 1, "Client should have submitted multiple workloads");
        NS_TEST_ASSERT_MSG_EQ(m_workloadsProcessed,
                              tasksSent,
                              "WorkloadProcessed must fire exactly once per submitted workload");
        NS_TEST_ASSERT_MSG_EQ(tasksDropped,
                              0,
                              "No submission should be dropped: the pending entry must be "
                              "released when the workload response arrives");
        NS_TEST_ASSERT_MSG_EQ(m_tasksWithBackendTime,
                              m_tasksSeen,
                              "Every returned task should carry a backend time from the wire");
        NS_TEST_ASSERT_MSG_EQ(m_tasksWithComputeTime,
                              m_tasksSeen,
                              "Every returned task should carry a compute time from the wire");
    }

    void InstallBackend(Ptr<Node> node, uint16_t port)
    {
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
        node->AggregateObject(gpu);

        Ptr<BackendServer> server = CreateObject<BackendServer>();
        server->SetAttribute("Port", UintegerValue(port));
        node->AddApplication(server);
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(10.0));
    }

    void OnWorkloadProcessed(Ptr<const DagTask> dag, Time latency)
    {
        m_workloadsProcessed++;

        for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
        {
            Ptr<Task> task = dag->GetTask(i);
            if (!task)
            {
                continue;
            }
            m_tasksSeen++;
            if (task->GetBackendTime().IsStrictlyPositive())
            {
                m_tasksWithBackendTime++;
            }
            if (task->GetComputeTime().IsStrictlyPositive())
            {
                m_tasksWithComputeTime++;
            }
        }
    }

    Ptr<OffloadClient> m_client;
    uint32_t m_workloadsProcessed;
    uint32_t m_tasksSeen;
    uint32_t m_tasksWithBackendTime;
    uint32_t m_tasksWithComputeTime;
};

/**
 * @ingroup continuum-tests
 * @brief Test that a completed multi-sink DAG survives a response-direction round trip.
 *
 * Exercises DagTask::SerializeFullData(isResponse=true) / DeserializeFullData
 * directly on a diamond DAG carrying recorded execution times, which is the
 * exact payload the orchestrator sends back in a WORKLOAD_RESPONSE.
 */
class MultiSinkDagSerializationTestCase : public TestCase
{
  public:
    MultiSinkDagSerializationTestCase()
        : TestCase("Multi-sink DAG response serialization roundtrip")
    {
    }

  private:
    Ptr<SimpleTask> MakeTask(uint64_t id, uint64_t outputSize, Time backendTime, Time computeTime)
    {
        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetTaskId(id);
        task->SetComputeDemand(1e9);
        task->SetInputSize(500);
        task->SetOutputSize(outputSize);
        task->SetBackendTime(backendTime);
        task->SetComputeTime(computeTime);
        return task;
    }

    void DoRun() override
    {
        // Diamond: task0 -> task1, task0 -> task2. Both leaves are sinks.
        Ptr<DagTask> dag = CreateObject<DagTask>();
        uint32_t idx0 = dag->AddTask(MakeTask(100, 200, MilliSeconds(7), MilliSeconds(5)));
        uint32_t idx1 = dag->AddTask(MakeTask(101, 300, MilliSeconds(9), MilliSeconds(6)));
        uint32_t idx2 = dag->AddTask(MakeTask(102, 400, MilliSeconds(11), MilliSeconds(8)));
        dag->AddDataDependency(idx0, idx1);
        dag->AddDataDependency(idx0, idx2);

        NS_TEST_ASSERT_MSG_EQ(dag->GetSinkTasks().size(), 2, "Diamond should have two sinks");

        Ptr<Packet> payload = dag->SerializeFullData(true);

        uint64_t consumedBytes = 0;
        Ptr<DagTask> restored =
            DagTask::DeserializeFullData(payload,
                                         MakeCallback(&DeserializeWithTypePrefix),
                                         consumedBytes);

        NS_TEST_ASSERT_MSG_NE(restored, nullptr, "Response DAG should deserialize");
        NS_TEST_ASSERT_MSG_EQ(restored->GetTaskCount(), 3, "All three tasks should survive");
        NS_TEST_ASSERT_MSG_EQ(restored->GetSinkTasks().size(),
                              2,
                              "Graph edges should survive, preserving both sinks");

        // Every task's recorded execution times must come back intact -- this is
        // what makes the client's DAG the authoritative record of the workload.
        NS_TEST_ASSERT_MSG_EQ(restored->GetTask(0)->GetBackendTime(),
                              MilliSeconds(7),
                              "Non-sink task backend time should survive");
        NS_TEST_ASSERT_MSG_EQ(restored->GetTask(0)->GetComputeTime(),
                              MilliSeconds(5),
                              "Non-sink task compute time should survive");
        NS_TEST_ASSERT_MSG_EQ(restored->GetTask(2)->GetBackendTime(),
                              MilliSeconds(11),
                              "Sink task backend time should survive");
        NS_TEST_ASSERT_MSG_EQ(restored->GetTask(2)->GetComputeTime(),
                              MilliSeconds(8),
                              "Sink task compute time should survive");
        NS_TEST_ASSERT_MSG_EQ(restored->GetTask(2)->GetOutputSize(),
                              400,
                              "Response payload sizing should follow output size");
    }
};

} // namespace

TestCase*
CreateWorkloadResponseEndToEndTestCase()
{
    return new WorkloadResponseEndToEndTestCase;
}

TestCase*
CreateMultiSinkDagSerializationTestCase()
{
    return new MultiSinkDagSerializationTestCase;
}

} // namespace ns3
