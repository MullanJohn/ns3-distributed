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
#include "ns3/boolean.h"
#include "ns3/least-loaded-scheduler.h"
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

/**
 * @ingroup continuum-tests
 * @brief Test Orchestrator admission, dispatch, completion and response on a single task.
 */
class SingleTaskEndToEndTestCase : public TestCase
{
  public:
    SingleTaskEndToEndTestCase()
        : TestCase("Orchestrator single task end-to-end"),
          m_workloadsAdmitted(0),
          m_workloadsCompleted(0),
          m_tasksDispatched(0),
          m_tasksCompleted(0)
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(3);
        Ptr<Node> clientNode = nodes.Get(0);
        Ptr<Node> orchNode = nodes.Get(1);
        Ptr<Node> serverNode = nodes.Get(2);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devClientOrch = p2p.Install(clientNode, orchNode);
        NetDeviceContainer devOrchServer = p2p.Install(orchNode, serverNode);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifClientOrch = ipv4.Assign(devClientOrch);

        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer ifOrchServer = ipv4.Assign(devOrchServer);

        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
        serverNode->AggregateObject(gpu);

        uint16_t serverPort = 9000;
        Ptr<BackendServer> server = CreateObject<BackendServer>();
        server->SetAttribute("Port", UintegerValue(serverPort));
        serverNode->AddApplication(server);
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(10.0));

        Cluster cluster;
        cluster.AddBackend(serverNode, InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

        Ptr<RoundRobinScheduler> scheduler = CreateObject<RoundRobinScheduler>();

        uint16_t orchPort = 8080;
        Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
        orchestrator->SetAttribute("Port", UintegerValue(orchPort));
        orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
        orchestrator->SetCluster(cluster);
        orchNode->AddApplication(orchestrator);
        orchestrator->SetStartTime(Seconds(0.0));
        orchestrator->SetStopTime(Seconds(10.0));

        orchestrator->TraceConnectWithoutContext(
            "WorkloadAdmitted",
            MakeCallback(&SingleTaskEndToEndTestCase::OnWorkloadAdmitted, this));
        orchestrator->TraceConnectWithoutContext(
            "WorkloadCompleted",
            MakeCallback(&SingleTaskEndToEndTestCase::OnWorkloadCompleted, this));
        orchestrator->TraceConnectWithoutContext(
            "TaskDispatched",
            MakeCallback(&SingleTaskEndToEndTestCase::OnTaskDispatched, this));
        orchestrator->TraceConnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&SingleTaskEndToEndTestCase::OnTaskCompleted, this));

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        // 1 Hz, stop at 1.5s: the client emits exactly one task.
        Ptr<OffloadClient> client = CreateObject<OffloadClient>();
        client->SetAttribute("Remote",
                             AddressValue(InetSocketAddress(ifClientOrch.GetAddress(1), orchPort)));
        client->SetAttribute("InterArrival",
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
        client->SetAttribute("DagTemplate", PointerValue(dagTemplate));

        clientNode->AddApplication(client);
        client->SetStartTime(Seconds(0.1));
        client->SetStopTime(Seconds(1.5));

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_workloadsAdmitted, 1, "WorkloadAdmitted trace should fire once");
        NS_TEST_ASSERT_MSG_EQ(m_workloadsCompleted, 1, "WorkloadCompleted trace should fire once");
        NS_TEST_ASSERT_MSG_EQ(m_tasksDispatched, 1, "TaskDispatched trace should fire once");
        NS_TEST_ASSERT_MSG_EQ(m_tasksCompleted, 1, "TaskCompleted trace should fire once");

        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsAdmitted(),
                              1,
                              "Orchestrator should have admitted 1 workload");
        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsCompleted(),
                              1,
                              "Orchestrator should have completed 1 workload");

        NS_TEST_ASSERT_MSG_EQ(client->GetResponsesReceived(),
                              1,
                              "Client should have received 1 response");
    }

    void OnWorkloadAdmitted(uint64_t workloadId, uint32_t taskCount)
    {
        m_workloadsAdmitted++;
    }

    void OnWorkloadCompleted(uint64_t workloadId)
    {
        m_workloadsCompleted++;
    }

    void OnTaskDispatched(uint64_t workloadId, uint64_t taskId, uint32_t backendIdx)
    {
        m_tasksDispatched++;
    }

    void OnTaskCompleted(uint64_t workloadId, uint64_t taskId, uint32_t backendIdx)
    {
        m_tasksCompleted++;
    }

    uint32_t m_workloadsAdmitted;
    uint32_t m_workloadsCompleted;
    uint32_t m_tasksDispatched;
    uint32_t m_tasksCompleted;
};

