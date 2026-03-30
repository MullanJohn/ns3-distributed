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
//  Multi-Client Distributed Computing Simulation Example
//
//  This example demonstrates multiple clients offloading periodic frames
//  through a single EdgeOrchestrator to a backend.
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
//                      Backend (n4)
//                   +------------------+
//                   | PeriodicServer   |
//                   |        |         |
//                   |        v         |
//                   |  GpuAccelerator  |
//                   +------------------+
//
// ===========================================================================

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistributedMultiClientExample");

static void
FrameSent(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client] Frame " << task->GetTaskId() << " sent"
                  << " (input=" << task->GetInputSize() / 1024.0 << " KB)");
}

static void
FrameProcessed(Ptr<const Task> task, Time latency)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client] Frame " << task->GetTaskId()
                  << " response (latency=" << latency.As(Time::MS) << ")");
}

static void
GpuTaskCompleted(Ptr<const Task> task, Time duration)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [GPU] Frame " << task->GetTaskId()
                  << " completed (processing=" << duration.As(Time::MS) << ")");
}

int
main(int argc, char* argv[])
{
    uint32_t numClients = 3;
    std::string dataRate = "100Mbps";
    std::string delay = "2ms";
    double simTime = 2.0;
    double frameRate = 5.0;
    double meanFrameSize = 5e4;
    double computeDemand = 1e9;
    double outputSize = 1e4;
    double computeRate = 1e12;
    double memoryBandwidth = 900e9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numClients", "Number of client nodes", numClients);
    cmd.AddValue("dataRate", "Link data rate", dataRate);
    cmd.AddValue("delay", "Link delay", delay);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("frameRate", "Frames per second per client", frameRate);
    cmd.AddValue("meanFrameSize", "Mean frame size in bytes", meanFrameSize);
    cmd.AddValue("computeDemand", "Compute demand per frame in FLOPS", computeDemand);
    cmd.AddValue("outputSize", "Output size in bytes", outputSize);
    cmd.AddValue("computeRate", "GPU compute rate in FLOPS", computeRate);
    cmd.AddValue("memoryBandwidth", "GPU memory bandwidth in bytes/sec", memoryBandwidth);
    cmd.Parse(argc, argv);

    NS_LOG_UNCOND("Multi-Client Distributed Computing Example");
    NS_LOG_UNCOND("Topology: Clients -> Orchestrator -> Backend");
    NS_LOG_UNCOND("Clients: " << numClients << ", Frame rate: " << frameRate << " FPS");
    NS_LOG_UNCOND("");

    NodeContainer clientNodes;
    clientNodes.Create(numClients);

    NodeContainer orchNode;
    orchNode.Create(1);

    NodeContainer serverNode;
    serverNode.Create(1);

    InternetStackHelper stack;
    stack.Install(clientNodes);
    stack.Install(orchNode);
    stack.Install(serverNode);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    Ipv4AddressHelper address;
    std::vector<Ipv4Address> orchAddresses;

    for (uint32_t i = 0; i < numClients; i++)
    {
        NetDeviceContainer devices = pointToPoint.Install(clientNodes.Get(i), orchNode.Get(0));
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        orchAddresses.push_back(interfaces.GetAddress(1));
    }

    NetDeviceContainer devOrchServer = pointToPoint.Install(orchNode.Get(0), serverNode.Get(0));
    std::ostringstream serverSubnet;
    serverSubnet << "10.1." << (numClients + 1) << ".0";
    address.SetBase(serverSubnet.str().c_str(), "255.255.255.0");
    Ipv4InterfaceContainer ifOrchServer = address.Assign(devOrchServer);

    Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
    Ptr<FifoQueueScheduler> queueScheduler = CreateObject<FifoQueueScheduler>();

    Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
    energyModel->SetAttribute("StaticPower", DoubleValue(30.0));
    energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(2e-9));

    Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
    gpu->SetAttribute("ComputeRate", DoubleValue(computeRate));
    gpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));
    gpu->SetAttribute("Voltage", DoubleValue(1.0));
    gpu->SetAttribute("Frequency", DoubleValue(1.5e9));
    gpu->SetAttribute("ProcessingModel", PointerValue(model));
    gpu->SetAttribute("QueueScheduler", PointerValue(queueScheduler));
    gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
    serverNode.Get(0)->AggregateObject(gpu);

    gpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&GpuTaskCompleted));

    uint16_t serverPort = 9000;
    PeriodicServerHelper serverHelper(serverPort);
    ApplicationContainer serverApps = serverHelper.Install(serverNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    Cluster cluster;
    cluster.AddBackend(serverNode.Get(0),
                       InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

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

    std::vector<Ptr<PeriodicClient>> clients;

    for (uint32_t i = 0; i < numClients; i++)
    {
        PeriodicClientHelper clientHelper(InetSocketAddress(orchAddresses[i], orchPort));
        clientHelper.SetFrameRate(frameRate);
        clientHelper.SetMeanFrameSize(meanFrameSize);
        clientHelper.SetComputeDemand(computeDemand);
        clientHelper.SetOutputSize(outputSize);

        ApplicationContainer clientApps = clientHelper.Install(clientNodes.Get(i));

        Ptr<PeriodicClient> client = DynamicCast<PeriodicClient>(clientApps.Get(0));
        client->TraceConnectWithoutContext("FrameSent", MakeCallback(&FrameSent));
        client->TraceConnectWithoutContext("FrameProcessed", MakeCallback(&FrameProcessed));

        clients.push_back(client);

        clientApps.Start(Seconds(0.1 + i * 0.01));
        clientApps.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    Ptr<PeriodicServer> server = DynamicCast<PeriodicServer>(serverApps.Get(0));

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Summary ===");

    uint64_t totalFramesSent = 0;
    uint64_t totalResponsesReceived = 0;
    uint64_t totalClientTx = 0;
    uint64_t totalClientRx = 0;

    for (uint32_t i = 0; i < numClients; i++)
    {
        NS_LOG_UNCOND("Client " << i << ": sent=" << clients[i]->GetFramesSent()
                                << ", responses=" << clients[i]->GetResponsesReceived()
                                << ", dropped=" << clients[i]->GetFramesDropped()
                                << ", TX=" << clients[i]->GetTotalTx() << " bytes"
                                << ", RX=" << clients[i]->GetTotalRx() << " bytes");
        totalFramesSent += clients[i]->GetFramesSent();
        totalResponsesReceived += clients[i]->GetResponsesReceived();
        totalClientTx += clients[i]->GetTotalTx();
        totalClientRx += clients[i]->GetTotalRx();
    }

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("Total frames sent:     " << totalFramesSent);
    NS_LOG_UNCOND("Total responses:       " << totalResponsesReceived);
    NS_LOG_UNCOND("Workloads admitted:    " << orchestrator->GetWorkloadsAdmitted());
    NS_LOG_UNCOND("Workloads completed:   " << orchestrator->GetWorkloadsCompleted());
    NS_LOG_UNCOND("Frames processed:      " << server->GetFramesProcessed());
    NS_LOG_UNCOND("Server RX bytes:       " << server->GetTotalRx());
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Energy ===");
    NS_LOG_UNCOND("Total energy:          " << gpu->GetTotalEnergy() << " J");

    Simulator::Destroy();

    return 0;
}
