/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/always-admit-policy.h"
#include "ns3/cluster.h"
#include "ns3/double.h"
#include "ns3/edge-orchestrator.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/first-fit-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/least-loaded-scheduler.h"
#include "ns3/offload-client.h"
#include "ns3/offload-server.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test single task end-to-end through EdgeOrchestrator.
 *
 * Topology: Client (n0) -> Orchestrator (n1) -> Server (n2) + GPU
 * Verifies the full admission -> dispatch -> completion -> response path.
 */
class SingleTaskEndToEndTestCase : public TestCase
{
  public:
    SingleTaskEndToEndTestCase()
        : TestCase("EdgeOrchestrator single task end-to-end"),
          m_workloadsAdmitted(0),
          m_workloadsCompleted(0),
          m_tasksDispatched(0),
          m_tasksCompleted(0)
    {
    }

  private:
    void DoRun() override
    {
        // Create 3 nodes: client, orchestrator, server
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
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
        serverNode->AggregateObject(gpu);

        uint16_t serverPort = 9000;
        Ptr<OffloadServer> server = CreateObject<OffloadServer>();
        server->SetAttribute("Port", UintegerValue(serverPort));
        serverNode->AddApplication(server);
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(10.0));

        Cluster cluster;
        cluster.AddBackend(serverNode, InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

        Ptr<FirstFitScheduler> scheduler = CreateObject<FirstFitScheduler>();
        Ptr<AlwaysAdmitPolicy> policy = CreateObject<AlwaysAdmitPolicy>();

        uint16_t orchPort = 8080;
        Ptr<EdgeOrchestrator> orchestrator = CreateObject<EdgeOrchestrator>();
        orchestrator->SetAttribute("Port", UintegerValue(orchPort));
        orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
        orchestrator->SetAttribute("AdmissionPolicy", PointerValue(policy));
        orchestrator->SetCluster(cluster);
        orchNode->AddApplication(orchestrator);
        orchestrator->SetStartTime(Seconds(0.0));
        orchestrator->SetStopTime(Seconds(10.0));

        // Connect orchestrator traces
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

        Ptr<OffloadClient> client = CreateObject<OffloadClient>();
        client->SetAttribute("Remote",
                             AddressValue(InetSocketAddress(ifClientOrch.GetAddress(1), orchPort)));
        client->SetAttribute("MaxTasks", UintegerValue(1));

        Ptr<ExponentialRandomVariable> interArrival = CreateObject<ExponentialRandomVariable>();
        interArrival->SetAttribute("Mean", DoubleValue(0.1));
        client->SetAttribute("InterArrivalTime", PointerValue(interArrival));

        Ptr<ExponentialRandomVariable> compute = CreateObject<ExponentialRandomVariable>();
        compute->SetAttribute("Mean", DoubleValue(1e9));
        client->SetAttribute("ComputeDemand", PointerValue(compute));

        Ptr<ExponentialRandomVariable> input = CreateObject<ExponentialRandomVariable>();
        input->SetAttribute("Mean", DoubleValue(1000));
        client->SetAttribute("InputSize", PointerValue(input));

        Ptr<ExponentialRandomVariable> output = CreateObject<ExponentialRandomVariable>();
        output->SetAttribute("Mean", DoubleValue(100));
        client->SetAttribute("OutputSize", PointerValue(output));

        clientNode->AddApplication(client);
        client->SetStartTime(Seconds(0.1));
        client->SetStopTime(Seconds(5.0));

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
 * @ingroup distributed-tests
 * @brief Test EdgeOrchestrator dispatches to multiple backends via LeastLoadedScheduler.
 *
 * Topology: Client (n0) -> Orchestrator (n1) -> Server0 (n2) + GPU
 *                                             -> Server1 (n3) + GPU
 * Verifies tasks are dispatched to different backends.
 */
class MultiBackendTestCase : public TestCase
{
  public:
    MultiBackendTestCase()
        : TestCase("EdgeOrchestrator dispatches to multiple backends")
    {
    }

  private:
    void DoRun() override
    {
        // Create 4 nodes: client, orchestrator, 2 servers
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

        {
            Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
            Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();
            Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
            gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
            gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
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
            gpu->SetAttribute("ProcessingModel", PointerValue(model));
            gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
            serverNode1->AggregateObject(gpu);
        }

        uint16_t serverPort = 9000;
        Ptr<OffloadServer> server0 = CreateObject<OffloadServer>();
        server0->SetAttribute("Port", UintegerValue(serverPort));
        serverNode0->AddApplication(server0);
        server0->SetStartTime(Seconds(0.0));
        server0->SetStopTime(Seconds(10.0));

        Ptr<OffloadServer> server1 = CreateObject<OffloadServer>();
        server1->SetAttribute("Port", UintegerValue(serverPort));
        serverNode1->AddApplication(server1);
        server1->SetStartTime(Seconds(0.0));
        server1->SetStopTime(Seconds(10.0));

        Cluster cluster;
        cluster.AddBackend(serverNode0, InetSocketAddress(ifOrchServer0.GetAddress(1), serverPort));
        cluster.AddBackend(serverNode1, InetSocketAddress(ifOrchServer1.GetAddress(1), serverPort));

        Ptr<LeastLoadedScheduler> scheduler = CreateObject<LeastLoadedScheduler>();
        Ptr<AlwaysAdmitPolicy> policy = CreateObject<AlwaysAdmitPolicy>();

        uint16_t orchPort = 8080;
        Ptr<EdgeOrchestrator> orchestrator = CreateObject<EdgeOrchestrator>();
        orchestrator->SetAttribute("Port", UintegerValue(orchPort));
        orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
        orchestrator->SetAttribute("AdmissionPolicy", PointerValue(policy));
        orchestrator->SetCluster(cluster);
        orchNode->AddApplication(orchestrator);
        orchestrator->SetStartTime(Seconds(0.0));
        orchestrator->SetStopTime(Seconds(10.0));

        orchestrator->TraceConnectWithoutContext(
            "TaskDispatched",
            MakeCallback(&MultiBackendTestCase::OnTaskDispatched, this));

        Ptr<OffloadClient> client = CreateObject<OffloadClient>();
        client->SetAttribute("Remote",
                             AddressValue(InetSocketAddress(ifClientOrch.GetAddress(1), orchPort)));
        client->SetAttribute("MaxTasks", UintegerValue(4));

        Ptr<ExponentialRandomVariable> interArrival = CreateObject<ExponentialRandomVariable>();
        interArrival->SetAttribute("Mean", DoubleValue(0.01));
        client->SetAttribute("InterArrivalTime", PointerValue(interArrival));

        Ptr<ExponentialRandomVariable> compute = CreateObject<ExponentialRandomVariable>();
        compute->SetAttribute("Mean", DoubleValue(1e12));
        client->SetAttribute("ComputeDemand", PointerValue(compute));

        Ptr<ExponentialRandomVariable> input = CreateObject<ExponentialRandomVariable>();
        input->SetAttribute("Mean", DoubleValue(1000));
        client->SetAttribute("InputSize", PointerValue(input));

        Ptr<ExponentialRandomVariable> output = CreateObject<ExponentialRandomVariable>();
        output->SetAttribute("Mean", DoubleValue(100));
        client->SetAttribute("OutputSize", PointerValue(output));

        clientNode->AddApplication(client);
        client->SetStartTime(Seconds(0.1));
        client->SetStopTime(Seconds(5.0));

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsAdmitted(),
                              4,
                              "Orchestrator should have admitted 4 workloads");
        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsCompleted(),
                              4,
                              "Orchestrator should have completed 4 workloads");
        NS_TEST_ASSERT_MSG_EQ(client->GetResponsesReceived(),
                              4,
                              "Client should have received 4 responses");

        NS_TEST_ASSERT_MSG_GT(m_backendsUsed.count(0),
                              0,
                              "Backend 0 should have received at least one task");
        NS_TEST_ASSERT_MSG_GT(m_backendsUsed.count(1),
                              0,
                              "Backend 1 should have received at least one task");

        uint64_t totalProcessed = server0->GetTasksCompleted() + server1->GetTasksCompleted();
        NS_TEST_ASSERT_MSG_EQ(totalProcessed,
                              4,
                              "Both servers should have completed 4 tasks total");
    }

    void OnTaskDispatched(uint64_t workloadId, uint64_t taskId, uint32_t backendIdx)
    {
        m_backendsUsed.insert(backendIdx);
    }

    std::set<uint32_t> m_backendsUsed;
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

} // namespace ns3