/**
 * @ingroup continuum-tests
 * @brief Test Orchestrator dispatches across multiple backends via LeastLoadedScheduler.
 */
class MultiBackendTestCase : public TestCase
{
  public:
    MultiBackendTestCase()
        : TestCase("Orchestrator dispatches to multiple backends")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(5);
        Ptr<Node> clientNode0 = nodes.Get(0);
        Ptr<Node> clientNode1 = nodes.Get(1);
        Ptr<Node> orchNode = nodes.Get(2);
        Ptr<Node> serverNode0 = nodes.Get(3);
        Ptr<Node> serverNode1 = nodes.Get(4);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devClient0Orch = p2p.Install(clientNode0, orchNode);
        NetDeviceContainer devClient1Orch = p2p.Install(clientNode1, orchNode);
        NetDeviceContainer devOrchServer0 = p2p.Install(orchNode, serverNode0);
        NetDeviceContainer devOrchServer1 = p2p.Install(orchNode, serverNode1);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifClient0Orch = ipv4.Assign(devClient0Orch);

        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer ifClient1Orch = ipv4.Assign(devClient1Orch);

        ipv4.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer ifOrchServer0 = ipv4.Assign(devOrchServer0);

        ipv4.SetBase("10.1.4.0", "255.255.255.0");
        Ipv4InterfaceContainer ifOrchServer1 = ipv4.Assign(devOrchServer1);

        {
            Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
            Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();
            Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
            gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
            gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
            gpu->SetAttribute("TransferBandwidth", DoubleValue(1e11));
            gpu->SetAttribute("ProcessingModel", PointerValue(model));
            gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
            serverNode0->AggregateObject(gpu);
        }

        {
            Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
            Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();
            Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
            gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
            gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
            gpu->SetAttribute("TransferBandwidth", DoubleValue(1e11));
            gpu->SetAttribute("ProcessingModel", PointerValue(model));
            gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
            serverNode1->AggregateObject(gpu);
        }

        uint16_t serverPort = 9000;
        Ptr<BackendServer> server0 = CreateObject<BackendServer>();
        server0->SetAttribute("Port", UintegerValue(serverPort));
        serverNode0->AddApplication(server0);
        server0->SetStartTime(Seconds(0.0));
        server0->SetStopTime(Seconds(10.0));

        Ptr<BackendServer> server1 = CreateObject<BackendServer>();
        server1->SetAttribute("Port", UintegerValue(serverPort));
        serverNode1->AddApplication(server1);
        server1->SetStartTime(Seconds(0.0));
        server1->SetStopTime(Seconds(10.0));

        Cluster cluster;
        cluster.AddBackend(serverNode0, InetSocketAddress(ifOrchServer0.GetAddress(1), serverPort));
        cluster.AddBackend(serverNode1, InetSocketAddress(ifOrchServer1.GetAddress(1), serverPort));

        Ptr<LeastLoadedScheduler> scheduler = CreateObject<LeastLoadedScheduler>();

        uint16_t orchPort = 8080;
        Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
        orchestrator->SetAttribute("Port", UintegerValue(orchPort));
        orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
        orchestrator->SetCluster(cluster);
        orchNode->AddApplication(orchestrator);
        orchestrator->SetStartTime(Seconds(0.0));
        orchestrator->SetStopTime(Seconds(10.0));

        orchestrator->TraceConnectWithoutContext(
            "TaskDispatched",
            MakeCallback(&MultiBackendTestCase::OnTaskDispatched, this));

