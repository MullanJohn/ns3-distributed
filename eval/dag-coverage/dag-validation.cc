/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "dag-validation-client.h"

#include "ns3/cluster.h"
#include "ns3/conservative-scaling-policy.h"
#include "ns3/core-module.h"
#include "ns3/dag-task.h"
#include "ns3/device-manager.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/orchestrator.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/round-robin-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/gpu-device-protocol.h"
#include "ns3/internet-module.h"
#include "ns3/least-loaded-scheduler.h"
#include "ns3/max-active-tasks-policy.h"
#include "ns3/network-module.h"
#include "ns3/backend-server-helper.h"
#include "ns3/backend-server.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/simple-task.h"
#include "ns3/utilization-scaling-policy.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistributedDagValidation");

namespace
{

Ptr<SimpleTask>
MakeTask(uint64_t taskId, uint64_t inputSize, double computeDemand, uint64_t outputSize,
         Time deadline)
{
    Ptr<SimpleTask> t = CreateObject<SimpleTask>();
    t->SetTaskId(taskId);
    t->SetInputSize(inputSize);
    t->SetComputeDemand(computeDemand);
    t->SetOutputSize(outputSize);
    t->SetDeadline(deadline);
    return t;
}

Ptr<DagTask>
BuildDag(const std::string& shape, uint64_t inputSize, double computeDemand,
         uint64_t outputSize, Time deadline)
{
    Ptr<DagTask> dag = CreateObject<DagTask>();
    uint64_t base = 0x1000;

    if (shape == "single")
    {
        dag->AddTask(MakeTask(base | 0, inputSize, computeDemand, outputSize, deadline));
    }
    else if (shape == "chain")
    {
        dag->AddTask(MakeTask(base | 0, inputSize, computeDemand, outputSize, deadline));
        dag->AddTask(MakeTask(base | 1, inputSize, computeDemand, outputSize, deadline));
        dag->AddTask(MakeTask(base | 2, inputSize, computeDemand, outputSize, deadline));
        dag->AddTask(MakeTask(base | 3, inputSize, computeDemand, outputSize, deadline));
        dag->AddDependency(0, 1);
        dag->AddDependency(1, 2);
        dag->AddDependency(2, 3);
    }
    else if (shape == "forkjoin")
    {
        dag->AddTask(MakeTask(base | 0, inputSize, computeDemand, outputSize, deadline));
        dag->AddTask(MakeTask(base | 1, inputSize, computeDemand, outputSize, deadline));
        dag->AddTask(MakeTask(base | 2, inputSize, computeDemand, outputSize, deadline));
        dag->AddTask(MakeTask(base | 3, inputSize, computeDemand, outputSize, deadline));
        dag->AddDependency(0, 1);
        dag->AddDependency(0, 2);
        dag->AddDependency(1, 3);
        dag->AddDependency(2, 3);
    }
    else
    {
        NS_FATAL_ERROR("Unknown shape '" << shape << "'. Use single, chain, or forkjoin.");
    }

    return dag;
}

struct TaskEvent
{
    uint64_t workloadId{0};
    uint64_t taskId{0};
    uint32_t backendIdx{0};
    double dispatchMs{0.0};
    double completeMs{0.0};
};

std::unordered_map<uint64_t, TaskEvent> g_taskEvents;
std::vector<uint64_t> g_taskOrder;

double
ToMs(Time t)
{
    return t.GetSeconds() * 1000.0;
}

void
OnTaskDispatched(uint64_t workloadId, uint64_t taskId, uint32_t backendIdx)
{
    auto [it, inserted] = g_taskEvents.try_emplace(taskId);
    if (inserted)
    {
        g_taskOrder.push_back(taskId);
    }
    it->second.workloadId = workloadId;
    it->second.taskId = taskId;
    it->second.backendIdx = backendIdx;
    it->second.dispatchMs = ToMs(Simulator::Now());
}

void
OnTaskCompleted(uint64_t /* workloadId */, uint64_t taskId, uint32_t /* backendIdx */)
{
    auto it = g_taskEvents.find(taskId);
    if (it != g_taskEvents.end())
    {
        it->second.completeMs = ToMs(Simulator::Now());
    }
}

} // anonymous namespace

