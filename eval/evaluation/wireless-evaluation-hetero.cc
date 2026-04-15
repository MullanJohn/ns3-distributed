/*
 * Copyright (c) 2026 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster.h"
#include "ns3/conservative-scaling-policy.h"
#include "ns3/core-module.h"
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
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/network-module.h"
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/backend-server-helper.h"
#include "ns3/backend-server.h"
#include "ns3/point-to-point-module.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/utilization-scaling-policy.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ContinuumWirelessEvaluationHetero");


struct TaskRecord
{
    double timestampMs;
    uint32_t clientId;
    std::string event;
    double latencyMs;
    double computeMs;
    double queueMs;
    double commMs;
};

static std::vector<TaskRecord> g_taskRecords;

struct ServerTiming
{
    double arrivalMs{0.0};
    double completionMs{0.0};
    double computeMs{0.0};
};

static std::unordered_map<uint64_t, ServerTiming> g_serverTimings;

static double
ToMs(Time t)
{
    return t.GetSeconds() * 1000.0;
}

static void
ServerTaskReceived(Ptr<const Task> task)
{
    g_serverTimings[task->GetTaskId()].arrivalMs = ToMs(Simulator::Now());
}

static void
ServerTaskProcessed(Ptr<const Task> task, Time duration)
{
    auto& timing = g_serverTimings[task->GetTaskId()];
    timing.computeMs = ToMs(duration);
    timing.completionMs = ToMs(Simulator::Now());
}

static void
TaskSent(uint32_t clientIdx, Ptr<const Task> /* task */)
{
    g_taskRecords.push_back(
        {ToMs(Simulator::Now()), clientIdx, "sent", 0.0, 0.0, 0.0, 0.0});
}

static void
TaskDropped(uint32_t clientIdx, uint64_t /* frameNumber */)
{
    g_taskRecords.push_back(
        {ToMs(Simulator::Now()), clientIdx, "dropped", 0.0, 0.0, 0.0, 0.0});
}

static void
TaskRejected(uint32_t clientIdx, Ptr<const Task> /* task */)
{
    g_taskRecords.push_back(
        {ToMs(Simulator::Now()), clientIdx, "rejected", 0.0, 0.0, 0.0, 0.0});
}

static void
TaskProcessed(uint32_t clientIdx, Ptr<const Task> task, Time latency)
{
    double latencyMs = ToMs(latency);
    double computeMs = 0.0;
    double queueMs = 0.0;
    double commMs = 0.0;

    auto it = g_serverTimings.find(task->GetTaskId());
    if (it != g_serverTimings.end())
    {
        computeMs = it->second.computeMs;
        double serverMs = it->second.completionMs - it->second.arrivalMs;
        queueMs = std::max(0.0, serverMs - computeMs);
        commMs = std::max(0.0, latencyMs - serverMs);
    }

    g_taskRecords.push_back(
        {ToMs(Simulator::Now()),
         clientIdx, "processed", latencyMs, computeMs, queueMs, commMs});
}

static void
TaskViolated(uint32_t clientIdx, Ptr<const Task> task, Time latency)
{
    double latencyMs = ToMs(latency);
    double computeMs = 0.0;
    double queueMs = 0.0;
    double commMs = 0.0;

    auto it = g_serverTimings.find(task->GetTaskId());
    if (it != g_serverTimings.end())
    {
        computeMs = it->second.computeMs;
        double serverMs = it->second.completionMs - it->second.arrivalMs;
        queueMs = std::max(0.0, serverMs - computeMs);
        commMs = std::max(0.0, latencyMs - serverMs);
    }

    g_taskRecords.push_back(
        {ToMs(Simulator::Now()),
         clientIdx, "violated", latencyMs, computeMs, queueMs, commMs});
}

