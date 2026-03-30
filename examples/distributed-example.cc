/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/always-admit-policy.h"
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
#include "ns3/periodic-client-helper.h"
#include "ns3/periodic-client.h"
#include "ns3/periodic-server-helper.h"
#include "ns3/periodic-server.h"
#include "ns3/point-to-point-module.h"
#include "ns3/task.h"

// ===========================================================================
//
//  Distributed Computing Simulation Example
//
//  This example demonstrates periodic frame offloading from a client through
//  an EdgeOrchestrator to a backend using the two-phase admission protocol.
//
//  Network topology:
//
//       Client Node (n0)         Orchestrator (n1)         Backend (n2)
//      +---------------+       +-----------------+       +------------------+
//      |               |       |                 |       |                  |
//      |PeriodicClient | ----> | EdgeOrchestrator| ----> | PeriodicServer   |
//      | Sends frames  |       | Admission ctrl  |       |    Receives      |
//      | via admission |       | + scheduling    |       |      frames      |
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
// ===========================================================================

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistributedExample");

static void
FrameSent(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client] Frame " << task->GetTaskId() << " sent"
                  << " (input=" << task->GetInputSize() / 1024.0 << " KB"
                  << ", compute=" << task->GetComputeDemand() / 1e9 << " GFLOP)");
}

static void
FrameProcessed(Ptr<const Task> task, Time latency)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client] Frame " << task->GetTaskId() << " response received"
                  << " (latency=" << latency.As(Time::MS) << ")");
}

static void
GpuTaskStarted(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [GPU] Frame " << task->GetTaskId() << " started processing");
}

static void
GpuTaskCompleted(Ptr<const Task> task, Time duration)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [GPU] Frame " << task->GetTaskId() << " completed"
                  << " (processing=" << duration.As(Time::MS) << ")");
}

int
main(int argc, char* argv[])
{
    std::string dataRate = "100Mbps";
    std::string delay = "5ms";
    double simTime = 2.0;
    double frameRate = 10.0;
    double meanFrameSize = 1e5;
    double computeDemand = 5e9;
    double outputSize = 1e4;
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
    cmd.AddValue("frameRate", "Frames per second", frameRate);
    cmd.AddValue("meanFrameSize", "Mean frame size in bytes", meanFrameSize);
    cmd.AddValue("computeDemand", "Compute demand per frame in FLOPS", computeDemand);
    cmd.AddValue("outputSize", "Output size in bytes", outputSize);
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
    NS_LOG_UNCOND("Topology: Client -> Orchestrator -> Backend");
    NS_LOG_UNCOND("Frame rate: " << frameRate << " FPS");
    NS_LOG_UNCOND("");

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devClientOrch = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devOrchServer = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifClientOrch = address.Assign(devClientOrch);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifOrchServer = address.Assign(devOrchServer);

    Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
    Ptr<FifoQueueScheduler> queueScheduler = CreateObject<FifoQueueScheduler>();

    Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
    energyModel->SetAttribute("StaticPower", DoubleValue(staticPower));
    energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(effectiveCapacitance));

    Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
    gpu->SetAttribute("ComputeRate", DoubleValue(computeRate));
    gpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));
    gpu->SetAttribute("Voltage", DoubleValue(gpuVoltage));
    gpu->SetAttribute("Frequency", DoubleValue(gpuFrequency));
    gpu->SetAttribute("ProcessingModel", PointerValue(model));
    gpu->SetAttribute("QueueScheduler", PointerValue(queueScheduler));
    gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
    nodes.Get(2)->AggregateObject(gpu);

    gpu->TraceConnectWithoutContext("TaskStarted", MakeCallback(&GpuTaskStarted));
    gpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&GpuTaskCompleted));

    uint16_t serverPort = 9000;
    PeriodicServerHelper serverHelper(serverPort);
    ApplicationContainer serverApps = serverHelper.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    Cluster cluster;
    cluster.AddBackend(nodes.Get(2), InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

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

    PeriodicClientHelper clientHelper(InetSocketAddress(ifClientOrch.GetAddress(1), orchPort));
    clientHelper.SetFrameRate(frameRate);
    clientHelper.SetMeanFrameSize(meanFrameSize);
    clientHelper.SetComputeDemand(computeDemand);
    clientHelper.SetOutputSize(outputSize);

    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));

    Ptr<PeriodicClient> client = DynamicCast<PeriodicClient>(clientApps.Get(0));
    client->TraceConnectWithoutContext("FrameSent", MakeCallback(&FrameSent));
    client->TraceConnectWithoutContext("FrameProcessed", MakeCallback(&FrameProcessed));

    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    Ptr<PeriodicServer> server = DynamicCast<PeriodicServer>(serverApps.Get(0));

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Summary ===");
    NS_LOG_UNCOND("Frames sent:         " << client->GetFramesSent());
    NS_LOG_UNCOND("Responses received:  " << client->GetResponsesReceived());
    NS_LOG_UNCOND("Frames dropped:      " << client->GetFramesDropped());
    NS_LOG_UNCOND("Workloads admitted:  " << orchestrator->GetWorkloadsAdmitted());
    NS_LOG_UNCOND("Workloads completed: " << orchestrator->GetWorkloadsCompleted());
    NS_LOG_UNCOND("Frames processed:    " << server->GetFramesProcessed());
    NS_LOG_UNCOND("Client TX bytes:     " << client->GetTotalTx());
    NS_LOG_UNCOND("Client RX bytes:     " << client->GetTotalRx());
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Energy ===");
    NS_LOG_UNCOND("Total energy:        " << gpu->GetTotalEnergy() << " J");
    NS_LOG_UNCOND("Final power:         " << gpu->GetCurrentPower() << " W");

    Simulator::Destroy();

    return 0;
}
