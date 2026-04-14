/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/backend-server-helper.h"
#include "ns3/backend-server.h"
#include "ns3/cluster.h"
#include "ns3/core-module.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/internet-module.h"
#include "ns3/max-active-tasks-policy.h"
#include "ns3/network-module.h"
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/orchestrator.h"
#include "ns3/point-to-point-module.h"
#include "ns3/round-robin-scheduler.h"
#include "ns3/task.h"

// ===========================================================================
//
//  Computing Continuum Simulation Example
//
//  Demonstrates periodic task offloading from a client through an
//  Orchestrator to a backend using the two-phase admission protocol.
//
//  Network topology:
//
//    Client ──P2P── Orchestrator ──P2P── Server
//
//  Each link is a dedicated point-to-point channel.
//
// ===========================================================================

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ContinuumExample");

static void
TaskSent(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client] Task " << task->GetTaskId() << " sent"
                  << " (input=" << task->GetInputSize() / 1024.0 << " KB"
                  << ", compute=" << task->GetComputeDemand() / 1e9 << " GFLOP)");
}

static void
TaskProcessed(Ptr<const Task> task, Time latency)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client] Task " << task->GetTaskId() << " response received"
                  << " (latency=" << latency.As(Time::MS) << ")");
}

static void
GpuTaskStarted(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [GPU] Task " << task->GetTaskId() << " started processing");
}

static void
GpuTaskCompleted(Ptr<const Task> task, Time duration)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [GPU] Task " << task->GetTaskId() << " completed"
                  << " (processing=" << duration.As(Time::MS) << ")");
}

int
main(int argc, char* argv[])
{
    std::string dataRate = "100Mbps";
    std::string delay = "5ms";
    double simTime = 2.0;
    double taskRate = 10.0;
    double meanInputSize = 1e5;
    double computeDemand = 5e9;
    double outputSize = 1e4;
    double computeRate = 1e12;
    double memoryBandwidth = 900e9;
    double transferBandwidth = 32e9;
    double gpuVoltage = 1.0;
    double gpuFrequency = 1.5e9;
    double staticPower = 30.0;
    double effectiveCapacitance = 2e-9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Link data rate", dataRate);
    cmd.AddValue("delay", "Link delay", delay);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("taskRate", "Tasks per second", taskRate);
    cmd.AddValue("meanInputSize", "Mean input size in bytes", meanInputSize);
    cmd.AddValue("computeDemand", "Compute demand per task in FLOPS", computeDemand);
    cmd.AddValue("outputSize", "Output size in bytes", outputSize);
    cmd.AddValue("computeRate", "GPU compute rate in FLOPS", computeRate);
    cmd.AddValue("memoryBandwidth", "GPU DRAM memory bandwidth in bytes/sec", memoryBandwidth);
    cmd.AddValue("transferBandwidth",
                 "Host-device transfer bandwidth in bytes/sec",
                 transferBandwidth);
    cmd.AddValue("gpuVoltage", "GPU operating voltage in Volts", gpuVoltage);
    cmd.AddValue("gpuFrequency", "GPU operating frequency in Hz", gpuFrequency);
    cmd.AddValue("staticPower", "GPU static power in Watts", staticPower);
    cmd.AddValue("effectiveCapacitance",
                 "GPU effective capacitance in Farads",
                 effectiveCapacitance);
    cmd.Parse(argc, argv);

    NS_LOG_UNCOND("Single-Client Computing Continuum Example");
    NS_LOG_UNCOND("Topology: Client -> Orchestrator -> Backend");
    NS_LOG_UNCOND("Task rate: " << taskRate << " Hz");
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
    gpu->SetAttribute("TransferBandwidth", DoubleValue(transferBandwidth));
    gpu->SetAttribute("Voltage", DoubleValue(gpuVoltage));
    gpu->SetAttribute("Frequency", DoubleValue(gpuFrequency));
    gpu->SetAttribute("ProcessingModel", PointerValue(model));
    gpu->SetAttribute("QueueScheduler", PointerValue(queueScheduler));
    gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
    nodes.Get(2)->AggregateObject(gpu);

    gpu->TraceConnectWithoutContext("TaskStarted", MakeCallback(&GpuTaskStarted));
    gpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&GpuTaskCompleted));

    uint16_t serverPort = 9000;
    BackendServerHelper serverHelper(serverPort);
    ApplicationContainer serverApps = serverHelper.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    Cluster cluster;
    cluster.AddBackend(nodes.Get(2), InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

    Ptr<RoundRobinScheduler> scheduler = CreateObject<RoundRobinScheduler>();
    Ptr<MaxActiveTasksPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
    policy->SetAttribute("MaxActiveTasks", UintegerValue(8));

    uint16_t orchPort = 8080;
    Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
    orchestrator->SetAttribute("AdmissionPolicy", PointerValue(policy));
    orchestrator->SetCluster(cluster);
    nodes.Get(1)->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(simTime + 1.0));

    OffloadClientHelper clientHelper(InetSocketAddress(ifClientOrch.GetAddress(1), orchPort));
    clientHelper.SetTaskRate(taskRate);
    clientHelper.SetMeanInputSize(meanInputSize);
    clientHelper.SetComputeDemand(computeDemand);
    clientHelper.SetOutputSize(outputSize);

    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));

    Ptr<OffloadClient> client = DynamicCast<OffloadClient>(clientApps.Get(0));
    client->TraceConnectWithoutContext("TaskSent", MakeCallback(&TaskSent));
    client->TraceConnectWithoutContext("TaskProcessed", MakeCallback(&TaskProcessed));

    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    Ptr<BackendServer> server = DynamicCast<BackendServer>(serverApps.Get(0));

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Summary ===");
    NS_LOG_UNCOND("Tasks sent:          " << client->GetTasksSent());
    NS_LOG_UNCOND("Responses received:  " << client->GetResponsesReceived());
    NS_LOG_UNCOND("Tasks dropped:       " << client->GetTasksDropped());
    NS_LOG_UNCOND("Workloads admitted:  " << orchestrator->GetWorkloadsAdmitted());
    NS_LOG_UNCOND("Workloads completed: " << orchestrator->GetWorkloadsCompleted());
    NS_LOG_UNCOND("Tasks processed:     " << server->GetTasksProcessed());
    NS_LOG_UNCOND("Client TX bytes:     " << client->GetTotalTx());
    NS_LOG_UNCOND("Client RX bytes:     " << client->GetTotalRx());
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Energy ===");
    NS_LOG_UNCOND("Total energy:        " << gpu->GetTotalEnergy() << " J");
    NS_LOG_UNCOND("Final power:         " << gpu->GetCurrentPower() << " W");

    Simulator::Destroy();

    return 0;
}
