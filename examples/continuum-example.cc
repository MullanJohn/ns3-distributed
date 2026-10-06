/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/always-submit-policy.h"
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
#include "ns3/network-aware-scheduler.h"
#include "ns3/network-module.h"
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/orchestrator.h"
#include "ns3/point-to-point-module.h"
#include "ns3/task.h"

// ===========================================================================
//
//  Computing Continuum Simulation Example
//
//  Demonstrates periodic task offloading from a client through an
//  Orchestrator to one of two backends using the two-phase admission
//  protocol. The two backends model an edge/cloud deployment: the "edge"
//  backend sits on a short-delay link, the "cloud" backend on a longer one.
//  NetworkAwareScheduler factors that difference into placement, favoring
//  the edge backend unless it is loaded enough that the cloud backend's
//  shorter estimated completion time wins despite the extra network cost
//  (try raising --taskRate or --computeDemand to see the spillover).
//
//  Network topology:
//
//                                   ┌─P2P (delay)──── Edge server
//    Client ──P2P (delay)── Orchestrator
//                                   └─P2P (cloudDelay)── Cloud server
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
WorkloadProcessed(Ptr<const DagTask> dag, Time latency)
{
    Time computeTime = Seconds(0);
    for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
    {
        Ptr<Task> task = dag->GetTask(i);
        if (task)
        {
            computeTime += task->GetComputeTime();
        }
    }

    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Client] Workload response received (" << dag->GetTaskCount()
                  << " tasks, latency=" << latency.As(Time::MS)
                  << ", compute=" << computeTime.As(Time::MS) << ")");
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

static void
TaskDispatchedToBackend(uint64_t workloadId, uint64_t taskId, uint32_t backendIdx)
{
    NS_LOG_UNCOND(Simulator::Now().As(Time::S)
                  << " [Orchestrator] Task " << taskId << " dispatched to "
                  << (backendIdx == 0 ? "edge" : "cloud") << " backend (index " << backendIdx
                  << ")");
}

