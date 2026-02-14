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
//  Distributed Computing Simulation Example
//
//  This example demonstrates task offloading from a client through an
//  EdgeOrchestrator to a backend server using the two-phase admission
//  protocol.
//
//       Client Node (n0)         Orchestrator (n1)         Server Node (n2)
//      +---------------+       +-----------------+       +------------------+
//      |               |       |                 |       |                  |
//      | OffloadClient | ----> | EdgeOrchestrator| ----> |  OffloadServer   |
//      |  Sends tasks  |       | Admission ctrl  |       |    Receives      |
//      |  via admission|       | + scheduling    |       |      tasks       |
//      |  protocol     | <---- |                 | <---- |        |         |
//      |               |       |                 |       |        v         |
//      +---------------+       +-----------------+       |  GpuAccelerator  |
//         10.1.1.1                 10.1.1.2              |   Processes via: |
//                                  10.1.2.1              |   1. Input xfer  |
//                                                        |   2. Compute     |
//                                                        |   3. Output xfer |
//                                                        +------------------+
//                                                           10.1.2.2
//
//  Two-phase admission protocol:
//    1. Client sends ADMISSION_REQUEST with task metadata
//    2. Orchestrator responds with ADMISSION_RESPONSE (admit/reject)
//    3. If admitted, client sends full task data
//    4. Orchestrator dispatches to backend, collects result
//    5. Orchestrator sends sink task response back to client
//
// ===========================================================================

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistributedExample");

/**
 * Callback for when a task is sent by the client.
 *
 * @param task The task that was submitted.
 */
static void
TaskSent(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client] Task " << task->GetTaskId() << " sent"
                  << " (input=" << task->GetInputSize() / 1024.0 << " KB"
                  << ", compute=" << task->GetComputeDemand() / 1e9 << " GFLOP)");
}

/**
 * Callback for when a response is received by the client.
 *
 * @param task The completed task from the response.
 * @param rtt Round-trip time from submission to response.
 */
static void
ResponseReceived(Ptr<const Task> task, Time rtt)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client] Task " << task->GetTaskId() << " response received"
                  << " (RTT=" << rtt.As(Time::MS) << ")");
}

/**
 * Callback for when a task is received by the server.
 *
 * @param task The task that was received.
 */
static void
TaskReceived(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Server] Task " << task->GetTaskId() << " received");
}

/**
 * Callback for when a task starts processing on the GPU.
 *
 * @param task The task that started.
 */
static void
GpuTaskStarted(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [GPU] Task " << task->GetTaskId() << " started processing");
}

/**
 * Callback for when a task completes processing on the GPU.
 *
 * @param task The completed task.
 * @param duration Processing duration.
 */
static void
GpuTaskCompleted(Ptr<const Task> task, Time duration)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [GPU] Task " << task->GetTaskId() << " completed"
                  << " (processing=" << duration.As(Time::MS) << ")");
}

/**
 * Callback for when the server sends a response.
 *
 * @param task The completed task.
 * @param duration The processing duration.
 */
static void
ServerTaskCompleted(Ptr<const Task> task, Time duration)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Server] Task " << task->GetTaskId() << " response sent");
}

