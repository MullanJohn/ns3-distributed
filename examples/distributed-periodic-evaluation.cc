/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/periodic-client-helper.h"
#include "ns3/periodic-client.h"
#include "ns3/periodic-server-helper.h"
#include "ns3/periodic-server.h"
#include "ns3/cluster.h"
#include "ns3/conservative-scaling-policy.h"
#include "ns3/core-module.h"
#include "ns3/device-manager.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/edge-orchestrator.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/first-fit-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/gpu-device-protocol.h"
#include "ns3/internet-module.h"
#include "ns3/least-loaded-scheduler.h"
#include "ns3/max-active-tasks-policy.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/utilization-scaling-policy.h"

#include <iomanip>

// ===========================================================================
//
//  Periodic Edge Computing Evaluation
//
//  Benchmarks three scheduling/admission/scaling schemes for periodic frame
//  offloading over WiFi 7 (802.11be) to GPU backends.
//
//  Network topology:
//
//       Periodic Client 0 (STA)   Periodic Client 1 (STA)   ...   Periodic Client N (STA)
//           |                    |                         |
//           |  WiFi 7 (802.11be) 10.0.1.0/24               |
//           |                    |                         |
//           +--------------------+-------------------------+
//                                |
//                       AP / Orchestrator (n_AP)
//                      +---------------------+
//                      | EdgeOrchestrator    |
//                      +--+---------------+--+
//                         |               |
//               10.0.2.0  |               |  10.0.3.0  ...
//               1Gbps/1ms |               |  1Gbps/1ms
//                         |               |
//                  Server 0 (n_S0)  Server 1 (n_S1)  ...
//                 +--------------+ +--------------+
//                 | PeriodicServer   | | PeriodicServer   |
//                 |      |       | |      |       |
//                 |      v       | |      v       |
//                 |GpuAccelerator| |GpuAccelerator|
//                 +--------------+ +--------------+
//
//  Schemes:
//     RR-NS: FirstFit + MaxActiveTasks + UtilizationScaling
//     LU-NS: LeastLoaded + MaxActiveTasks + UtilizationScaling
//     LU-SG: LeastLoaded + MaxActiveTasks + ConservativeScaling
//
// ===========================================================================

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistributedPeriodicEvaluation");

struct ClientStats
{
    uint64_t framesSent{0};
    uint64_t framesDropped{0};
    uint64_t framesRejected{0};
    uint64_t framesProcessed{0};
    double totalLatencyMs{0.0};
};

static std::vector<ClientStats> g_clientStats;
static std::vector<double> g_backendEnergyJ;

static void
FrameSent(uint32_t clientIdx, Ptr<const Task> task)
{
    g_clientStats[clientIdx].framesSent++;
}

static void
FrameDropped(uint32_t clientIdx, uint64_t frameNumber)
{
    g_clientStats[clientIdx].framesDropped++;
}

static void
FrameRejected(uint32_t clientIdx, Ptr<const Task> task)
{
    g_clientStats[clientIdx].framesRejected++;
}

static void
FrameProcessed(uint32_t clientIdx, Ptr<const Task> task, Time latency)
{
    g_clientStats[clientIdx].framesProcessed++;
    g_clientStats[clientIdx].totalLatencyMs += latency.GetMilliSeconds();
}

