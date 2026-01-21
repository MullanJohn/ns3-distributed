/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster.h"
#include "ns3/core-module.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/internet-module.h"
#include "ns3/load-balancer-helper.h"
#include "ns3/load-balancer.h"
#include "ns3/network-module.h"
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/offload-server-helper.h"
#include "ns3/offload-server.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simple-task-header.h"

// ===========================================================================
//
//  Load Balancer Example
//
//  This example demonstrates load balancing task offloading across multiple
//  backend servers. Multiple clients connect to a single load balancer, which
//  distributes tasks across backend servers using round-robin scheduling.
//
//  Network Topology:
//                                                 +----------+
//     +-----------+                               | Server 0 |
//     | Client 0  |---+                       +---| + GPU    |
//     | (Offload  |   |   +---------------+   |   +----------+
//     |  Client)  |   |   |  LoadBalancer |---+   +----------+
//     +-----------+   +-->|               |       | Server 1 |
//                         | (Round-Robin  |-------|    +     |
//     +-----------+   +-->|  Scheduler)   |       |   GPU    |
//     | Client 1  |   |   |               |---+   +----------+
//     | (Offload  |   |   +---------------+   |   +----------+
//     |  Client)  |---+                       +---| Server 2 |
//     +-----------+                               | + GPU    |
//                                                 +----------+
//
//  Message Flow:
//
//     Client          LoadBalancer              Server
//        |                 |                       |
//        | TASK_REQUEST    |                       |
//        |---------------->| SelectBackend()       |
//        |                 | (round-robin: 0,1,2)  |
//        |                 |                       |
//        |                 | TASK_REQUEST          |
//        |                 |---------------------->|
//        |                 |                       | GpuAccelerator
//        |                 |                       | processes task
//        |                 |    TASK_RESPONSE      |
//        |                 |<----------------------|
//        |  TASK_RESPONSE  |                       |
//        |<----------------|                       |
//        |                 |                       |
//
//  The LoadBalancer uses ConnectionManagers for transport and maintains a
//  mapping of taskId -> clientAddress to route responses back correctly.
//
// ===========================================================================

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LoadBalancerExample");

static void
TaskSent(uint32_t clientId, const SimpleTaskHeader& header)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client " << clientId << "] Task " << header.GetTaskId() << " sent"
                  << " (input=" << header.GetInputSize() / 1024.0 << " KB)");
}

static void
ResponseReceived(uint32_t clientId, const SimpleTaskHeader& header, Time rtt)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client " << clientId << "] Task " << header.GetTaskId()
                  << " response (RTT=" << rtt.As(Time::MS) << ")");
}

static void
TaskForwarded(const TaskHeader& header, uint32_t backendIndex)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S) << " [LoadBalancer] Task " << header.GetTaskId()
                                               << " -> Backend " << backendIndex);
}

static void
ResponseRouted(const TaskHeader& header, Time latency)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [LoadBalancer] Task " << header.GetTaskId()
                  << " response routed (latency=" << latency.As(Time::MS) << ")");
}

static void
TaskReceived(uint32_t serverId, const SimpleTaskHeader& header)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Server " << serverId << "] Task " << header.GetTaskId() << " received");
}

static void
ServerTaskCompleted(uint32_t serverId, const SimpleTaskHeader& header, Time duration)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Server " << serverId << "] Task " << header.GetTaskId()
                  << " completed (processing=" << duration.As(Time::MS) << ")");
}

