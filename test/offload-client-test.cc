/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/double.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/offload-server-helper.h"
#include "ns3/offload-server.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
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

        // Create and configure GPU on server
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));     // 1 TFLOPS
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11)); // 100 GB/s
        serverNode->AggregateObject(gpu);

        // Install OffloadServer on server node
        uint16_t port = 9000;
        OffloadServerHelper serverHelper(port);
        ApplicationContainer serverApps = serverHelper.Install(serverNode);
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(10.0));

        Ptr<OffloadServer> server = DynamicCast<OffloadServer>(serverApps.Get(0));

        // Install OffloadClient on client1 (uses server address from if1)
        OffloadClientHelper clientHelper1(InetSocketAddress(if1.GetAddress(0), port));
        clientHelper1.SetMeanInterArrival(0.1);     // 100ms between tasks
        clientHelper1.SetMeanComputeDemand(1e9);    // 1 GFLOP
        clientHelper1.SetMeanInputSize(1000);       // 1 KB
        clientHelper1.SetMeanOutputSize(100);       // 100 bytes
        clientHelper1.SetMaxTasks(3);

        ApplicationContainer client1Apps = clientHelper1.Install(client1Node);
        client1Apps.Start(Seconds(0.1));
        client1Apps.Stop(Seconds(5.0));

        Ptr<OffloadClient> client1 = DynamicCast<OffloadClient>(client1Apps.Get(0));

        // Install OffloadClient on client2 (uses server address from if2)
        OffloadClientHelper clientHelper2(InetSocketAddress(if2.GetAddress(0), port));
        clientHelper2.SetMeanInterArrival(0.1);     // 100ms between tasks
        clientHelper2.SetMeanComputeDemand(1e9);    // 1 GFLOP
        clientHelper2.SetMeanInputSize(1000);       // 1 KB
        clientHelper2.SetMeanOutputSize(100);       // 100 bytes
        clientHelper2.SetMaxTasks(3);

        ApplicationContainer client2Apps = clientHelper2.Install(client2Node);
        client2Apps.Start(Seconds(0.1));
        client2Apps.Stop(Seconds(5.0));

        Ptr<OffloadClient> client2 = DynamicCast<OffloadClient>(client2Apps.Get(0));

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