int
main(int argc, char* argv[])
{
    std::string dataRate = "100Mbps";
    std::string delay = "5ms";
    std::string cloudDelay = "50ms";
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
    cmd.AddValue("delay", "Client-orchestrator and orchestrator-edge link delay", delay);
    cmd.AddValue("cloudDelay", "Orchestrator-cloud link delay", cloudDelay);
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
    NS_LOG_UNCOND("Topology: Client -> Orchestrator -> {Edge, Cloud} backend");
    NS_LOG_UNCOND("Task rate: " << taskRate << " Hz");
    NS_LOG_UNCOND("");

    NodeContainer nodes;
    nodes.Create(4);
    Ptr<Node> clientNode = nodes.Get(0);
    Ptr<Node> orchNode = nodes.Get(1);
    Ptr<Node> edgeNode = nodes.Get(2);
    Ptr<Node> cloudNode = nodes.Get(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    PointToPointHelper cloudPointToPoint;
    cloudPointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    cloudPointToPoint.SetChannelAttribute("Delay", StringValue(cloudDelay));

    NetDeviceContainer devClientOrch = pointToPoint.Install(clientNode, orchNode);
    NetDeviceContainer devOrchEdge = pointToPoint.Install(orchNode, edgeNode);
    NetDeviceContainer devOrchCloud = cloudPointToPoint.Install(orchNode, cloudNode);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifClientOrch = address.Assign(devClientOrch);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifOrchEdge = address.Assign(devOrchEdge);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifOrchCloud = address.Assign(devOrchCloud);

    // Static commCapacity for both backends comes straight from the shared link
    // data rate; NetworkAwareScheduler reads capacity in bytes/sec.
    double linkCapacityBytesPerSec = DataRate(dataRate).GetBitRate() / 8.0;

    Ptr<GpuAccelerator> edgeGpu = CreateObject<GpuAccelerator>();
    Ptr<GpuAccelerator> cloudGpu = CreateObject<GpuAccelerator>();
    for (Ptr<GpuAccelerator> gpu : {edgeGpu, cloudGpu})
    {
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> queueScheduler = CreateObject<FifoQueueScheduler>();

        Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
        energyModel->SetAttribute("StaticPower", DoubleValue(staticPower));
        energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(effectiveCapacitance));

        gpu->SetAttribute("ComputeRate", DoubleValue(computeRate));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(transferBandwidth));
        gpu->SetAttribute("Voltage", DoubleValue(gpuVoltage));
        gpu->SetAttribute("Frequency", DoubleValue(gpuFrequency));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueScheduler));
        gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
    }
    edgeNode->AggregateObject(edgeGpu);
    cloudNode->AggregateObject(cloudGpu);

    edgeGpu->TraceConnectWithoutContext("TaskStarted", MakeCallback(&GpuTaskStarted));
    edgeGpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&GpuTaskCompleted));
    cloudGpu->TraceConnectWithoutContext("TaskStarted", MakeCallback(&GpuTaskStarted));
    cloudGpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&GpuTaskCompleted));

    uint16_t serverPort = 9000;
    BackendServerHelper serverHelper(serverPort);
    ApplicationContainer serverApps = serverHelper.Install(NodeContainer(edgeNode, cloudNode));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    Cluster cluster;
    // Backend 0 ("edge"): short delay, matches the client-orchestrator link.
    cluster.AddBackend(edgeNode,
                       InetSocketAddress(ifOrchEdge.GetAddress(1), serverPort),
                       "",
                       Time(delay),
                       linkCapacityBytesPerSec);
    // Backend 1 ("cloud"): longer delay.
    cluster.AddBackend(cloudNode,
                       InetSocketAddress(ifOrchCloud.GetAddress(1), serverPort),
                       "",
                       Time(cloudDelay),
                       linkCapacityBytesPerSec);

    Ptr<NetworkAwareScheduler> scheduler = CreateObject<NetworkAwareScheduler>();
    Ptr<MaxActiveTasksPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
    policy->SetAttribute("MaxActiveTasks", UintegerValue(8));

    uint16_t orchPort = 8080;
    Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
    orchestrator->SetAttribute("AdmissionPolicy", PointerValue(policy));
    orchestrator->SetCluster(cluster);
    orchNode->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(simTime + 1.0));

    orchestrator->TraceConnectWithoutContext("TaskDispatched",
                                             MakeCallback(&TaskDispatchedToBackend));

    OffloadClientHelper clientHelper(InetSocketAddress(ifClientOrch.GetAddress(1), orchPort));
    clientHelper.SetTaskRate(taskRate);

    Ptr<ConstantRandomVariable> inputSizeRv = CreateObject<ConstantRandomVariable>();
    inputSizeRv->SetAttribute("Constant", DoubleValue(meanInputSize));
    Ptr<ConstantRandomVariable> computeDemandRv = CreateObject<ConstantRandomVariable>();
    computeDemandRv->SetAttribute("Constant", DoubleValue(computeDemand));
    Ptr<ConstantRandomVariable> outputSizeRv = CreateObject<ConstantRandomVariable>();
    outputSizeRv->SetAttribute("Constant", DoubleValue(outputSize));
    clientHelper.SetLinearChainDagTemplate(1, inputSizeRv, computeDemandRv, outputSizeRv);

    // AlwaysSubmitPolicy (rather than the default DropWhilePendingSubmissionPolicy)
    // lets multiple tasks queue up on the edge backend, so a high --taskRate
    // actually builds enough estimated compute backlog for NetworkAwareScheduler
    // to spill tasks over to the cloud backend despite its longer link delay.
    clientHelper.SetSubmissionPolicy(CreateObject<AlwaysSubmitPolicy>());

    ApplicationContainer clientApps = clientHelper.Install(clientNode);

    Ptr<OffloadClient> client = DynamicCast<OffloadClient>(clientApps.Get(0));
    client->TraceConnectWithoutContext("TaskSent", MakeCallback(&TaskSent));
    client->TraceConnectWithoutContext("WorkloadProcessed", MakeCallback(&WorkloadProcessed));

    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    Ptr<BackendServer> edgeServer = DynamicCast<BackendServer>(serverApps.Get(0));
    Ptr<BackendServer> cloudServer = DynamicCast<BackendServer>(serverApps.Get(1));

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Summary ===");
    NS_LOG_UNCOND("Tasks sent:              " << client->GetTasksSent());
    NS_LOG_UNCOND("Responses received:      " << client->GetResponsesReceived());
    NS_LOG_UNCOND("Tasks dropped:           " << client->GetTasksDropped());
    NS_LOG_UNCOND("Workloads admitted:      " << orchestrator->GetWorkloadsAdmitted());
    NS_LOG_UNCOND("Workloads completed:     " << orchestrator->GetWorkloadsCompleted());
    NS_LOG_UNCOND("Tasks processed (edge):  " << edgeServer->GetTasksProcessed());
    NS_LOG_UNCOND("Tasks processed (cloud): " << cloudServer->GetTasksProcessed());
    NS_LOG_UNCOND("Client TX bytes:         " << client->GetTotalTx());
    NS_LOG_UNCOND("Client RX bytes:         " << client->GetTotalRx());
    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Energy ===");
    NS_LOG_UNCOND("Edge energy:             " << edgeGpu->GetTotalEnergy() << " J");
    NS_LOG_UNCOND("Cloud energy:            " << cloudGpu->GetTotalEnergy() << " J");

    Simulator::Destroy();

    return 0;
}
