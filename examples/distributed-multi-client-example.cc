/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/always-admit-policy.h"
#include "ns3/applications-module.h"
#include "ns3/cluster.h"
#include "ns3/core-module.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/edge-orchestrator.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/first-fit-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/offload-server-helper.h"
#include "ns3/offload-server.h"
#include "ns3/point-to-point-module.h"
#include "ns3/task.h"

// ===========================================================================
//
//  Multi-Client Distributed Computing Simulation Example
//
//  This example demonstrates multiple clients offloading tasks through a
//  single EdgeOrchestrator to a backend server. Each client generates tasks
//  with globally unique IDs, preventing ID collisions.
//
//  Network topology:
//
//       Client 0 (n0)     Client 1 (n1)     Client 2 (n2)
//           |                 |                 |
//           |  10.1.1.0/24    |  10.1.2.0/24    |  10.1.3.0/24
//           |                 |                 |
//           +-----------------+-----------------+
//                             |
//                    Orchestrator (n3)
//                   +------------------+
//                   | EdgeOrchestrator |
//                   | Admission ctrl   |
//                   | + scheduling     |
//                   +--------+---------+
//                            |  10.1.4.0/24
//                            |
//                     Server Node (n4)
//                   +------------------+
//                   |  OffloadServer   |
//                   |        |         |
//                   |        v         |
//                   |  GpuAccelerator  |
//                   +------------------+
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
 * @param task The task that was submitted.
 */
static void
TaskSent(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client " << GetClientIdFromTaskId(task->GetTaskId()) << "] "
                  << "Task 0x" << std::hex << task->GetTaskId() << std::dec << " sent"
                  << " (input=" << task->GetInputSize() / 1024.0 << " KB)");
}

/**
 * Callback for when a response is received by a client.
 *
 * @param task The completed task from the response.
 * @param rtt Round-trip time from submission to response.
 */
static void
ResponseReceived(Ptr<const Task> task, Time rtt)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client " << GetClientIdFromTaskId(task->GetTaskId()) << "] "
                  << "Task 0x" << std::hex << task->GetTaskId() << std::dec
                  << " response (RTT=" << rtt.As(Time::MS) << ")");
}

/**
 * Callback for when a task is received by the server.
 *
 * @param task The task that was received.
 */
static void
TaskReceived(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S) << " [Server] Task 0x" << std::hex
                                               << task->GetTaskId() << std::dec << " received");
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
    NS_LOG_UNCOND("Topology: Clients → Orchestrator → Server");
    NS_LOG_UNCOND("Clients: " << numClients << ", Tasks per client: " << tasksPerClient);
    NS_LOG_UNCOND("");

    // Create nodes: numClients client nodes + 1 orchestrator + 1 server
    NodeContainer clientNodes;
    clientNodes.Create(numClients);

    NodeContainer orchNode;
    orchNode.Create(1);

    NodeContainer serverNode;
    serverNode.Create(1);

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(clientNodes);
    stack.Install(orchNode);
    stack.Install(serverNode);

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    // Client-to-orchestrator links
    Ipv4AddressHelper address;
    std::vector<Ipv4Address> orchAddresses;

    for (uint32_t i = 0; i < numClients; i++)
    {
        NetDeviceContainer devices = pointToPoint.Install(clientNodes.Get(i), orchNode.Get(0));
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        orchAddresses.push_back(interfaces.GetAddress(1)); // Orchestrator's address
    }

    // Orchestrator-to-server link
    NetDeviceContainer devOrchServer = pointToPoint.Install(orchNode.Get(0), serverNode.Get(0));
    std::ostringstream serverSubnet;
    serverSubnet << "10.1." << (numClients + 1) << ".0";
    address.SetBase(serverSubnet.str().c_str(), "255.255.255.0");
    Ipv4InterfaceContainer ifOrchServer = address.Assign(devOrchServer);

    // Create processing model and queue scheduler
    Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
    Ptr<FifoQueueScheduler> queueScheduler = CreateObject<FifoQueueScheduler>();

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
    gpu->SetAttribute("QueueScheduler", PointerValue(queueScheduler));
    gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
    serverNode.Get(0)->AggregateObject(gpu);

    // Connect GPU trace source
    gpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&GpuTaskCompleted));

    // Install OffloadServer on server node
    uint16_t serverPort = 9000;
    OffloadServerHelper serverHelper(serverPort);
    ApplicationContainer serverApps = serverHelper.Install(serverNode.Get(0));

    Ptr<OffloadServer> server = DynamicCast<OffloadServer>(serverApps.Get(0));
    server->TraceConnectWithoutContext("TaskReceived", MakeCallback(&TaskReceived));

    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    // Set up Cluster with one backend
    Cluster cluster;
    cluster.AddBackend(serverNode.Get(0),
                       InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

    // Set up EdgeOrchestrator
    Ptr<FirstFitScheduler> scheduler = CreateObject<FirstFitScheduler>();
    Ptr<AlwaysAdmitPolicy> policy = CreateObject<AlwaysAdmitPolicy>();

    uint16_t orchPort = 8080;
    Ptr<EdgeOrchestrator> orchestrator = CreateObject<EdgeOrchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
    orchestrator->SetAttribute("AdmissionPolicy", PointerValue(policy));
    orchestrator->SetCluster(cluster);
    orchNode.Get(0)->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(simTime + 1.0));

    // Install OffloadClient on each client node — connect to orchestrator
    std::vector<Ptr<OffloadClient>> clients;

    for (uint32_t i = 0; i < numClients; i++)
    {
        OffloadClientHelper clientHelper(InetSocketAddress(orchAddresses[i], orchPort));
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

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

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
    NS_LOG_UNCOND("Total tasks sent:      " << totalTasksSent);
    NS_LOG_UNCOND("Total responses:       " << totalResponsesReceived);
    NS_LOG_UNCOND("Workloads admitted:    " << orchestrator->GetWorkloadsAdmitted());
    NS_LOG_UNCOND("Workloads completed:   " << orchestrator->GetWorkloadsCompleted());
    NS_LOG_UNCOND("Server tasks received: " << server->GetTasksReceived());
    NS_LOG_UNCOND("Server tasks done:     " << server->GetTasksCompleted());
    NS_LOG_UNCOND("Server RX bytes:       " << server->GetTotalRx());
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Energy ===");
    NS_LOG_UNCOND("Total energy:          " << gpu->GetTotalEnergy() << " J");

    Simulator::Destroy();

    return 0;
}