int
main(int argc, char* argv[])
{
    std::string shape = "chain";
    uint32_t nBackends = 2;
    double simTime = 3.0;
    uint64_t inputSize = 60000;
    double computeDemand = 8.7e9;
    uint64_t outputSize = 1000;
    double effectiveCapacitance = 1.94e-8;
    double deadlineBudgetMs = 1000.0;
    uint32_t maxActiveTasks = 16;
    std::string linkRate = "1Gbps";
    std::string linkDelay = "2us";
    std::string scheme = "LU-SG";
    std::string outputDir = "";
    uint32_t seed = 1;
    uint32_t runNumber = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("shape", "DAG shape: single, chain, forkjoin", shape);
    cmd.AddValue("nBackends", "Number of GPU backend servers", nBackends);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("inputSize", "Root task input size in bytes", inputSize);
    cmd.AddValue("computeDemand", "Compute demand per task in FLOPS", computeDemand);
    cmd.AddValue("outputSize", "Per-task output size in bytes", outputSize);
    cmd.AddValue("effectiveCapacitance", "DVFS effective capacitance", effectiveCapacitance);
    cmd.AddValue("deadlineBudgetMs", "Per-task deadline budget in ms", deadlineBudgetMs);
    cmd.AddValue("maxActiveTasks", "Admission policy per-backend capacity", maxActiveTasks);
    cmd.AddValue("linkRate", "P2P link data rate", linkRate);
    cmd.AddValue("linkDelay", "P2P link propagation delay", linkDelay);
    cmd.AddValue("scheme", "Scheduling scheme: RR-NS, LU-NS, LU-SG", scheme);
    cmd.AddValue("outputDir", "Directory for CSV output", outputDir);
    cmd.AddValue("seed", "RNG seed", seed);
    cmd.AddValue("runNumber", "RNG run number", runNumber);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(scheme != "RR-NS" && scheme != "LU-NS" && scheme != "LU-SG",
                    "Unknown scheme: " << scheme);

    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(runNumber);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(65));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(2097152));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(2097152));
    Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(1)));

    NodeContainer clientNode;
    clientNode.Create(1);
    NodeContainer orchNode;
    orchNode.Create(1);
    NodeContainer serverNodes;
    serverNodes.Create(nBackends);

    InternetStackHelper stack;
    stack.Install(clientNode);
    stack.Install(orchNode);
    stack.Install(serverNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(linkRate));
    p2p.SetChannelAttribute("Delay", StringValue(linkDelay));

    Ipv4AddressHelper ipv4;
    uint32_t subnetIdx = 1;

    NetDeviceContainer clientOrchDevs = p2p.Install(clientNode.Get(0), orchNode.Get(0));
    {
        std::ostringstream base;
        base << "10.0." << subnetIdx++ << ".0";
        ipv4.SetBase(base.str().c_str(), "255.255.255.0");
    }
    Ipv4InterfaceContainer clientOrchIfaces = ipv4.Assign(clientOrchDevs);

    std::vector<Ipv4InterfaceContainer> orchServerIfaces(nBackends);
    for (uint32_t i = 0; i < nBackends; i++)
    {
        NetDeviceContainer devs = p2p.Install(orchNode.Get(0), serverNodes.Get(i));
        std::ostringstream base;
        base << "10.0." << subnetIdx++ << ".0";
        ipv4.SetBase(base.str().c_str(), "255.255.255.0");
        orchServerIfaces[i] = ipv4.Assign(devs);
    }
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    const std::vector<OperatingPoint> rtx5060Opps = {
        {2.280e9, 0.880},
        {2.356e9, 0.894},
        {2.497e9, 0.940},
        {2.631e9, 0.982},
        {2.757e9, 1.035},
    };
    const double peakRate = 19.2e12;
    const double memBw = 448e9;
    const double transferBw = 32e9;
    const double staticPower = 9.0;

    uint16_t serverPort = 9000;
    std::vector<Ptr<GpuAccelerator>> gpus(nBackends);

    for (uint32_t i = 0; i < nBackends; i++)
    {
        const OperatingPoint& baseOpp = rtx5060Opps.front();
        double rateAtBaseFreq = peakRate * (baseOpp.frequency / 2.50e9);

        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> queueScheduler = CreateObject<FifoQueueScheduler>();

        Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
        energyModel->SetAttribute("StaticPower", DoubleValue(staticPower));
        energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(effectiveCapacitance));

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(rateAtBaseFreq));
        gpu->SetAttribute("ComputeEfficiency", DoubleValue(0.075));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(memBw));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(transferBw));
        gpu->SetAttribute("Voltage", DoubleValue(baseOpp.voltage));
        gpu->SetAttribute("Frequency", DoubleValue(baseOpp.frequency));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueScheduler));
        gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
        for (const auto& opp : rtx5060Opps)
        {
            gpu->AddOperatingPoint(opp.frequency, opp.voltage);
        }
        serverNodes.Get(i)->AggregateObject(gpu);
        gpus[i] = gpu;

        BackendServerHelper serverHelper(serverPort);
        ApplicationContainer serverApps = serverHelper.Install(serverNodes.Get(i));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simTime + 1.0));
    }

    Cluster cluster;
    for (uint32_t i = 0; i < nBackends; i++)
    {
        cluster.AddBackend(
            serverNodes.Get(i),
            InetSocketAddress(orchServerIfaces[i].GetAddress(1), serverPort));
    }

    Ptr<ClusterScheduler> scheduler;
    Ptr<ScalingPolicy> scalingPolicy;
    if (scheme == "RR-NS")
    {
        scheduler = CreateObject<RoundRobinScheduler>();
        scalingPolicy = CreateObject<UtilizationScalingPolicy>();
    }
    else if (scheme == "LU-NS")
    {
        scheduler = CreateObject<LeastLoadedScheduler>();
        scalingPolicy = CreateObject<UtilizationScalingPolicy>();
    }
    else
    {
        scheduler = CreateObject<LeastLoadedScheduler>();
        scalingPolicy = CreateObject<ConservativeScalingPolicy>();
    }

    Ptr<MaxActiveTasksPolicy> admissionPolicy = CreateObject<MaxActiveTasksPolicy>();
    admissionPolicy->SetAttribute("MaxActiveTasks", UintegerValue(maxActiveTasks));

    Ptr<DeviceManager> deviceManager = CreateObject<DeviceManager>();
    deviceManager->SetAttribute("ScalingPolicy", PointerValue(scalingPolicy));
    deviceManager->SetAttribute("DeviceProtocol",
                                PointerValue(CreateObject<GpuDeviceProtocol>()));

    uint16_t orchPort = 8080;
    Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
    orchestrator->SetAttribute("AdmissionPolicy", PointerValue(admissionPolicy));
    orchestrator->SetAttribute("DeviceManager", PointerValue(deviceManager));
    orchestrator->SetCluster(cluster);
    orchNode.Get(0)->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(simTime + 1.0));

    orchestrator->TraceConnectWithoutContext("TaskDispatched",
                                             MakeCallback(&OnTaskDispatched));
    orchestrator->TraceConnectWithoutContext("TaskCompleted",
                                             MakeCallback(&OnTaskCompleted));

    Ptr<DagTask> dag = BuildDag(shape, inputSize, computeDemand, outputSize,
                                Seconds(1.0) + MilliSeconds(deadlineBudgetMs));

    Ptr<DagValidationClient> client = CreateObject<DagValidationClient>();
    client->SetRemote(InetSocketAddress(clientOrchIfaces.GetAddress(1), orchPort));
    client->SetDag(dag);
    clientNode.Get(0)->AddApplication(client);
    client->SetStartTime(Seconds(1.0));
    client->SetStopTime(Seconds(simTime));

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    std::cout << "\n=== DAG Validation: " << shape
              << " (nBackends=" << nBackends << ", scheme=" << scheme << ") ===\n";
    std::cout << "Tasks in DAG:       " << dag->GetTaskCount() << "\n";
    std::cout << "Tasks dispatched:   " << g_taskEvents.size() << "\n";
    std::cout << "Workload complete:  " << (client->IsComplete() ? "yes" : "no") << "\n";

    if (client->IsComplete())
    {
        std::cout << std::fixed << std::setprecision(3)
                  << "Workload latency:   " << client->GetWorkloadLatency().GetMilliSeconds()
                  << " ms\n";
    }

    std::cout << "\nPer-task timeline (ms):\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  idx  taskId      backend   dispatch   complete   duration\n";
    for (size_t k = 0; k < g_taskOrder.size(); k++)
    {
        const TaskEvent& e = g_taskEvents.at(g_taskOrder[k]);
        std::cout << "  " << std::setw(3) << k << "  0x" << std::hex << std::setw(8)
                  << std::setfill('0') << e.taskId << std::dec << std::setfill(' ')
                  << "  " << std::setw(7) << e.backendIdx << "   "
                  << std::setw(8) << e.dispatchMs << "   "
                  << std::setw(8) << e.completeMs << "   "
                  << std::setw(8) << (e.completeMs - e.dispatchMs) << "\n";
    }

    std::cout << "\nBackend energy:\n";
    for (uint32_t i = 0; i < nBackends; i++)
    {
        std::cout << "  backend " << i << ": " << std::setprecision(6)
                  << gpus[i]->GetTotalEnergy() << " J\n";
    }

    if (!outputDir.empty())
    {
        std::ostringstream prefix;
        prefix << outputDir << "/" << shape << "_b" << nBackends << "_" << scheme
               << "_s" << seed;

        std::ofstream tofs(prefix.str() + "_tasks.csv");
        if (tofs.is_open())
        {
            tofs << "dag_idx,task_id,backend_idx,dispatch_ms,complete_ms,duration_ms\n";
            tofs << std::fixed << std::setprecision(4);
            for (size_t k = 0; k < g_taskOrder.size(); k++)
            {
                const TaskEvent& e = g_taskEvents.at(g_taskOrder[k]);
                tofs << k << "," << e.taskId << "," << e.backendIdx << ","
                     << e.dispatchMs << "," << e.completeMs << ","
                     << (e.completeMs - e.dispatchMs) << "\n";
            }
        }

        std::ofstream wofs(prefix.str() + "_workload.csv");
        if (wofs.is_open())
        {
            wofs << "shape,n_backends,scheme,seed,task_count,submit_ms,complete_ms,"
                    "latency_ms,completed\n";
            wofs << shape << "," << nBackends << "," << scheme << "," << seed << ","
                 << dag->GetTaskCount() << "," << ToMs(client->GetSubmitTime()) << ","
                 << ToMs(client->GetCompleteTime()) << ","
                 << client->GetWorkloadLatency().GetMilliSeconds() << ","
                 << (client->IsComplete() ? 1 : 0) << "\n";
        }
    }

    Simulator::Destroy();
    return 0;
}