int
main(int argc, char* argv[])
{
    std::string scheme = "RR-NS";
    uint32_t nClients = 1;
    uint32_t nBackends = 2;
    double frameRate = 30.0;
    double meanFrameSize = 60000;
    double simTime = 20.0;
    uint32_t maxActiveTasks = 1;
    double computeDemand = 8.7e9;
    double outputSize = 1000;
    double deadlineBudgetMs = 33.0;
    double commBudgetMs = 2.0;
    double discRadius = 5.0;
    uint32_t channelWidth = 80;
    double inputSizeStddev = 0.0;
    double computeDemandStddev = 0.0;
    double outputSizeStddev = 0.0;
    std::string backboneRate = "1Gbps";
    std::string backboneDelay = "2us";
    std::string outputDir = "";
    uint32_t seed = 1;
    uint32_t runNumber = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("scheme", "Scheduling scheme: RR-NS, LL-NS, LL-SG", scheme);
    cmd.AddValue("nClients", "Number of periodic clients", nClients);
    cmd.AddValue("nBackends", "Number of GPU backend servers", nBackends);
    cmd.AddValue("frameRate", "Frames per second", frameRate);
    cmd.AddValue("meanFrameSize", "Mean frame size in bytes", meanFrameSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("maxActiveTasks", "Per-backend admission threshold", maxActiveTasks);
    cmd.AddValue("computeDemand", "Compute demand per frame in FLOPS", computeDemand);
    cmd.AddValue("outputSize", "Output size per frame in bytes", outputSize);
    cmd.AddValue("deadlineBudgetMs",
                 "End-to-end delay budget per frame in ms (0 = use 1/frameRate)",
                 deadlineBudgetMs);
    cmd.AddValue("commBudgetMs",
                 "Communication budget (T_ul + T_dl) in ms subtracted from deadline",
                 commBudgetMs);
    cmd.AddValue("discRadius", "Radius of client placement disc in metres", discRadius);
    cmd.AddValue("channelWidth", "WiFi channel width in MHz (20/40/80/160)", channelWidth);
    cmd.AddValue("inputSizeStddev",
                 "Std-dev of per-frame input size in bytes (0 = deterministic)",
                 inputSizeStddev);
    cmd.AddValue("computeDemandStddev",
                 "Std-dev of per-frame compute demand in FLOPS (0 = deterministic)",
                 computeDemandStddev);
    cmd.AddValue("outputSizeStddev",
                 "Std-dev of per-frame output size in bytes (0 = deterministic)",
                 outputSizeStddev);
    cmd.AddValue("backboneRate", "AP-to-server link data rate", backboneRate);
    cmd.AddValue("backboneDelay", "AP-to-server link delay", backboneDelay);
    cmd.AddValue("outputDir", "Directory for CSV output (empty = no file output)", outputDir);
    cmd.AddValue("seed", "RNG seed for reproducibility", seed);
    cmd.AddValue("runNumber", "RNG run number for independent replications", runNumber);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(scheme != "RR-NS" && scheme != "LL-NS" && scheme != "LL-SG",
                    "Unknown scheme: " << scheme << ". Use RR-NS, LL-NS, or LL-SG.");

    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(runNumber);

    NodeContainer clientNodes;
    clientNodes.Create(nClients);

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer serverNodes;
    serverNodes.Create(nBackends);

    // Short HE guard interval (800 ns) for indoor LOS;
    Config::SetDefault("ns3::HeConfiguration::GuardInterval",
                       TimeValue(NanoSeconds(800)));

    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("100p"));
    Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(MilliSeconds(100)));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("continuum-wireless-eval");

    SpectrumWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    // 80 MHz on 5 GHz, primary20 = 0. Modal consumer WiFi 6 configuration.
    uint32_t channelNumber = (channelWidth == 160) ? 50 : 0;
    std::ostringstream channelSettings;
    channelSettings << "{" << channelNumber << ", " << channelWidth << ", BAND_5GHZ, 0}";
    wifiPhy.Set("ChannelSettings", StringValue(channelSettings.str()));
    // typical of consumer WiFi 6 AP / prosumer STA datasheets.
    wifiPhy.Set("TxPowerStart", DoubleValue(20.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(20.0));
    // 2x2 MIMO — consumer WiFi 6 AP and typical WiFi 6 smartphone/camera
    wifiPhy.Set("Antennas", UintegerValue(2));
    wifiPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(2));
    wifiPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(2));

    auto spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    // LogDistance indoor path loss: n = 3.0 (residential / small office)
    Ptr<LogDistancePropagationLossModel> lossModel =
        CreateObject<LogDistancePropagationLossModel>();
    lossModel->SetAttribute("Exponent", DoubleValue(3.0));
    lossModel->SetAttribute("ReferenceDistance", DoubleValue(1.0));
    lossModel->SetAttribute("ReferenceLoss", DoubleValue(46.67));
    spectrumChannel->AddPropagationLossModel(lossModel);
    spectrumChannel->SetPropagationDelayModel(
        CreateObject<ConstantSpeedPropagationDelayModel>());
    wifiPhy.SetChannel(spectrumChannel);

    wifiMac.SetType("ns3::ApWifiMac",
                    "QosSupported",
                    BooleanValue(true),
                    "Ssid",
                    SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac",
                    "QosSupported",
                    BooleanValue(true),
                    "Ssid",
                    SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, clientNodes);

    // AP fixed at disc centre.
    MobilityHelper apMobility;
    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator>();
    apAlloc->Add(Vector(0.0, 0.0, 0.0));
    apMobility.SetPositionAllocator(apAlloc);
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(apNode);

    // Clients uniformly distributed in a disc around the AP
    MobilityHelper clientMobility;
    clientMobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                                        "X",
                                        DoubleValue(0.0),
                                        "Y",
                                        DoubleValue(0.0),
                                        "rho",
                                        DoubleValue(discRadius));
    clientMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    clientMobility.Install(clientNodes);

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

    struct BackendSpec
    {
        double peakRate;
        double memBw;
        double transferBw;
        double sPower;
        double boostFrequency;
        double computeEfficiency;
        double effectiveCapacitance;
        const std::vector<OperatingPoint>* opps;
    };

    const std::vector<OperatingPoint> rtx5060Opps = {
        {2.280e9, 0.880},
        {2.356e9, 0.894},
        {2.497e9, 0.940},
        {2.631e9, 0.982},
        {2.757e9, 1.035},
    };
    const std::vector<OperatingPoint> rtx4060Opps = {
        {1.830e9, 0.875},
        {1.980e9, 0.900},
        {2.130e9, 0.935},
        {2.280e9, 0.975},
        {2.460e9, 1.025},
    };
    std::vector<BackendSpec> specs = {
        {19.2e12,   448e9,  32e9,       9.0,    2.50e9,    0.075,  18.3e-9, &rtx5060Opps},
        {15.1e12,   272e9,  4e9,        13.0,   2.46e9,    0.075,  16.6e-9, &rtx4060Opps},
    };

    for (uint32_t i = 0; i < nBackends; i++)
    {
        const BackendSpec& spec = specs[i % specs.size()];
        const OperatingPoint& baseOpp = spec.opps->front();
        double rateAtBaseFreq = spec.peakRate * (baseOpp.frequency / spec.boostFrequency);

        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> queueScheduler = CreateObject<FifoQueueScheduler>();

        Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
        energyModel->SetAttribute("StaticPower", DoubleValue(spec.sPower));
        energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(spec.effectiveCapacitance));

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(rateAtBaseFreq));
        gpu->SetAttribute("ComputeEfficiency", DoubleValue(spec.computeEfficiency));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(spec.memBw));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(spec.transferBw));
        gpu->SetAttribute("Voltage", DoubleValue(baseOpp.voltage));
        gpu->SetAttribute("Frequency", DoubleValue(baseOpp.frequency));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(queueScheduler));
        gpu->SetAttribute("EnergyModel", PointerValue(energyModel));

        for (const auto& opp : *spec.opps)
        {
            gpu->AddOperatingPoint(opp.frequency, opp.voltage);
        }

        serverNodes.Get(i)->AggregateObject(gpu);
        gpus[i] = gpu;

        BackendServerHelper serverHelper(serverPort);
        ApplicationContainer serverApps = serverHelper.Install(serverNodes.Get(i));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simTime + 1.0));

        Ptr<BackendServer> server = DynamicCast<BackendServer>(serverApps.Get(0));
        server->TraceConnectWithoutContext("TaskReceived",
                                           MakeCallback(&ServerTaskReceived));
        server->TraceConnectWithoutContext("TaskProcessed",
                                           MakeCallback(&ServerTaskProcessed));
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
        scheduler = CreateObject<RoundRobinScheduler>();
        scalingPolicy = CreateObject<UtilizationScalingPolicy>();
    }
    else if (scheme == "LL-NS")
    {
        scheduler = CreateObject<LeastLoadedScheduler>();
        scalingPolicy = CreateObject<UtilizationScalingPolicy>();
    }
    else
    {
        scheduler = CreateObject<LeastLoadedScheduler>();
        scalingPolicy = CreateObject<ConservativeScalingPolicy>();
    }

    Ptr<DeviceManager> deviceManager = CreateObject<DeviceManager>();
    deviceManager->SetAttribute("ScalingPolicy", PointerValue(scalingPolicy));
    deviceManager->SetAttribute("DeviceProtocol", PointerValue(CreateObject<GpuDeviceProtocol>()));

    uint16_t orchPort = 8080;
    Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(scheduler));
    orchestrator->SetAttribute("AdmissionPolicy", PointerValue(admissionPolicy));
    orchestrator->SetAttribute("DeviceManager", PointerValue(deviceManager));
    orchestrator->SetCluster(cluster);
    apNode.Get(0)->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(simTime + 1.0));


    for (uint32_t i = 0; i < nClients; i++)
    {
        OffloadClientHelper clientHelper(
            InetSocketAddress(apWifiInterface.GetAddress(0), orchPort));
        clientHelper.SetTaskRate(frameRate);
        clientHelper.SetMeanInputSize(meanFrameSize, inputSizeStddev);
        clientHelper.SetComputeDemand(computeDemand, computeDemandStddev);
        clientHelper.SetOutputSize(outputSize, outputSizeStddev);
        if (deadlineBudgetMs > 0)
        {
            clientHelper.SetDeadlineBudget(MilliSeconds(deadlineBudgetMs));
        }
        if (commBudgetMs > 0)
        {
            clientHelper.SetCommunicationBudget(MilliSeconds(commBudgetMs));
        }

        ApplicationContainer clientApps = clientHelper.Install(clientNodes.Get(i));
        Ptr<OffloadClient> client = DynamicCast<OffloadClient>(clientApps.Get(0));

        client->TraceConnectWithoutContext("TaskSent", MakeBoundCallback(&TaskSent, i));
        client->TraceConnectWithoutContext("TaskDropped", MakeBoundCallback(&TaskDropped, i));
        client->TraceConnectWithoutContext("TaskRejected", MakeBoundCallback(&TaskRejected, i));
        client->TraceConnectWithoutContext("TaskProcessed", MakeBoundCallback(&TaskProcessed, i));
        client->TraceConnectWithoutContext("TaskViolated", MakeBoundCallback(&TaskViolated, i));

        clientApps.Start(Seconds(1.0 + i * 0.01));
        clientApps.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    uint32_t statSent = 0;
    uint32_t statProcessed = 0;
    uint32_t statDropped = 0;
    uint32_t statRejected = 0;
    uint32_t statViolated = 0;
    double sumLatency = 0.0;
    double sumCompute = 0.0;
    double sumQueue = 0.0;
    double sumComm = 0.0;

    for (const auto& r : g_taskRecords)
    {
        if (r.event == "sent")
        {
            statSent++;
        }
        else if (r.event == "processed")
        {
            statProcessed++;
            sumLatency += r.latencyMs;
            sumCompute += r.computeMs;
            sumQueue += r.queueMs;
            sumComm += r.commMs;
        }
        else if (r.event == "violated")
        {
            statViolated++;
            sumLatency += r.latencyMs;
            sumCompute += r.computeMs;
            sumQueue += r.queueMs;
            sumComm += r.commMs;
        }
        else if (r.event == "dropped")
        {
            statDropped++;
        }
        else if (r.event == "rejected")
        {
            statRejected++;
        }
    }

    std::cout << "\n=== " << scheme << " Summary ===" << std::endl;
    std::cout << "  Tasks sent:      " << statSent << std::endl;
    std::cout << "  Tasks processed: " << statProcessed << std::endl;
    std::cout << "  Tasks violated:  " << statViolated << std::endl;
    std::cout << "  Tasks dropped:   " << statDropped << std::endl;
    std::cout << "  Tasks rejected:  " << statRejected << std::endl;
    uint32_t statCompleted = statProcessed + statViolated;
    if (statCompleted > 0)
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Avg latency:      " << sumLatency / statCompleted << " ms" << std::endl;
        std::cout << "  Avg compute:      " << sumCompute / statCompleted << " ms" << std::endl;
        std::cout << "  Avg queue:        " << sumQueue / statCompleted << " ms" << std::endl;
        std::cout << "  Avg comm:         " << sumComm / statCompleted << " ms" << std::endl;
    }
    for (uint32_t i = 0; i < nBackends; i++)
    {
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "  Backend " << i << " energy: " << gpus[i]->GetTotalEnergy() << " J"
                  << std::endl;
    }

    if (!outputDir.empty())
    {
        std::ostringstream prefix;
        prefix << outputDir << "/" << scheme << "_c" << nClients << "_b" << nBackends << "_s"
               << seed << "_r" << runNumber;

        std::string framesFile = prefix.str() + "_frames.csv";
        std::ofstream fofs(framesFile);
        if (fofs.is_open())
        {
            fofs << "timestamp_ms,client_id,event,latency_ms,compute_ms,queue_ms,comm_ms\n";
            fofs << std::fixed << std::setprecision(4);
            for (const auto& r : g_taskRecords)
            {
                fofs << r.timestampMs << "," << r.clientId << "," << r.event << ","
                     << r.latencyMs << "," << r.computeMs << "," << r.queueMs << ","
                     << r.commMs << "\n";
            }
            fofs.close();
        }
        else
        {
            NS_LOG_ERROR("Failed to open " << framesFile);
        }

        std::string energyFile = prefix.str() + "_energy.csv";
        std::ofstream eofs(energyFile);
        if (eofs.is_open())
        {
            eofs << "backend_id,total_energy_j\n";
            eofs << std::fixed << std::setprecision(6);
            for (uint32_t i = 0; i < nBackends; i++)
            {
                eofs << i << "," << gpus[i]->GetTotalEnergy() << "\n";
            }
            eofs.close();
        }
        else
        {
            NS_LOG_ERROR("Failed to open " << energyFile);
        }

        std::string positionsFile = prefix.str() + "_positions.csv";
        std::ofstream pofs(positionsFile);
        if (pofs.is_open())
        {
            pofs << "client_id,x,y,distance_m\n";
            pofs << std::fixed << std::setprecision(4);
            for (uint32_t i = 0; i < nClients; i++)
            {
                Vector pos = clientNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
                double distance = std::sqrt(pos.x * pos.x + pos.y * pos.y);
                pofs << i << "," << pos.x << "," << pos.y << "," << distance << "\n";
            }
            pofs.close();
        }
        else
        {
            NS_LOG_ERROR("Failed to open " << positionsFile);
        }
    }

    Simulator::Destroy();

    return 0;
}