int
main(int argc, char* argv[])
{
    std::string scheme = "RR-NS";
    uint32_t nClients = 4;
    uint32_t nBackends = 2;
    double frameRate = 30.0;
    double meanFrameSize = 60000;
    double simTime = 10.0;
    uint32_t maxActiveTasks = 10;
    double computeDemand = 28.6e9;
    double computeRate = 8.1e12;
    double memoryBandwidth = 300e9;
    double gpuMaxFreq = 1.59e9;
    double gpuMinFreq = 585e6;
    double gpuMaxVoltage = 1.05;
    double gpuMinVoltage = 0.65;
    double staticPower = 36.0;
    double effectiveCapacitance = 1.94e-8;
    double outputSize = 1000;
    std::string backboneRate = "1Gbps";
    std::string backboneDelay = "1ms";
    uint32_t seed = 1;
    uint32_t runNumber = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("scheme", "Scheduling scheme: RR-NS, LU-NS, LU-SG", scheme);
    cmd.AddValue("nClients", "Number of periodic clients", nClients);
    cmd.AddValue("nBackends", "Number of GPU backend servers", nBackends);
    cmd.AddValue("frameRate", "Frames per second", frameRate);
    cmd.AddValue("meanFrameSize", "Mean frame size in bytes", meanFrameSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("maxActiveTasks", "Per-backend admission threshold", maxActiveTasks);
    cmd.AddValue("computeDemand", "Compute demand per frame in FLOPS", computeDemand);
    cmd.AddValue("computeRate", "GPU compute rate in FLOPS", computeRate);
    cmd.AddValue("memoryBandwidth", "GPU memory bandwidth in bytes/sec", memoryBandwidth);
    cmd.AddValue("gpuMaxFreq", "GPU maximum frequency in Hz", gpuMaxFreq);
    cmd.AddValue("gpuMinFreq", "GPU minimum frequency in Hz", gpuMinFreq);
    cmd.AddValue("gpuMaxVoltage", "GPU maximum voltage in V", gpuMaxVoltage);
    cmd.AddValue("gpuMinVoltage", "GPU minimum voltage in V", gpuMinVoltage);
    cmd.AddValue("staticPower", "GPU static/idle power in W", staticPower);
    cmd.AddValue("effectiveCapacitance", "DVFS effective capacitance", effectiveCapacitance);
    cmd.AddValue("outputSize", "Output size per frame in bytes", outputSize);
    cmd.AddValue("backboneRate", "AP-to-server link data rate", backboneRate);
    cmd.AddValue("backboneDelay", "AP-to-server link delay", backboneDelay);
    cmd.AddValue("seed", "RNG seed for reproducibility", seed);
    cmd.AddValue("runNumber", "RNG run number for independent replications", runNumber);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(scheme != "RR-NS" && scheme != "LU-NS" && scheme != "LU-SG",
                    "Unknown scheme: " << scheme << ". Use RR-NS, LU-NS, or LU-SG.");

    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(runNumber);

    g_clientStats.resize(nClients);
    g_backendEnergyJ.resize(nBackends, 0.0);

    NS_LOG_UNCOND("=== Periodic Edge Computing Evaluation ===");
    NS_LOG_UNCOND("Scheme:      " << scheme);
    NS_LOG_UNCOND("Clients:     " << nClients);
    NS_LOG_UNCOND("Backends:    " << nBackends);
    NS_LOG_UNCOND("Frame rate:  " << frameRate << " fps");
    NS_LOG_UNCOND("Frame size:  " << meanFrameSize / 1000.0 << " KB");
    NS_LOG_UNCOND("Sim time:    " << simTime << " s");
    NS_LOG_UNCOND("Seed:        " << seed);
    NS_LOG_UNCOND("Run:         " << runNumber);
    NS_LOG_UNCOND("");

    NodeContainer clientNodes;
    clientNodes.Create(nClients);

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer serverNodes;
    serverNodes.Create(nBackends);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("EhtMcs9"),
                                 "ControlMode",
                                 StringValue("EhtMcs0"));

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("periodic-edge");

    SpectrumWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    auto spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    wifiPhy.SetChannel(spectrumChannel);

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, clientNodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    for (uint32_t i = 0; i < nClients; i++)
    {
        positionAlloc->Add(Vector(5.0 * (i + 1), 0.0, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(clientNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(backboneRate));
    p2p.SetChannelAttribute("Delay", StringValue(backboneDelay));

    std::vector<NetDeviceContainer> backboneDevices(nBackends);
    for (uint32_t i = 0; i < nBackends; i++)
    {
        backboneDevices[i] = p2p.Install(apNode.Get(0), serverNodes.Get(i));
    }

    InternetStackHelper stack;
    stack.Install(clientNodes);
    stack.Install(apNode);
    stack.Install(serverNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apWifiInterface = ipv4.Assign(apDevice);
    ipv4.Assign(staDevices);

    std::vector<Ipv4InterfaceContainer> backboneInterfaces(nBackends);
    for (uint32_t i = 0; i < nBackends; i++)
    {
        std::ostringstream base;
        base << "10.0." << (i + 2) << ".0";
        ipv4.SetBase(base.str().c_str(), "255.255.255.0");
        backboneInterfaces[i] = ipv4.Assign(backboneDevices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t serverPort = 9000;
    std::vector<Ptr<GpuAccelerator>> gpus(nBackends);

    double rateAtMinFreq = computeRate * (gpuMinFreq / gpuMaxFreq);

    for (uint32_t i = 0; i < nBackends; i++)
    {
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> queueScheduler = CreateObject<FifoQueueScheduler>();

        Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
        energyModel->SetAttribute("StaticPower", DoubleValue(staticPower));
        energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(effectiveCapacitance));

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(rateAtMinFreq));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));
        gpu->SetAttribute("Voltage", DoubleValue(gpuMinVoltage));
        gpu->SetAttribute("Frequency", DoubleValue(gpuMinFreq));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueScheduler));
        gpu->SetAttribute("EnergyModel", PointerValue(energyModel));
        serverNodes.Get(i)->AggregateObject(gpu);
        gpus[i] = gpu;

        PeriodicServerHelper serverHelper(serverPort);
        ApplicationContainer serverApps = serverHelper.Install(serverNodes.Get(i));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simTime + 1.0));
    }

    Cluster cluster;
    for (uint32_t i = 0; i < nBackends; i++)
    {
        cluster.AddBackend(serverNodes.Get(i),
                           InetSocketAddress(backboneInterfaces[i].GetAddress(1), serverPort));
    }

    Ptr<ClusterScheduler> scheduler;
    Ptr<ScalingPolicy> scalingPolicy;

    Ptr<MaxActiveTasksPolicy> admissionPolicy = CreateObject<MaxActiveTasksPolicy>();
    admissionPolicy->SetAttribute("MaxActiveTasks", UintegerValue(maxActiveTasks));

    if (scheme == "RR-NS")
    {
        scheduler = CreateObject<FirstFitScheduler>();

        Ptr<UtilizationScalingPolicy> usp = CreateObject<UtilizationScalingPolicy>();
        usp->SetAttribute("MinFrequency", DoubleValue(gpuMinFreq));
        usp->SetAttribute("MaxFrequency", DoubleValue(gpuMaxFreq));
        scalingPolicy = usp;
    }
    else if (scheme == "LU-NS")
    {
        scheduler = CreateObject<LeastLoadedScheduler>();

        Ptr<UtilizationScalingPolicy> usp = CreateObject<UtilizationScalingPolicy>();
        usp->SetAttribute("MinFrequency", DoubleValue(gpuMinFreq));
        usp->SetAttribute("MaxFrequency", DoubleValue(gpuMaxFreq));
        scalingPolicy = usp;
    }
    else
    {
        scheduler = CreateObject<LeastLoadedScheduler>();

        Ptr<ConservativeScalingPolicy> csp = CreateObject<ConservativeScalingPolicy>();
        csp->SetAttribute("MinFrequency", DoubleValue(gpuMinFreq));
        csp->SetAttribute("MaxFrequency", DoubleValue(gpuMaxFreq));
        csp->SetAttribute("MinVoltage", DoubleValue(gpuMinVoltage));
        csp->SetAttribute("MaxVoltage", DoubleValue(gpuMaxVoltage));
        scalingPolicy = csp;
    }

    Ptr<DeviceManager> deviceManager = CreateObject<DeviceManager>();
    deviceManager->SetAttribute("ScalingPolicy", PointerValue(scalingPolicy));
    deviceManager->SetAttribute("DeviceProtocol", PointerValue(CreateObject<GpuDeviceProtocol>()));

    uint16_t orchPort = 8080;
    Ptr<EdgeOrchestrator> orchestrator = CreateObject<EdgeOrchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
    orchestrator->SetAttribute("AdmissionPolicy", PointerValue(admissionPolicy));
    orchestrator->SetAttribute("DeviceManager", PointerValue(deviceManager));
    orchestrator->SetCluster(cluster);
    apNode.Get(0)->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(simTime + 1.0));

    std::vector<Ptr<PeriodicClient>> clients(nClients);

    for (uint32_t i = 0; i < nClients; i++)
    {
        PeriodicClientHelper clientHelper(InetSocketAddress(apWifiInterface.GetAddress(0), orchPort));
        clientHelper.SetFrameRate(frameRate);
        clientHelper.SetMeanFrameSize(meanFrameSize);
        clientHelper.SetComputeDemand(computeDemand);
        clientHelper.SetOutputSize(outputSize);

        ApplicationContainer clientApps = clientHelper.Install(clientNodes.Get(i));
        Ptr<PeriodicClient> client = DynamicCast<PeriodicClient>(clientApps.Get(0));
        clients[i] = client;

        client->TraceConnectWithoutContext("FrameSent", MakeBoundCallback(&FrameSent, i));
        client->TraceConnectWithoutContext("FrameDropped", MakeBoundCallback(&FrameDropped, i));
        client->TraceConnectWithoutContext("FrameRejected", MakeBoundCallback(&FrameRejected, i));
        client->TraceConnectWithoutContext("FrameProcessed", MakeBoundCallback(&FrameProcessed, i));

        // Stagger start times for WiFi association
        clientApps.Start(Seconds(1.0 + i * 0.01));
        clientApps.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    for (uint32_t i = 0; i < nBackends; i++)
    {
        g_backendEnergyJ[i] = gpus[i]->GetTotalEnergy();
    }

    uint64_t totalSent = 0;
    uint64_t totalDropped = 0;
    uint64_t totalRejected = 0;
    uint64_t totalProcessed = 0;
    double totalLatencyMs = 0.0;

    for (uint32_t i = 0; i < nClients; i++)
    {
        totalSent += g_clientStats[i].framesSent;
        totalDropped += g_clientStats[i].framesDropped;
        totalRejected += g_clientStats[i].framesRejected;
        totalProcessed += g_clientStats[i].framesProcessed;
        totalLatencyMs += g_clientStats[i].totalLatencyMs;
    }

    uint64_t totalGenerated = totalSent + totalDropped;
    double dropRate = (totalGenerated > 0) ? (100.0 * totalDropped / totalGenerated) : 0.0;
    double rejectRate = (totalSent > 0) ? (100.0 * totalRejected / totalSent) : 0.0;
    double processRate = (totalSent > 0) ? (100.0 * totalProcessed / totalSent) : 0.0;
    double meanE2ELatency = (totalProcessed > 0) ? (totalLatencyMs / totalProcessed) : 0.0;

    NS_LOG_UNCOND("=== Per-Client Results ===");
    NS_LOG_UNCOND(std::left << std::setw(8) << "Client" << std::setw(10) << "Sent" << std::setw(10)
                            << "Dropped" << std::setw(10) << "Rejected" << std::setw(12)
                            << "Processed" << "Latency (ms)");
    NS_LOG_UNCOND(std::string(60, '-'));
    for (uint32_t i = 0; i < nClients; i++)
    {
        const auto& s = g_clientStats[i];
        double meanLatency = (s.framesProcessed > 0) ? (s.totalLatencyMs / s.framesProcessed) : 0.0;
        NS_LOG_UNCOND(std::left << std::setw(8) << i << std::setw(10) << s.framesSent
                                << std::setw(10) << s.framesDropped << std::setw(10)
                                << s.framesRejected << std::setw(12) << s.framesProcessed
                                << std::fixed << std::setprecision(2) << meanLatency);
    }

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Per-Backend Energy ===");
    NS_LOG_UNCOND(std::left << std::setw(10) << "Backend" << "Energy (J)");
    NS_LOG_UNCOND(std::string(24, '-'));
    double totalEnergy = 0.0;
    for (uint32_t i = 0; i < nBackends; i++)
    {
        NS_LOG_UNCOND(std::left << std::setw(10) << i << std::fixed << std::setprecision(3)
                                << g_backendEnergyJ[i]);
        totalEnergy += g_backendEnergyJ[i];
    }

    NS_LOG_UNCOND("");
    NS_LOG_UNCOND("=== Aggregate ===");
    NS_LOG_UNCOND("Frames generated:  " << totalGenerated);
    NS_LOG_UNCOND("Frames sent:       " << totalSent);
    NS_LOG_UNCOND("Frames dropped:    " << totalDropped << " (" << std::fixed
                                        << std::setprecision(1) << dropRate << "%)");
    NS_LOG_UNCOND("Frames rejected:   " << totalRejected << " (" << std::fixed
                                        << std::setprecision(1) << rejectRate << "%)");
    NS_LOG_UNCOND("Frames processed:  " << totalProcessed << " (" << std::fixed
                                        << std::setprecision(1) << processRate << "%)");
    NS_LOG_UNCOND("Mean E2E latency:  " << std::fixed << std::setprecision(2) << meanE2ELatency
                                        << " ms");
    NS_LOG_UNCOND("Total energy:      " << std::fixed << std::setprecision(3) << totalEnergy
                                        << " J");

    Simulator::Destroy();

    return 0;
}
