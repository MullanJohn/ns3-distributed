/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/offload-header.h"
#include "ns3/offload-server-helper.h"
#include "ns3/offload-server.h"
#include "ns3/point-to-point-module.h"
#include "ns3/task.h"

// ===========================================================================
//
//  Multi-Client Distributed Computing Simulation Example
//
//  This example demonstrates multiple clients offloading tasks to a single
//  server. Each client generates tasks with globally unique IDs, preventing
//  ID collisions when the server processes tasks from different sources.
//
//  Network topology:
//
//       Client 0 (n0)     Client 1 (n1)     Client 2 (n2)
//           |                 |                 |
//           |  10.1.1.0/24    |  10.1.2.0/24    |  10.1.3.0/24
//           |                 |                 |
//           +-----------------+-----------------+
//                             |
//                      Server Node (n3)
//                     +------------------+
//                     |  OffloadServer   |
//                     |        |         |
//                     |        v         |
//                     |  GpuAccelerator  |
//                     +------------------+
//
//  Task ID format: (clientId << 32) | sequenceNumber
//    - Client 0: 0x0000000000000000, 0x0000000000000001, ...
//    - Client 1: 0x0000000100000000, 0x0000000100000001, ...
//    - Client 2: 0x0000000200000000, 0x0000000200000001, ...
//
// ===========================================================================

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistributedMultiClientExample");

/**
 * Extract client ID from a task ID.
 *
 * @param taskId The full task ID.
 * @return The client ID (upper 32 bits).
 */
static uint32_t
GetClientIdFromTaskId(uint64_t taskId)
{
    return static_cast<uint32_t>(taskId >> 32);
}

/**
 * Callback for when a task is sent by a client.
 *
 * @param header The offload header that was sent.
 */
static void
TaskSent(const OffloadHeader& header)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client " << GetClientIdFromTaskId(header.GetTaskId()) << "] "
                  << "Task 0x" << std::hex << header.GetTaskId() << std::dec << " sent"
                  << " (input=" << header.GetInputSize() / 1024.0 << " KB)");
}

/**
 * Callback for when a response is received by a client.
 *
 * @param header The offload header from the response.
 * @param rtt Round-trip time from task sent to response received.
 */
static void
ResponseReceived(const OffloadHeader& header, Time rtt)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client " << GetClientIdFromTaskId(header.GetTaskId()) << "] "
                  << "Task 0x" << std::hex << header.GetTaskId() << std::dec
                  << " response (RTT=" << rtt.As(Time::MS) << ")");
}

/**
 * Callback for when a task is received by the server.
 *
 * @param header The offload header that was received.
 */
static void
TaskReceived(const OffloadHeader& header)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Server] Task 0x" << std::hex << header.GetTaskId() << std::dec
                  << " received from client " << GetClientIdFromTaskId(header.GetTaskId()));
}

/**
 * Callback for when a task completes on the GPU.
 *
 * @param task The completed task.
 * @param duration Processing duration.
 */
static void
GpuTaskCompleted(Ptr<const Task> task, Time duration)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [GPU] Task 0x" << std::hex << task->GetTaskId() << std::dec
                  << " completed (processing=" << duration.As(Time::MS) << ")");
}