int
main(int argc, char* argv[])
{
    // Default parameters
    std::string dataRate = "100Mbps";
    std::string delay = "5ms";
    double simTime = 2.0;
    uint64_t numTasks = 5;
    double meanInterArrival = 0.05;
    double meanComputeDemand = 5e9;
    double meanInputSize = 1e5;
    double meanOutputSize = 1e4;
    double computeRate = 1e12;
    double memoryBandwidth = 900e9;
    double gpuVoltage = 1.0;
    double gpuFrequency = 1.5e9;
    double staticPower = 30.0;
    double effectiveCapacitance = 2e-9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Link data rate", dataRate);
    cmd.AddValue("delay", "Link delay", delay);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("numTasks", "Number of tasks to generate", numTasks);
    cmd.AddValue("meanInterArrival", "Mean task inter-arrival time in seconds", meanInterArrival);
    cmd.AddValue("meanComputeDemand", "Mean compute demand in FLOPS", meanComputeDemand);
    cmd.AddValue("meanInputSize", "Mean input data size in bytes", meanInputSize);
    cmd.AddValue("meanOutputSize", "Mean output data size in bytes", meanOutputSize);
    cmd.AddValue("computeRate", "GPU compute rate in FLOPS", computeRate);
    cmd.AddValue("memoryBandwidth", "GPU memory bandwidth in bytes/sec", memoryBandwidth);
    cmd.AddValue("gpuVoltage", "GPU operating voltage in Volts", gpuVoltage);
    cmd.AddValue("gpuFrequency", "GPU operating frequency in Hz", gpuFrequency);
    cmd.AddValue("staticPower", "GPU static power in Watts", staticPower);
    cmd.AddValue("effectiveCapacitance",
                 "GPU effective capacitance in Farads",
                 effectiveCapacitance);
    cmd.Parse(argc, argv);

    NS_LOG_UNCOND("Single-Client Distributed Computing Example");
    NS_LOG_UNCOND("Topology: Client → Orchestrator → Server");
    NS_LOG_UNCOND("Number of Tasks: " << numTasks);
    NS_LOG_UNCOND("");

    // Create nodes: client (n0), orchestrator (n1), server (n2)
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    // Client ↔ Orchestrator
    NetDeviceContainer devClientOrch = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    // Orchestrator ↔ Server
    NetDeviceContainer devOrchServer = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifClientOrch = address.Assign(devClientOrch);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifOrchServer = address.Assign(devOrchServer);

    // Create processing model and queue scheduler
    Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
    Ptr<FifoQueueScheduler> queueScheduler = CreateObject<FifoQueueScheduler>();

    // Create energy model for GPU
    Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
    energyModel->SetAttribute("StaticPower", DoubleValue(staticPower));
    energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(effectiveCapacitance));

    // Create GPU accelerator and aggregate to server node
    Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
    gpu->SetAttribute("ComputeRate", DoubleValue(computeRate));
    gpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));
    gpu->SetAttribute("Voltage", DoubleValue(gpuVoltage));
    gpu->SetAttribute("Frequency", DoubleValue(gpuFrequency));
    gpu->SetAttribute("ProcessingModel", PointerValue(model));
    gpu->SetAttribute("QueueScheduler", PointerValue(queueScheduler));
    gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
    nodes.Get(2)->AggregateObject(gpu);

    // Connect GPU trace sources
    gpu->TraceConnectWithoutContext("TaskStarted", MakeCallback(&GpuTaskStarted));
    gpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&GpuTaskCompleted));

    // Install OffloadServer on server node (n2)
    uint16_t serverPort = 9000;
    OffloadServerHelper serverHelper(serverPort);
    ApplicationContainer serverApps = serverHelper.Install(nodes.Get(2));

    Ptr<OffloadServer> server = DynamicCast<OffloadServer>(serverApps.Get(0));
    server->TraceConnectWithoutContext("TaskReceived", MakeCallback(&TaskReceived));
    server->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&ServerTaskCompleted));

    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    // Set up Cluster with one backend
    Cluster cluster;
    cluster.AddBackend(nodes.Get(2), InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

    // Set up EdgeOrchestrator on node 1
    Ptr<FirstFitScheduler> scheduler = CreateObject<FirstFitScheduler>();
    Ptr<AlwaysAdmitPolicy> policy = CreateObject<AlwaysAdmitPolicy>();

    uint16_t orchPort = 8080;
    Ptr<EdgeOrchestrator> orchestrator = CreateObject<EdgeOrchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
    orchestrator->SetAttribute("AdmissionPolicy", PointerValue(policy));
    orchestrator->SetCluster(cluster);
    nodes.Get(1)->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(simTime + 1.0));

    // Install OffloadClient on client node (n0) — connects to orchestrator
    OffloadClientHelper clientHelper(InetSocketAddress(ifClientOrch.GetAddress(1), orchPort));
    clientHelper.SetMeanInterArrival(meanInterArrival);
    clientHelper.SetMeanComputeDemand(meanComputeDemand);
    clientHelper.SetMeanInputSize(meanInputSize);
    clientHelper.SetMeanOutputSize(meanOutputSize);
    clientHelper.SetMaxTasks(numTasks);

    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));

    Ptr<OffloadClient> client = DynamicCast<OffloadClient>(clientApps.Get(0));
    client->TraceConnectWithoutContext("TaskSent", MakeCallback(&TaskSent));
    client->TraceConnectWithoutContext("ResponseReceived", MakeCallback(&ResponseReceived));

    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Summary ===");
    NS_LOG_UNCOND("Tasks sent:          " << client->GetTasksSent());
    NS_LOG_UNCOND("Responses received:  " << client->GetResponsesReceived());
    NS_LOG_UNCOND("Workloads admitted:  " << orchestrator->GetWorkloadsAdmitted());
    NS_LOG_UNCOND("Workloads completed: " << orchestrator->GetWorkloadsCompleted());
    NS_LOG_UNCOND("Tasks processed:     " << server->GetTasksCompleted());
    NS_LOG_UNCOND("Client TX bytes:     " << client->GetTotalTx());
    NS_LOG_UNCOND("Client RX bytes:     " << client->GetTotalRx());
    NS_LOG_UNCOND("Server RX bytes:     " << server->GetTotalRx());
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Energy ===");
    NS_LOG_UNCOND("Total energy:        " << gpu->GetTotalEnergy() << " J");
    NS_LOG_UNCOND("Final power:         " << gpu->GetCurrentPower() << " W");

    Simulator::Destroy();

    return 0;
}
