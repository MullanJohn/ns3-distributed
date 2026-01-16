/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/double.h"
#include "ns3/fifo-queue-scheduler.h"
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
#include "ns3/tcp-connection-manager.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test that multiple OffloadClients can send tasks to the same server
 *        and receive correct responses (validates task ID uniqueness)
 */
class OffloadClientMultiClientTestCase : public TestCase
{
  public:
    OffloadClientMultiClientTestCase()
        : TestCase("Test multiple OffloadClients with unique task IDs")
    {
    }

  private:
    void DoRun() override
    {
        // Create 3 nodes: 1 server + 2 clients
        NodeContainer nodes;
        nodes.Create(3);
        Ptr<Node> serverNode = nodes.Get(0);
        Ptr<Node> client1Node = nodes.Get(1);
        Ptr<Node> client2Node = nodes.Get(2);

        // Create point-to-point links (star topology)
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer dev1 = p2p.Install(serverNode, client1Node);
        NetDeviceContainer dev2 = p2p.Install(serverNode, client2Node);

        // Install internet stack
        InternetStackHelper internet;
        internet.Install(nodes);

        // Assign IP addresses
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer if1 = ipv4.Assign(dev1);

        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer if2 = ipv4.Assign(dev2);

        // Create processing model and queue scheduler
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        // Create and configure GPU on server
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));     // 1 TFLOPS
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11)); // 100 GB/s
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));
        serverNode->AggregateObject(gpu);

        // Create ConnectionManager for server (TCP transport)
        Ptr<TcpConnectionManager> serverConnMgr = CreateObject<TcpConnectionManager>();

        // Install OffloadServer on server node with explicit ConnectionManager
        uint16_t port = 9000;
        Ptr<OffloadServer> server = CreateObject<OffloadServer>();
        server->SetAttribute("Port", UintegerValue(port));
        server->SetAttribute("ConnectionManager", PointerValue(serverConnMgr));
        serverNode->AddApplication(server);
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(10.0));

        // Create ConnectionManager for client1 (TCP transport)
        Ptr<TcpConnectionManager> client1ConnMgr = CreateObject<TcpConnectionManager>();

        // Install OffloadClient on client1 with explicit ConnectionManager
        Ptr<OffloadClient> client1 = CreateObject<OffloadClient>();
        client1->SetAttribute("Remote", AddressValue(InetSocketAddress(if1.GetAddress(0), port)));
        client1->SetAttribute("ConnectionManager", PointerValue(client1ConnMgr));
        client1->SetAttribute("MaxTasks", UintegerValue(3));

        // Configure task generation distributions for client1
        Ptr<ExponentialRandomVariable> interArrival1 = CreateObject<ExponentialRandomVariable>();
        interArrival1->SetAttribute("Mean", DoubleValue(0.1)); // 100ms
        client1->SetAttribute("InterArrivalTime", PointerValue(interArrival1));

        Ptr<ExponentialRandomVariable> compute1 = CreateObject<ExponentialRandomVariable>();
        compute1->SetAttribute("Mean", DoubleValue(1e9)); // 1 GFLOP
        client1->SetAttribute("ComputeDemand", PointerValue(compute1));

        Ptr<ExponentialRandomVariable> input1 = CreateObject<ExponentialRandomVariable>();
        input1->SetAttribute("Mean", DoubleValue(1000)); // 1 KB
        client1->SetAttribute("InputSize", PointerValue(input1));

        Ptr<ExponentialRandomVariable> output1 = CreateObject<ExponentialRandomVariable>();
        output1->SetAttribute("Mean", DoubleValue(100)); // 100 bytes
        client1->SetAttribute("OutputSize", PointerValue(output1));

        client1Node->AddApplication(client1);
        client1->SetStartTime(Seconds(0.1));
        client1->SetStopTime(Seconds(5.0));

        // Create ConnectionManager for client2 (TCP transport)
        Ptr<TcpConnectionManager> client2ConnMgr = CreateObject<TcpConnectionManager>();

        // Install OffloadClient on client2 with explicit ConnectionManager
        Ptr<OffloadClient> client2 = CreateObject<OffloadClient>();
        client2->SetAttribute("Remote", AddressValue(InetSocketAddress(if2.GetAddress(0), port)));
        client2->SetAttribute("ConnectionManager", PointerValue(client2ConnMgr));
        client2->SetAttribute("MaxTasks", UintegerValue(3));

        // Configure task generation distributions for client2
        Ptr<ExponentialRandomVariable> interArrival2 = CreateObject<ExponentialRandomVariable>();
        interArrival2->SetAttribute("Mean", DoubleValue(0.1)); // 100ms
        client2->SetAttribute("InterArrivalTime", PointerValue(interArrival2));

        Ptr<ExponentialRandomVariable> compute2 = CreateObject<ExponentialRandomVariable>();
        compute2->SetAttribute("Mean", DoubleValue(1e9)); // 1 GFLOP
        client2->SetAttribute("ComputeDemand", PointerValue(compute2));

        Ptr<ExponentialRandomVariable> input2 = CreateObject<ExponentialRandomVariable>();
        input2->SetAttribute("Mean", DoubleValue(1000)); // 1 KB
        client2->SetAttribute("InputSize", PointerValue(input2));

        Ptr<ExponentialRandomVariable> output2 = CreateObject<ExponentialRandomVariable>();
        output2->SetAttribute("Mean", DoubleValue(100)); // 100 bytes
        client2->SetAttribute("OutputSize", PointerValue(output2));

        client2Node->AddApplication(client2);
        client2->SetStartTime(Seconds(0.1));
        client2->SetStopTime(Seconds(5.0));

        // Run simulation
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();

        // Verify both clients sent their tasks
        NS_TEST_ASSERT_MSG_EQ(client1->GetTasksSent(),
                              3,
                              "Client 1 should have sent 3 tasks");
        NS_TEST_ASSERT_MSG_EQ(client2->GetTasksSent(),
                              3,
                              "Client 2 should have sent 3 tasks");

        // Verify both clients received all their responses
        // This is the key test - if task IDs collided, responses would be misrouted
        NS_TEST_ASSERT_MSG_EQ(client1->GetResponsesReceived(),
                              3,
                              "Client 1 should have received 3 responses");
        NS_TEST_ASSERT_MSG_EQ(client2->GetResponsesReceived(),
                              3,
                              "Client 2 should have received 3 responses");

        // Verify server processed all tasks
        NS_TEST_ASSERT_MSG_EQ(server->GetTasksReceived(),
                              6,
                              "Server should have received 6 tasks total");
        NS_TEST_ASSERT_MSG_EQ(server->GetTasksCompleted(),
                              6,
                              "Server should have completed 6 tasks total");
    }
};

} // namespace

TestCase*
CreateOffloadClientMultiClientTestCase()
{
    return new OffloadClientMultiClientTestCase;
}

} // namespace ns3