int
main(int argc, char* argv[])
{
    // Default parameters
    uint32_t numClients = 3;
    std::string dataRate = "100Mbps";
    std::string delay = "2ms";
    double simTime = 1.0;
    uint64_t tasksPerClient = 3;
    double meanInterArrival = 0.1;
    double meanComputeDemand = 1e9;
    double meanInputSize = 5e4;
    double meanOutputSize = 1e4;
    double computeRate = 1e12;
    double memoryBandwidth = 900e9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numClients", "Number of client nodes", numClients);
    cmd.AddValue("dataRate", "Link data rate", dataRate);
    cmd.AddValue("delay", "Link delay", delay);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("tasksPerClient", "Number of tasks per client", tasksPerClient);
    cmd.AddValue("meanInterArrival", "Mean task inter-arrival time in seconds", meanInterArrival);
    cmd.AddValue("meanComputeDemand", "Mean compute demand in FLOPS", meanComputeDemand);
    cmd.AddValue("meanInputSize", "Mean input data size in bytes", meanInputSize);
    cmd.AddValue("meanOutputSize", "Mean output data size in bytes", meanOutputSize);
    cmd.AddValue("computeRate", "GPU compute rate in FLOPS", computeRate);
    cmd.AddValue("memoryBandwidth", "GPU memory bandwidth in bytes/sec", memoryBandwidth);
    cmd.Parse(argc, argv);

    NS_LOG_UNCOND("Multi-Client Distributed Computing Example");
    NS_LOG_UNCOND("Clients: " << numClients << ", Tasks per client: " << tasksPerClient);
    NS_LOG_UNCOND("");

    // Create nodes: numClients client nodes + 1 server node
    NodeContainer clientNodes;
    clientNodes.Create(numClients);

    NodeContainer serverNode;
    serverNode.Create(1);

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(clientNodes);
    stack.Install(serverNode);

    // Create point-to-point links from each client to the server
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    Ipv4AddressHelper address;
    std::vector<Ipv4Address> serverAddresses;

    for (uint32_t i = 0; i < numClients; i++)
    {
        // Create link between client i and server
        NetDeviceContainer devices = pointToPoint.Install(clientNodes.Get(i), serverNode.Get(0));

        // Assign IP addresses for this link
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Store server's address on this subnet for client to use
        serverAddresses.push_back(interfaces.GetAddress(1));
    }

    // Create processing model and queue scheduler
    Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
    Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

    // Create energy model for GPU
    Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
    energyModel->SetAttribute("StaticPower", DoubleValue(30.0));
    energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(2e-9));

    // Create GPU accelerator and aggregate to server node
    Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
    gpu->SetAttribute("ComputeRate", DoubleValue(computeRate));
    gpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));
    gpu->SetAttribute("Voltage", DoubleValue(1.0));
    gpu->SetAttribute("Frequency", DoubleValue(1.5e9));
    gpu->SetAttribute("ProcessingModel", PointerValue(model));
    gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));
    gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
    serverNode.Get(0)->AggregateObject(gpu);

    // Connect GPU trace source
    gpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&GpuTaskCompleted));

    // Install OffloadServer on server node
    uint16_t port = 9000;
    OffloadServerHelper serverHelper(port);
    ApplicationContainer serverApps = serverHelper.Install(serverNode.Get(0));

    Ptr<OffloadServer> server = DynamicCast<OffloadServer>(serverApps.Get(0));
    server->TraceConnectWithoutContext("TaskReceived", MakeCallback(&TaskReceived));

    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    // Install OffloadClient on each client node
    std::vector<Ptr<OffloadClient>> clients;

    for (uint32_t i = 0; i < numClients; i++)
    {
        OffloadClientHelper clientHelper(InetSocketAddress(serverAddresses[i], port));
        clientHelper.SetMeanInterArrival(meanInterArrival);
        clientHelper.SetMeanComputeDemand(meanComputeDemand);
        clientHelper.SetMeanInputSize(meanInputSize);
        clientHelper.SetMeanOutputSize(meanOutputSize);
        clientHelper.SetMaxTasks(tasksPerClient);

        ApplicationContainer clientApps = clientHelper.Install(clientNodes.Get(i));

        Ptr<OffloadClient> client = DynamicCast<OffloadClient>(clientApps.Get(0));
        client->TraceConnectWithoutContext("TaskSent", MakeCallback(&TaskSent));
        client->TraceConnectWithoutContext("ResponseReceived", MakeCallback(&ResponseReceived));

        clients.push_back(client);

        // Stagger client start times slightly to show interleaving
        clientApps.Start(Seconds(0.1 + i * 0.01));
        clientApps.Stop(Seconds(simTime));
    }

    // Run simulation
    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    // Print summary
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Summary ===");

    uint64_t totalTasksSent = 0;
    uint64_t totalResponsesReceived = 0;
    uint64_t totalClientTx = 0;
    uint64_t totalClientRx = 0;

    for (uint32_t i = 0; i < numClients; i++)
    {
        NS_LOG_UNCOND("Client " << i << ": sent=" << clients[i]->GetTasksSent()
                                << ", responses=" << clients[i]->GetResponsesReceived()
                                << ", TX=" << clients[i]->GetTotalTx() << " bytes"
                                << ", RX=" << clients[i]->GetTotalRx() << " bytes");
        totalTasksSent += clients[i]->GetTasksSent();
        totalResponsesReceived += clients[i]->GetResponsesReceived();
        totalClientTx += clients[i]->GetTotalTx();
        totalClientRx += clients[i]->GetTotalRx();
    }

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("Total tasks sent:     " << totalTasksSent);
    NS_LOG_UNCOND("Total responses:      " << totalResponsesReceived);
    NS_LOG_UNCOND("Server tasks received:" << server->GetTasksReceived());
    NS_LOG_UNCOND("Server tasks done:    " << server->GetTasksCompleted());
    NS_LOG_UNCOND("Server RX bytes:      " << server->GetTotalRx());
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Energy ===");
    NS_LOG_UNCOND("Total energy:         " << gpu->GetTotalEnergy() << " J");

    Simulator::Destroy();

    return 0;
}