        Ptr<OffloadClient> client0 = CreateObject<OffloadClient>();
        client0->SetAttribute(
            "Remote",
            AddressValue(InetSocketAddress(ifClient0Orch.GetAddress(1), orchPort)));
        client0->SetAttribute("InterArrival",
                              StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));

        Ptr<ConstantRandomVariable> inputSize0 = CreateObject<ConstantRandomVariable>();
        inputSize0->SetAttribute("Constant", DoubleValue(1000));
        Ptr<ConstantRandomVariable> compute0 = CreateObject<ConstantRandomVariable>();
        compute0->SetAttribute("Constant", DoubleValue(4e11));
        Ptr<ConstantRandomVariable> output0 = CreateObject<ConstantRandomVariable>();
        output0->SetAttribute("Constant", DoubleValue(100));

        Ptr<SimpleTask> templateTask0 = CreateObject<SimpleTask>();
        templateTask0->SetAttribute("InputSizeDistribution", PointerValue(inputSize0));
        templateTask0->SetAttribute("ComputeDemandDistribution", PointerValue(compute0));
        templateTask0->SetAttribute("OutputSizeDistribution", PointerValue(output0));
        Ptr<DagTask> dagTemplate0 = CreateObject<DagTask>();
        dagTemplate0->AddTask(templateTask0);
        client0->SetAttribute("DagTemplate", PointerValue(dagTemplate0));

        Ptr<OffloadClient> client1 = CreateObject<OffloadClient>();
        client1->SetAttribute(
            "Remote",
            AddressValue(InetSocketAddress(ifClient1Orch.GetAddress(1), orchPort)));
        client1->SetAttribute("InterArrival",
                              StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));

        Ptr<ConstantRandomVariable> inputSize1 = CreateObject<ConstantRandomVariable>();
        inputSize1->SetAttribute("Constant", DoubleValue(1000));
        Ptr<ConstantRandomVariable> compute1 = CreateObject<ConstantRandomVariable>();
        compute1->SetAttribute("Constant", DoubleValue(4e11));
        Ptr<ConstantRandomVariable> output1 = CreateObject<ConstantRandomVariable>();
        output1->SetAttribute("Constant", DoubleValue(100));

        Ptr<SimpleTask> templateTask1 = CreateObject<SimpleTask>();
        templateTask1->SetAttribute("InputSizeDistribution", PointerValue(inputSize1));
        templateTask1->SetAttribute("ComputeDemandDistribution", PointerValue(compute1));
        templateTask1->SetAttribute("OutputSizeDistribution", PointerValue(output1));
        Ptr<DagTask> dagTemplate1 = CreateObject<DagTask>();
        dagTemplate1->AddTask(templateTask1);
        client1->SetAttribute("DagTemplate", PointerValue(dagTemplate1));

        clientNode0->AddApplication(client0);
        client0->SetStartTime(Seconds(0.1));
        client0->SetStopTime(Seconds(2.0));

        clientNode1->AddApplication(client1);
        client1->SetStartTime(Seconds(0.1));
        client1->SetStopTime(Seconds(2.0));

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsAdmitted(),
                              2,
                              "Orchestrator should have admitted 2 workloads");
        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsCompleted(),
                              2,
                              "Orchestrator should have completed 2 workloads");
        NS_TEST_ASSERT_MSG_EQ(client0->GetResponsesReceived() + client1->GetResponsesReceived(),
                              2,
                              "Clients should have received 2 responses in total");

        NS_TEST_ASSERT_MSG_GT(m_backendsUsed.count(0),
                              0,
                              "Backend 0 should have received at least one task");
        NS_TEST_ASSERT_MSG_GT(m_backendsUsed.count(1),
                              0,
                              "Backend 1 should have received at least one task");

        uint64_t totalProcessed = server0->GetTasksProcessed() + server1->GetTasksProcessed();
        NS_TEST_ASSERT_MSG_EQ(totalProcessed,
                              2,
                              "Both servers should have processed 2 tasks total");
    }

    void OnTaskDispatched(uint64_t workloadId, uint64_t taskId, uint32_t backendIdx)
    {
        m_backendsUsed.insert(backendIdx);
    }

    std::set<uint32_t> m_backendsUsed;
};

/**
 * @ingroup continuum-tests
 * @brief Test Orchestrator's live network-delay tracking, enabled and (by default) disabled.
 */
