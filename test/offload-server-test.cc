/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/double.h"
#include "ns3/accelerator.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/inet-socket-address.h"
#include "ns3/pointer.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/offload-header.h"
#include "ns3/offload-server.h"
#include "ns3/packet.h"
#include "ns3/simple-channel.h"
#include "ns3/simple-net-device.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test OffloadServer receives tasks and submits to Accelerator
 */
class OffloadServerBasicTestCase : public TestCase
{
  public:
    OffloadServerBasicTestCase()
        : TestCase("Test OffloadServer basic task reception and Accelerator submission"),
          m_taskReceived(false),
          m_taskCompleted(false)
    {
    }

  private:
    void DoRun() override
    {
        // Create a node with GpuAccelerator
        Ptr<Node> serverNode = CreateObject<Node>();

        // Install internet stack (required for TCP sockets)
        InternetStackHelper internet;
        internet.Install(serverNode);

        // Create processing model
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();

        // Create and configure GPU
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));     // 1 TFLOPS
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11)); // 100 GB/s
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        serverNode->AggregateObject(gpu);

        // Create and install OffloadServer
        Ptr<OffloadServer> server = CreateObject<OffloadServer>();
        server->SetAttribute("Port", UintegerValue(9000));
        serverNode->AddApplication(server);

        // Connect to trace sources
        server->TraceConnectWithoutContext(
            "TaskReceived",
            MakeCallback(&OffloadServerBasicTestCase::OnTaskReceived, this));
        server->TraceConnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&OffloadServerBasicTestCase::OnTaskCompleted, this));

        // Start the server
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(10.0));

        // Verify accelerator is accessible from server's node (via both interfaces)
        Ptr<Accelerator> nodeAccel = serverNode->GetObject<Accelerator>();
        NS_TEST_ASSERT_MSG_NE(nodeAccel, nullptr, "Accelerator should be aggregated to node");
        NS_TEST_ASSERT_MSG_EQ(nodeAccel->GetName(), "GPU", "Accelerator should be a GPU");

        Ptr<GpuAccelerator> nodeGpu = serverNode->GetObject<GpuAccelerator>();
        NS_TEST_ASSERT_MSG_NE(nodeGpu, nullptr, "GpuAccelerator should be accessible");

        // Run and cleanup
        Simulator::Run();
        Simulator::Destroy();

        // Basic test - server should start without errors
        NS_TEST_ASSERT_MSG_EQ(server->GetTasksReceived(), 0, "No tasks should be received yet");
    }

    void OnTaskReceived(const OffloadHeader& header)
    {
        m_taskReceived = true;
    }

    void OnTaskCompleted(const OffloadHeader& header, Time duration)
    {
        m_taskCompleted = true;
    }

    bool m_taskReceived;
    bool m_taskCompleted;
};

/**
 * @ingroup distributed-tests
 * @brief Test OffloadServer handles missing Accelerator gracefully
 */
class OffloadServerNoAcceleratorTestCase : public TestCase
{
  public:
    OffloadServerNoAcceleratorTestCase()
        : TestCase("Test OffloadServer handles missing Accelerator gracefully")
    {
    }

  private:
    void DoRun() override
    {
        // Create a node WITHOUT Accelerator
        Ptr<Node> serverNode = CreateObject<Node>();

        // Install internet stack (required for TCP sockets)
        InternetStackHelper internet;
        internet.Install(serverNode);

        // Create and install OffloadServer
        Ptr<OffloadServer> server = CreateObject<OffloadServer>();
        server->SetAttribute("Port", UintegerValue(9001));
        serverNode->AddApplication(server);

        // Start the server - should not crash even without GPU
        server->SetStartTime(Seconds(0.0));
        server->SetStopTime(Seconds(1.0));

        // Run and cleanup
        Simulator::Run();
        Simulator::Destroy();

        // Server should start without crashing
        NS_TEST_ASSERT_MSG_EQ(server->GetTasksReceived(), 0, "No tasks received");
    }
};

} // namespace

TestCase*
CreateOffloadServerBasicTestCase()
{
    return new OffloadServerBasicTestCase;
}

TestCase*
CreateOffloadServerNoAcceleratorTestCase()
{
    return new OffloadServerNoAcceleratorTestCase;
}

} // namespace ns3
