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
#include "ns3/offload-client.h"
#include "ns3/offload-server.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/task.h"
#include "ns3/tcp-connection-manager.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test OffloadClient admission protocol through orchestrator
 *
 * Topology: Client (n0) → Orchestrator (n1) → Server (n2) + GPU
 */
class OffloadClientAdmissionTestCase : public TestCase
{
  public:
    OffloadClientAdmissionTestCase()
        : TestCase("Test OffloadClient two-phase admission via orchestrator"),
          m_tasksSent(0),
          m_responsesReceived(0)
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

        // Create point-to-point links
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devClientOrch = p2p.Install(clientNode, orchNode);
        NetDeviceContainer devOrchServer = p2p.Install(orchNode, serverNode);

        // Install internet stack
        InternetStackHelper internet;
        internet.Install(nodes);

        // Assign IP addresses
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifClientOrch = ipv4.Assign(devClientOrch);

        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer ifOrchServer = ipv4.Assign(devOrchServer);

        // Set up GPU on server node
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
        serverNode->AggregateObject(gpu);

        // Install OffloadServer on server node
        uint16_t serverPort = 9000;
        Ptr<OffloadServer> server = CreateObject<OffloadServer>();
        server->SetAttribute("Port", UintegerValue(serverPort));
        serverNode->AddApplication(server);
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(10.0));

        // Set up Cluster with one backend
        Cluster cluster;
        cluster.AddBackend(serverNode, InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

        // Set up orchestrator
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

        // Set up client connecting to orchestrator
        Ptr<OffloadClient> client = CreateObject<OffloadClient>();
        client->SetAttribute("Remote",
                             AddressValue(InetSocketAddress(ifClientOrch.GetAddress(1), orchPort)));
        client->SetAttribute("MaxTasks", UintegerValue(3));

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

        // Connect traces
        client->TraceConnectWithoutContext(
            "TaskSent",
            MakeCallback(&OffloadClientAdmissionTestCase::OnTaskSent, this));
        client->TraceConnectWithoutContext(
            "ResponseReceived",
            MakeCallback(&OffloadClientAdmissionTestCase::OnResponseReceived, this));

        // Run simulation
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();

        // Verify client sent tasks and received responses
        NS_TEST_ASSERT_MSG_EQ(client->GetTasksSent(), 3, "Client should have sent 3 tasks");
        NS_TEST_ASSERT_MSG_EQ(client->GetResponsesReceived(),
                              3,
                              "Client should have received 3 responses");
        NS_TEST_ASSERT_MSG_EQ(m_tasksSent, 3, "TaskSent trace should have fired 3 times");
        NS_TEST_ASSERT_MSG_EQ(m_responsesReceived,
                              3,
                              "ResponseReceived trace should have fired 3 times");

        // Verify orchestrator processed all workloads
        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsAdmitted(),
                              3,
                              "Orchestrator should have admitted 3 workloads");
        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsCompleted(),
                              3,
                              "Orchestrator should have completed 3 workloads");

        // Verify server processed all tasks
        NS_TEST_ASSERT_MSG_EQ(server->GetTasksReceived(), 3, "Server should have received 3 tasks");
        NS_TEST_ASSERT_MSG_EQ(server->GetTasksCompleted(),
                              3,
                              "Server should have completed 3 tasks");
    }

    void OnTaskSent(Ptr<const Task> task)
    {
        m_tasksSent++;
    }

    void OnResponseReceived(Ptr<const Task> task, Time rtt)
    {
        m_responsesReceived++;
    }

    uint32_t m_tasksSent;
    uint32_t m_responsesReceived;
};

/**
 * @ingroup distributed-tests
 * @brief Test multiple OffloadClients through orchestrator
 *
 * Topology: Client0 (n0) ─┐
 *           Client1 (n1) ─┤→ Orchestrator (n3) → Server (n4) + GPU
 *           Client2 (n2) ─┘
 */