class OrchestratorNetworkDelayTrackingTestCase : public TestCase
{
  public:
    OrchestratorNetworkDelayTrackingTestCase()
        : TestCase("Orchestrator live network-delay tracking")
    {
    }

  private:
    // Builds the standard single-client/single-backend topology, runs one task to
    // completion, and reports the resulting network state for backend 0.
    void RunScenario(bool enableTracking, bool& delayValidOut, Time& measuredDelayOut)
    {
        NodeContainer nodes;
        nodes.Create(3);
        Ptr<Node> clientNode = nodes.Get(0);
        Ptr<Node> orchNode = nodes.Get(1);
        Ptr<Node> serverNode = nodes.Get(2);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devClientOrch = p2p.Install(clientNode, orchNode);
        NetDeviceContainer devOrchServer = p2p.Install(orchNode, serverNode);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifClientOrch = ipv4.Assign(devClientOrch);

        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer ifOrchServer = ipv4.Assign(devOrchServer);

        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
        serverNode->AggregateObject(gpu);

        uint16_t serverPort = 9000;
        Ptr<BackendServer> server = CreateObject<BackendServer>();
        server->SetAttribute("Port", UintegerValue(serverPort));
        serverNode->AddApplication(server);
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(10.0));

        Cluster cluster;
        cluster.AddBackend(serverNode, InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

        Ptr<RoundRobinScheduler> scheduler = CreateObject<RoundRobinScheduler>();

        uint16_t orchPort = 8080;
        Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
        orchestrator->SetAttribute("Port", UintegerValue(orchPort));
        orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
        orchestrator->SetAttribute("EnableNetworkDelayTracking", BooleanValue(enableTracking));
        orchestrator->SetAttribute("NetworkDelaySmoothingFactor", DoubleValue(1.0));
        orchestrator->SetCluster(cluster);
        orchNode->AddApplication(orchestrator);
        orchestrator->SetStartTime(Seconds(0.0));
        orchestrator->SetStopTime(Seconds(10.0));

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        // 1 Hz, stop at 1.5s: the client emits exactly one task.
        Ptr<OffloadClient> client = CreateObject<OffloadClient>();
        client->SetAttribute("Remote",
                             AddressValue(InetSocketAddress(ifClientOrch.GetAddress(1), orchPort)));
        client->SetAttribute("InterArrival",
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
        client->SetAttribute("DagTemplate", PointerValue(dagTemplate));

        clientNode->AddApplication(client);
        client->SetStartTime(Seconds(0.1));
        client->SetStopTime(Seconds(1.5));

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        const ClusterState::BackendState& backend0 = orchestrator->GetClusterState().Get(0);
        delayValidOut = backend0.network.delayValid;
        measuredDelayOut = backend0.network.measuredDelay;

        Simulator::Destroy();
    }

    void DoRun() override
    {
        bool delayValid = true;
        Time measuredDelay;

        // Control case: tracking left at its default (disabled).
        RunScenario(false, delayValid, measuredDelay);
        NS_TEST_ASSERT_MSG_EQ(delayValid,
                              false,
                              "network.delayValid should stay false when tracking is disabled");

        // Tracking enabled: a live sample should be recorded after the round trip.
        RunScenario(true, delayValid, measuredDelay);
        NS_TEST_ASSERT_MSG_EQ(delayValid,
                              true,
                              "network.delayValid should become true when tracking is enabled");
        NS_TEST_ASSERT_MSG_EQ((measuredDelay >= Seconds(0)),
                              true,
                              "Measured delay should never be negative");
        NS_TEST_ASSERT_MSG_EQ((measuredDelay < Seconds(1)),
                              true,
                              "Measured delay should be well under the 1s simulation scale used "
                              "here (1ms links, sub-ms compute time)");
    }
};

} // namespace

TestCase*
CreateSingleTaskEndToEndTestCase()
{
    return new SingleTaskEndToEndTestCase;
}

TestCase*
CreateMultiBackendTestCase()
{
    return new MultiBackendTestCase;
}

TestCase*
CreateOrchestratorNetworkDelayTrackingTestCase()
{
    return new OrchestratorNetworkDelayTrackingTestCase;
}

} // namespace ns3