int
main(int argc, char* argv[])
{
    // Default parameters
    std::string dataRate = "100Mbps";
    std::string delay = "2ms";
    double simTime = 3.0;
    uint32_t numClients = 2;
    uint32_t numServers = 3;
    uint64_t tasksPerClient = 4;
    double meanInterArrival = 0.1;
    double meanComputeDemand = 5e9;
    double meanInputSize = 1e5;
    double meanOutputSize = 1e4;
    double computeRate = 1e12;
    double memoryBandwidth = 900e9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Link data rate", dataRate);
    cmd.AddValue("delay", "Link delay", delay);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("numClients", "Number of client nodes", numClients);
    cmd.AddValue("numServers", "Number of backend servers", numServers);
    cmd.AddValue("tasksPerClient", "Tasks per client", tasksPerClient);
    cmd.AddValue("meanInterArrival", "Mean task inter-arrival time", meanInterArrival);
    cmd.AddValue("meanComputeDemand", "Mean compute demand in FLOPS", meanComputeDemand);
    cmd.AddValue("meanInputSize", "Mean input size in bytes", meanInputSize);
    cmd.AddValue("meanOutputSize", "Mean output size in bytes", meanOutputSize);
    cmd.AddValue("computeRate", "GPU compute rate in FLOPS", computeRate);
    cmd.AddValue("memoryBandwidth", "GPU memory bandwidth in bytes/sec", memoryBandwidth);
    cmd.Parse(argc, argv);

    NS_LOG_UNCOND("Load Balancer Example");
    NS_LOG_UNCOND("Clients: " << numClients << ", Servers: " << numServers);
    NS_LOG_UNCOND("Tasks per client: " << tasksPerClient);
    NS_LOG_UNCOND("");

    // Create nodes
    NodeContainer clientNodes;
    clientNodes.Create(numClients);

    NodeContainer lbNode;
    lbNode.Create(1);

    NodeContainer serverNodes;
    serverNodes.Create(numServers);

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(clientNodes);
    stack.Install(lbNode);
    stack.Install(serverNodes);

    // Connect clients to load balancer (10.1.x.0/24 subnets)
    Ipv4Address lbFrontendAddr;
    for (uint32_t i = 0; i < numClients; ++i)
    {
        NetDeviceContainer devices = pointToPoint.Install(clientNodes.Get(i), lbNode.Get(0));

        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";

        Ipv4AddressHelper address;
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        if (i == 0)
        {
            lbFrontendAddr = interfaces.GetAddress(1);
        }
    }

    // Connect load balancer to servers (10.2.x.0/24 subnets)
    std::vector<Ipv4Address> serverAddrs(numServers);
    for (uint32_t i = 0; i < numServers; ++i)
    {
        NetDeviceContainer devices = pointToPoint.Install(lbNode.Get(0), serverNodes.Get(i));

        std::ostringstream subnet;
        subnet << "10.2." << (i + 1) << ".0";

        Ipv4AddressHelper address;
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        serverAddrs[i] = interfaces.GetAddress(1);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create shared processing model for GPU compute time calculation
    Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();

    // Setup servers with GPU accelerators
    uint16_t serverPort = 9000;
    std::vector<Ptr<OffloadServer>> servers(numServers);
    std::vector<Ptr<GpuAccelerator>> gpus(numServers);

    for (uint32_t i = 0; i < numServers; ++i)
    {
        // Create energy model for this GPU
        Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
        energyModel->SetAttribute("StaticPower", DoubleValue(30.0));
        energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(2e-9));

        // Create and attach GPU accelerator
        Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(computeRate));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));
        gpu->SetAttribute("Voltage", DoubleValue(1.0));
        gpu->SetAttribute("Frequency", DoubleValue(1.5e9));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
        gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
        serverNodes.Get(i)->AggregateObject(gpu);
        gpus[i] = gpu;

        // Install server application
        OffloadServerHelper serverHelper(serverPort);
        ApplicationContainer serverApps = serverHelper.Install(serverNodes.Get(i));

        servers[i] = DynamicCast<OffloadServer>(serverApps.Get(0));
        servers[i]->TraceConnectWithoutContext("TaskReceived", MakeBoundCallback(&TaskReceived, i));
        servers[i]->TraceConnectWithoutContext("TaskCompleted",
                                               MakeBoundCallback(&ServerTaskCompleted, i));

        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simTime + 2.0));
    }

    // Create cluster of backend servers for load balancer
    Cluster cluster;
    for (uint32_t i = 0; i < numServers; ++i)
    {
        cluster.AddBackend(serverNodes.Get(i), InetSocketAddress(serverAddrs[i], serverPort));
    }

    // Install LoadBalancer with round-robin scheduling
    uint16_t lbPort = 8000;
    LoadBalancerHelper lbHelper(lbPort);
    lbHelper.SetCluster(cluster);
    lbHelper.SetScheduler("ns3::RoundRobinScheduler");

    ApplicationContainer lbApps = lbHelper.Install(lbNode.Get(0));
    Ptr<LoadBalancer> lb = DynamicCast<LoadBalancer>(lbApps.Get(0));
    lb->TraceConnectWithoutContext("TaskForwarded", MakeCallback(&TaskForwarded));
    lb->TraceConnectWithoutContext("ResponseRouted", MakeCallback(&ResponseRouted));

    lbApps.Start(Seconds(0.0));
    lbApps.Stop(Seconds(simTime + 2.0));

    // Install clients
    std::vector<Ptr<OffloadClient>> clients(numClients);
    for (uint32_t i = 0; i < numClients; ++i)
    {
        OffloadClientHelper clientHelper(InetSocketAddress(lbFrontendAddr, lbPort));
        clientHelper.SetMeanInterArrival(meanInterArrival);
        clientHelper.SetMeanComputeDemand(meanComputeDemand);
        clientHelper.SetMeanInputSize(meanInputSize);
        clientHelper.SetMeanOutputSize(meanOutputSize);
        clientHelper.SetMaxTasks(tasksPerClient);

        ApplicationContainer clientApps = clientHelper.Install(clientNodes.Get(i));

        clients[i] = DynamicCast<OffloadClient>(clientApps.Get(0));
        clients[i]->TraceConnectWithoutContext("TaskSent", MakeBoundCallback(&TaskSent, i));
        clients[i]->TraceConnectWithoutContext("ResponseReceived",
                                               MakeBoundCallback(&ResponseReceived, i));

        // Stagger client starts
        clientApps.Start(Seconds(0.1 + i * 0.05));
        clientApps.Stop(Seconds(simTime));
    }

    // Run simulation
    Simulator::Stop(Seconds(simTime + 3.0));
    Simulator::Run();

    // Print summary
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Summary ===");

    uint64_t totalTasks = 0;
    uint64_t totalResponses = 0;

    for (uint32_t i = 0; i < numClients; ++i)
    {
        NS_LOG_UNCOND("Client " << i << ": sent=" << clients[i]->GetTasksSent()
                                << ", responses=" << clients[i]->GetResponsesReceived() << ", TX="
                                << clients[i]->GetTotalTx() << ", RX=" << clients[i]->GetTotalRx());
        totalTasks += clients[i]->GetTasksSent();
        totalResponses += clients[i]->GetResponsesReceived();
    }

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("LoadBalancer: forwarded="
                  << lb->GetTasksForwarded() << ", routed=" << lb->GetResponsesRouted()
                  << ", clientRx=" << lb->GetClientRx() << ", backendRx=" << lb->GetBackendRx());

    NS_LOG_UNCOND("");
    double totalEnergy = 0;
    for (uint32_t i = 0; i < numServers; ++i)
    {
        double energy = gpus[i]->GetTotalEnergy();
        totalEnergy += energy;
        NS_LOG_UNCOND("Server " << i << ": completed=" << servers[i]->GetTasksCompleted() << ", RX="
                                << servers[i]->GetTotalRx() << ", energy=" << energy << "J");
    }

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("Total: tasks=" << totalTasks << ", responses=" << totalResponses);
    NS_LOG_UNCOND("Total energy: " << totalEnergy << " J");

    Simulator::Destroy();

    return 0;
}