class OffloadClientMultiClientTestCase : public TestCase
{
  public:
    OffloadClientMultiClientTestCase()
        : TestCase("Test multiple OffloadClients with unique task IDs via orchestrator")
    {
    }

  private:
    void DoRun() override
    {
        // Create 5 nodes: 3 clients + 1 orchestrator + 1 server
        NodeContainer nodes;
        nodes.Create(5);
        Ptr<Node> orchNode = nodes.Get(3);
        Ptr<Node> serverNode = nodes.Get(4);

        // Create point-to-point links
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        // Install internet stack on all nodes
        InternetStackHelper internet;
        internet.Install(nodes);

        // Client-to-orchestrator links
        Ipv4AddressHelper ipv4;
        std::vector<Ipv4Address> orchAddresses;

        for (uint32_t i = 0; i < 3; i++)
        {
            NetDeviceContainer dev = p2p.Install(nodes.Get(i), orchNode);
            std::ostringstream subnet;
            subnet << "10.1." << (i + 1) << ".0";
            ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
            Ipv4InterfaceContainer ifc = ipv4.Assign(dev);
            orchAddresses.push_back(ifc.GetAddress(1)); // Orchestrator's address on this subnet
        }

        // Orchestrator-to-server link
        NetDeviceContainer devOrchServer = p2p.Install(orchNode, serverNode);
        ipv4.SetBase("10.1.4.0", "255.255.255.0");
        Ipv4InterfaceContainer ifOrchServer = ipv4.Assign(devOrchServer);

        // Set up GPU on server node
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
        serverNode->AggregateObject(gpu);

        // Install OffloadServer on server node
        uint16_t serverPort = 9000;
        Ptr<OffloadServer> server = CreateObject<OffloadServer>();
        server->SetAttribute("Port", UintegerValue(serverPort));
        serverNode->AddApplication(server);
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(10.0));

        // Set up Cluster with one backend
        Cluster cluster;
        cluster.AddBackend(serverNode, InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

        // Set up orchestrator
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

        // Install 3 OffloadClients
        std::vector<Ptr<OffloadClient>> clients;
        for (uint32_t i = 0; i < 3; i++)
        {
            Ptr<OffloadClient> client = CreateObject<OffloadClient>();
            client->SetAttribute("Remote",
                                 AddressValue(InetSocketAddress(orchAddresses[i], orchPort)));
            client->SetAttribute("MaxTasks", UintegerValue(3));

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

            nodes.Get(i)->AddApplication(client);
            client->SetStartTime(Seconds(0.1));
            client->SetStopTime(Seconds(5.0));
            clients.push_back(client);
        }

        // Run simulation
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();

        // Verify all clients sent and received their tasks
        for (uint32_t i = 0; i < 3; i++)
        {
            NS_TEST_ASSERT_MSG_EQ(clients[i]->GetTasksSent(),
                                  3,
                                  "Client " << i << " should have sent 3 tasks");
            NS_TEST_ASSERT_MSG_EQ(clients[i]->GetResponsesReceived(),
                                  3,
                                  "Client " << i << " should have received 3 responses");
        }

        // Verify orchestrator and server totals
        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsAdmitted(),
                              9,
                              "Orchestrator should have admitted 9 workloads");
        NS_TEST_ASSERT_MSG_EQ(orchestrator->GetWorkloadsCompleted(),
                              9,
                              "Orchestrator should have completed 9 workloads");
        NS_TEST_ASSERT_MSG_EQ(server->GetTasksReceived(), 9, "Server should have received 9 tasks");
        NS_TEST_ASSERT_MSG_EQ(server->GetTasksCompleted(),
                              9,
                              "Server should have completed 9 tasks");
    }
};

} // namespace

TestCase*
CreateOffloadClientAdmissionTestCase()
{
    return new OffloadClientAdmissionTestCase;
}

TestCase*
CreateOffloadClientMultiClientTestCase()
{
    return new OffloadClientMultiClientTestCase;
}

} // namespace ns3
