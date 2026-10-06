/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/backend-server.h"
#include "ns3/cluster.h"
#include "ns3/core-module.h"
#include "ns3/device-manager.h"
#include "ns3/drop-while-pending-submission-policy.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/internet-module.h"
#include "ns3/least-loaded-scheduler.h"
#include "ns3/max-active-tasks-policy.h"
#include "ns3/network-aware-scheduler.h"
#include "ns3/network-module.h"
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/orchestrator.h"
#include "ns3/point-to-point-module.h"
#include "ns3/system-path.h"
#include "ns3/task.h"
#include "ns3/tiered-delay-scheduler.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

// ===========================================================================
//
//  Computing Continuum Showcase: multi-tier scheduling of taskDAGs
//
//  8 clients, each generating a 2-task sequential-chain DAG (a template
//  DagTask built via SetLinearChainDagTemplate(2, ...)), offload through an
//  Orchestrator to a two-tier cluster:
//
//    Client 0 ──P2P (clientDelay)──┐
//    Client 1 ──P2P (clientDelay)──┤
//       ...                        Orchestrator ──P2P (edgeDelay,  edgeLinkRate)── Edge  (Orin NX)
//    Client 7 ──P2P (clientDelay)──┘             └─P2P (tier2Delay, tier2LinkRate)── Core (Orin AGX)
//
//  --scheduler selects which ClusterScheduler compares LeastLoadedScheduler
//  (delay-unaware baseline) against NetworkAwareScheduler/TieredDelayScheduler
//  (both delay-aware). --tier2Delay is the swept control variable; every other
//  link parameter is fixed. All per-run metrics are logged to two CSVs
//  (per-task client-side and orchestrator-side rows, joined on taskId at
//  analysis time) which are flushed and closed before Simulator::Destroy(),
//  so re-plotting never requires re-running the simulation.
//
// ===========================================================================

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ContinuumShowcaseExample");

namespace
{

/**
 * @brief Writes one CSV row per task for every workload completed by one client.
 */
class ClientLogger
{
  public:
    ClientLogger(uint32_t clientId, std::ofstream& out, double deadlineBudgetMs)
        : m_clientId(clientId),
          m_out(out),
          m_deadlineBudgetMs(deadlineBudgetMs)
    {
    }

    void OnWorkloadProcessed(Ptr<const DagTask> dag, Time latency)
    {
        double latencyMs = latency.GetSeconds() * 1000.0;

        for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
        {
            Ptr<Task> task = dag->GetTask(i);
            if (!task)
            {
                continue;
            }

            uint64_t taskId = task->GetTaskId();
            // Task IDs are (clientId << 32) | (tasksSent << 8) | taskIndex;
            // dropping the low 8 bits groups every task of one workload under
            // a single, uniquely-identifying key for per-workload analysis.
            uint64_t dagKey = taskId >> 8;

            m_out << m_clientId << ',' << dagKey << ',' << taskId << ',' << task->GetInputSize()
                  << ',' << task->GetComputeDemand() << ',' << task->GetOutputSize() << ','
                  << (task->GetComputeTime().GetSeconds() * 1000.0) << ','
                  << (task->GetBackendTime().GetSeconds() * 1000.0) << ',' << latencyMs << ','
                  << m_deadlineBudgetMs << '\n';
        }
    }

  private:
    uint32_t m_clientId;
    std::ofstream& m_out;
    double m_deadlineBudgetMs;
};

/**
 * @brief Writes one CSV row per completed task with its backend/tier assignment.
 */
class OrchestratorLogger
{
  public:
    explicit OrchestratorLogger(std::ofstream& out)
        : m_out(out)
    {
    }

    void OnTaskCompleted(uint64_t workloadId, uint64_t taskId, uint32_t backendIdx)
    {
        m_out << workloadId << ',' << taskId << ',' << backendIdx << '\n';
    }

  private:
    std::ofstream& m_out;
};

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t numClients = 8;
    std::string clientDataRate = "100Mbps";
    std::string clientDelay = "1ms";
    std::string edgeLinkRate = "1Gbps";
    double edgeDelayMs = 0.1;
    std::string tier2LinkRate = "500Mbps";
    double tier2DelayMs = 1.0;
    double edgeComputeRate = 1.8e12;  // Jetson Orin NX class
    double coreComputeRate = 5.3e12;  // Jetson Orin AGX class
    double memoryBandwidth = 900e9;
    double transferBandwidth = 32e9;
    double meanInputSize = 5e4;
    double meanComputeDemand = 5e9;
    double meanOutputSize = 5e3;
    double uniformRangeFraction = 0.10;
    double taskRate = 25.0;
    double deadlineBudgetMs = 40.0;
    uint32_t maxActiveTasks = 64;
    double gracePeriodMultiplier = 20.0;
    double overflowQueueDelayMs = 15;
    double recoverQueueDelayMs = 10;
    bool admitAll = false;
    std::string admissionMode = "session";
    double simTime = 5.0;
    std::string scheduler = "leastloaded";
    std::string outputDir = "examples/showcase/results";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numClients", "Number of client nodes", numClients);
    cmd.AddValue("clientDataRate", "Client-orchestrator link data rate", clientDataRate);
    cmd.AddValue("clientDelay", "Client-orchestrator link delay", clientDelay);
    cmd.AddValue("edgeLinkRate", "Orchestrator-edge link data rate (fixed)", edgeLinkRate);
    cmd.AddValue("edgeDelayMs", "Orchestrator-edge link delay in ms (fixed)", edgeDelayMs);
    cmd.AddValue("tier2LinkRate", "Orchestrator-core link data rate (fixed)", tier2LinkRate);
    cmd.AddValue("tier2DelayMs", "Orchestrator-core link delay in ms (swept control variable)",
                 tier2DelayMs);
    cmd.AddValue("edgeComputeRate", "Edge GPU compute rate in FLOPS", edgeComputeRate);
    cmd.AddValue("coreComputeRate", "Core GPU compute rate in FLOPS", coreComputeRate);
    cmd.AddValue("memoryBandwidth", "GPU DRAM memory bandwidth in bytes/sec", memoryBandwidth);
    cmd.AddValue("transferBandwidth",
                 "Host-device transfer bandwidth in bytes/sec",
                 transferBandwidth);
    cmd.AddValue("meanInputSize", "Mean input size per task in bytes", meanInputSize);
    cmd.AddValue("meanComputeDemand", "Mean compute demand per task in FLOPS", meanComputeDemand);
    cmd.AddValue("meanOutputSize", "Mean output size per task in bytes", meanOutputSize);
    cmd.AddValue("uniformRangeFraction",
                 "Half-width of the uniform [min,max] range for input/compute/output, "
                 "as a fraction of the mean (default 0.10 = +/-10%)",
                 uniformRangeFraction);
    cmd.AddValue("taskRate", "Per-client DAG generation rate in Hz", taskRate);
    cmd.AddValue("deadlineBudgetMs", "End-to-end deadline budget in ms", deadlineBudgetMs);
    cmd.AddValue("maxActiveTasks", "MaxActiveTasksPolicy capacity per backend", maxActiveTasks);
    cmd.AddValue("gracePeriodMultiplier",
                 "On stop, a client stops generating immediately but keeps its "
                 "connection open for up to gracePeriodMultiplier*deadlineBudgetMs "
                 "to let its one outstanding workload's response still arrive",
                 gracePeriodMultiplier);
    cmd.AddValue("overflowQueueDelayMs",
                 "TieredDelayScheduler: queue drain time in ms at/above which a tier "
                 "is considered saturated and overflows to the next tier (only used "
                 "with --scheduler=tiereddelay)",
                 overflowQueueDelayMs);
    cmd.AddValue("recoverQueueDelayMs",
                 "TieredDelayScheduler: queue drain time in ms at/below which an "
                 "overflowing backend is considered recovered and preferred again; "
                 "should be <= overflowQueueDelayMs (only used with "
                 "--scheduler=tiereddelay)",
                 recoverQueueDelayMs);
    cmd.AddValue("admitAll",
                 "Skip admission control entirely (always admit), instead of "
                 "MaxActiveTasksPolicy",
                 admitAll);
    cmd.AddValue("admissionMode",
                 "OffloadClient admission mode: session (default, admit once per "
                 "client) or workload (every DAG individually admitted)",
                 admissionMode);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("scheduler", "Scheduler: leastloaded, networkaware, or tiereddelay", scheduler);
    cmd.AddValue("outputDir", "Root directory for per-run CSV logs", outputDir);
    cmd.Parse(argc, argv);

    OffloadClient::AdmissionMode admissionModeEnum;
    if (admissionMode == "session")
    {
        admissionModeEnum = OffloadClient::SESSION_MODE;
    }
    else if (admissionMode == "workload")
    {
        admissionModeEnum = OffloadClient::WORKLOAD_MODE;
    }
    else
    {
        NS_ABORT_MSG("Unknown --admissionMode '" << admissionMode
                                                  << "'; expected session or workload");
    }

    Time gracePeriod = MilliSeconds(gracePeriodMultiplier * deadlineBudgetMs);

    NS_LOG_UNCOND("Continuum Showcase: " << numClients << " clients, scheduler=" << scheduler
                                         << ", admissionMode=" << admissionMode
                                         << ", tier2Delay=" << tier2DelayMs << "ms"
                                         << ", gracePeriod=" << gracePeriod.As(Time::MS));

    // A multi-task DAG's serialized payload (each task's full input data) can
    // exceed the 128 KB default TCP send/receive buffer once a chain has more
    // than one task; size both buffers generously so ns-3's TCP sockets never
    // silently drop an oversized single Send() call.
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4 * 1024 * 1024));

    NodeContainer clientNodes;
    clientNodes.Create(numClients);
    NodeContainer orchNode;
    orchNode.Create(1);
    NodeContainer edgeNode;
    edgeNode.Create(1);
    NodeContainer coreNode;
    coreNode.Create(1);

    InternetStackHelper stack;
    stack.Install(clientNodes);
    stack.Install(orchNode);
    stack.Install(edgeNode);
    stack.Install(coreNode);

    PointToPointHelper clientLink;
    clientLink.SetDeviceAttribute("DataRate", StringValue(clientDataRate));
    clientLink.SetChannelAttribute("Delay", StringValue(clientDelay));

    Ipv4AddressHelper address;
    std::vector<Ipv4Address> orchAddresses;
    for (uint32_t i = 0; i < numClients; i++)
    {
        NetDeviceContainer devices = clientLink.Install(clientNodes.Get(i), orchNode.Get(0));
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        orchAddresses.push_back(interfaces.GetAddress(1));
    }

    PointToPointHelper edgeLink;
    edgeLink.SetDeviceAttribute("DataRate", StringValue(edgeLinkRate));
    edgeLink.SetChannelAttribute("Delay", TimeValue(MilliSeconds(edgeDelayMs)));
    NetDeviceContainer devOrchEdge = edgeLink.Install(orchNode.Get(0), edgeNode.Get(0));
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifOrchEdge = address.Assign(devOrchEdge);

    PointToPointHelper coreLink;
    coreLink.SetDeviceAttribute("DataRate", StringValue(tier2LinkRate));
    coreLink.SetChannelAttribute("Delay", TimeValue(MilliSeconds(tier2DelayMs)));
    NetDeviceContainer devOrchCore = coreLink.Install(orchNode.Get(0), coreNode.Get(0));
    address.SetBase("10.2.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifOrchCore = address.Assign(devOrchCore);

    double edgeLinkCapacityBytesPerSec = DataRate(edgeLinkRate).GetBitRate() / 8.0;
    double tier2LinkCapacityBytesPerSec = DataRate(tier2LinkRate).GetBitRate() / 8.0;

    Ptr<GpuAccelerator> edgeGpu = CreateObject<GpuAccelerator>();
    edgeGpu->SetAttribute("ComputeRate", DoubleValue(edgeComputeRate));
    edgeGpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));
    edgeGpu->SetAttribute("TransferBandwidth", DoubleValue(transferBandwidth));
    edgeGpu->SetAttribute("ProcessingModel", PointerValue(CreateObject<FixedRatioProcessingModel>()));
    edgeGpu->SetAttribute("QueueScheduler", PointerValue(CreateObject<FifoQueueScheduler>()));
    edgeNode.Get(0)->AggregateObject(edgeGpu);

    Ptr<GpuAccelerator> coreGpu = CreateObject<GpuAccelerator>();
    coreGpu->SetAttribute("ComputeRate", DoubleValue(coreComputeRate));
    coreGpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));
    coreGpu->SetAttribute("TransferBandwidth", DoubleValue(transferBandwidth));
    coreGpu->SetAttribute("ProcessingModel", PointerValue(CreateObject<FixedRatioProcessingModel>()));
    coreGpu->SetAttribute("QueueScheduler", PointerValue(CreateObject<FifoQueueScheduler>()));
    coreNode.Get(0)->AggregateObject(coreGpu);

    uint16_t serverPort = 9000;
    Ptr<BackendServer> edgeServer = CreateObject<BackendServer>();
    edgeServer->SetAttribute("Port", UintegerValue(serverPort));
    edgeNode.Get(0)->AddApplication(edgeServer);
    edgeServer->SetStartTime(Seconds(0.0));
    edgeServer->SetStopTime(Seconds(simTime + gracePeriod.GetSeconds() + 1.0));

    Ptr<BackendServer> coreServer = CreateObject<BackendServer>();
    coreServer->SetAttribute("Port", UintegerValue(serverPort));
    coreNode.Get(0)->AddApplication(coreServer);
    coreServer->SetStartTime(Seconds(0.0));
    coreServer->SetStopTime(Seconds(simTime + gracePeriod.GetSeconds() + 1.0));

    Cluster cluster;
    // Backend 0 = edge tier (low, fixed delay).
    cluster.AddBackend(edgeNode.Get(0),
                       InetSocketAddress(ifOrchEdge.GetAddress(1), serverPort),
                       "",
                       MilliSeconds(edgeDelayMs),
                       edgeLinkCapacityBytesPerSec);
    // Backend 1 = core tier (swept delay).
    cluster.AddBackend(coreNode.Get(0),
                       InetSocketAddress(ifOrchCore.GetAddress(1), serverPort),
                       "",
                       MilliSeconds(tier2DelayMs),
                       tier2LinkCapacityBytesPerSec);

    Ptr<ClusterScheduler> clusterScheduler;
    if (scheduler == "leastloaded")
    {
        clusterScheduler = CreateObject<LeastLoadedScheduler>();
    }
    else if (scheduler == "networkaware")
    {
        clusterScheduler = CreateObject<NetworkAwareScheduler>();
    }
    else if (scheduler == "tiereddelay")
    {
        clusterScheduler = CreateObject<TieredDelayScheduler>();
        clusterScheduler->SetAttribute("OverflowQueueDelay",
                                       TimeValue(MilliSeconds(overflowQueueDelayMs)));
        clusterScheduler->SetAttribute("RecoverQueueDelay",
                                       TimeValue(MilliSeconds(recoverQueueDelayMs)));
    }
    else
    {
        NS_ABORT_MSG("Unknown --scheduler '" << scheduler
                                             << "'; expected leastloaded, networkaware, "
                                                "or tiereddelay");
    }

    // Orchestrator::CheckAdmission() already treats an unset AdmissionPolicy as
    // "admit everything" -- reuse that existing, tested path instead of
    // approximating "admit all" with an arbitrarily large MaxActiveTasks cap.
    Ptr<MaxActiveTasksPolicy> admissionPolicy;
    if (!admitAll)
    {
        admissionPolicy = CreateObject<MaxActiveTasksPolicy>();
        admissionPolicy->SetAttribute("MaxActiveTasks", UintegerValue(maxActiveTasks));
    }

    uint16_t orchPort = 8080;
    Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(clusterScheduler));
    if (admissionPolicy)
    {
        orchestrator->SetAttribute("AdmissionPolicy", PointerValue(admissionPolicy));
    }
    // Seeds each backend's real compute capacity (from its GpuAccelerator's
    // ComputeRate) into ClusterState -- without this, nominalComputeRate/
    // effectiveComputeRate stay 0 and TieredDelayScheduler's utilization ratio
    // degenerates to an all-or-nothing 0/1 signal. No ScalingPolicy is set, so
    // this only performs the one-time capacity seeding; EvaluateScaling() is a
    // no-op and no DVFS behavior is introduced.
    orchestrator->SetAttribute("DeviceManager", PointerValue(CreateObject<DeviceManager>()));
    orchestrator->SetCluster(cluster);
    orchNode.Get(0)->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(simTime + gracePeriod.GetSeconds() + 1.0));

    // --- Output setup: two per-task CSVs, flushed/closed before Simulator::Destroy() ---
    uint32_t rngRun = RngSeedManager::GetRun();
    std::ostringstream baseName;
    baseName << "tier2Delay_" << tier2DelayMs << "ms_" << admissionMode << "_seed_" << rngRun;

    std::string runDir = SystemPath::Append(outputDir, scheduler);
    SystemPath::MakeDirectories(runDir);

    std::string clientCsvPath = SystemPath::Append(runDir, baseName.str() + "_client.csv");
    std::string orchCsvPath = SystemPath::Append(runDir, baseName.str() + "_orchestrator.csv");
    std::string statsCsvPath = SystemPath::Append(runDir, baseName.str() + "_stats.csv");

    std::ofstream clientCsv(clientCsvPath);
    NS_ABORT_MSG_IF(!clientCsv.is_open(), "Failed to open " << clientCsvPath);
    clientCsv << "clientId,dagId,taskId,inputSize,computeDemand,outputSize,computeTimeMs,"
                 "backendTimeMs,workloadLatencyMs,deadlineBudgetMs\n";

    std::ofstream orchCsv(orchCsvPath);
    NS_ABORT_MSG_IF(!orchCsv.is_open(), "Failed to open " << orchCsvPath);
    orchCsv << "workloadId,taskId,backendIdx\n";

    OrchestratorLogger orchLogger(orchCsv);
    orchestrator->TraceConnectWithoutContext("TaskCompleted",
                                             MakeCallback(&OrchestratorLogger::OnTaskCompleted,
                                                         &orchLogger));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    std::vector<Ptr<OffloadClient>> clients;
    std::vector<std::unique_ptr<ClientLogger>> clientLoggers;

    // Uniform, not Gaussian: [mean*(1-frac), mean*(1+frac)] for each of
    // input size, compute demand, and output size. A fresh RV per client
    // (rather than one shared instance) keeps each client's draws on its own
    // stream instead of interleaving through a single shared generator.
    auto makeUniform = [](double mean, double fraction) {
        Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable>();
        rv->SetAttribute("Min", DoubleValue(mean * (1.0 - fraction)));
        rv->SetAttribute("Max", DoubleValue(mean * (1.0 + fraction)));
        return rv;
    };

    for (uint32_t i = 0; i < numClients; i++)
    {
        OffloadClientHelper clientHelper(InetSocketAddress(orchAddresses[i], orchPort));
        clientHelper.SetTaskRate(taskRate);
        clientHelper.SetLinearChainDagTemplate(2,
                                               makeUniform(meanInputSize, uniformRangeFraction),
                                               makeUniform(meanComputeDemand, uniformRangeFraction),
                                               makeUniform(meanOutputSize, uniformRangeFraction));
        clientHelper.SetDeadlineBudget(MilliSeconds(deadlineBudgetMs));
        clientHelper.SetAdmissionMode(admissionModeEnum);
        clientHelper.SetGracePeriod(gracePeriod);
        // Explicit, even though it is also OffloadClient's own default: never
        // generate (and thus never send) a new workload while a prior one is
        // still awaiting its response.
        clientHelper.SetSubmissionPolicy(CreateObject<DropWhilePendingSubmissionPolicy>());

        ApplicationContainer clientApps = clientHelper.Install(clientNodes.Get(i));
        Ptr<OffloadClient> client = DynamicCast<OffloadClient>(clientApps.Get(0));
        clients.push_back(client);

        auto logger = std::make_unique<ClientLogger>(i, clientCsv, deadlineBudgetMs);
        client->TraceConnectWithoutContext("WorkloadProcessed",
                                           MakeCallback(&ClientLogger::OnWorkloadProcessed,
                                                       logger.get()));
        clientLoggers.push_back(std::move(logger));

        clientApps.Start(Seconds(0.1 + i * 0.001));
        clientApps.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime + gracePeriod.GetSeconds() + 2.0));
    Simulator::Run();

    uint64_t totalTasksSent = 0;
    uint64_t totalResponsesReceived = 0;
    uint64_t totalTasksDropped = 0;
    for (const auto& client : clients)
    {
        totalTasksSent += client->GetTasksSent();
        totalResponsesReceived += client->GetResponsesReceived();
        totalTasksDropped += client->GetTasksDropped();
    }

    // Per-run workload stats (not derivable from the per-task CSVs: dropped
    // workloads never reach a task/backend and so never appear as a row
    // there) -- written to disk, not stdout, so a sweep's results are fully
    // captured on disk rather than scattered across terminal output.
    std::ofstream statsCsv(statsCsvPath);
    NS_ABORT_MSG_IF(!statsCsv.is_open(), "Failed to open " << statsCsvPath);
    statsCsv << "scheduler,tier2DelayMs,admissionMode,rngRun,tasksSent,responsesReceived,"
                "tasksDropped,workloadsAdmitted,workloadsCompleted\n";
    statsCsv << scheduler << ',' << tier2DelayMs << ',' << admissionMode << ',' << rngRun << ','
             << totalTasksSent << ',' << totalResponsesReceived << ',' << totalTasksDropped << ','
             << orchestrator->GetWorkloadsAdmitted() << ','
             << orchestrator->GetWorkloadsCompleted() << '\n';

    NS_LOG_UNCOND("Logs written to: " << clientCsvPath << ", " << orchCsvPath << ", "
                                      << statsCsvPath);

    // Guarantee logs are durable on disk before teardown/exit.
    clientCsv.flush();
    clientCsv.close();
    orchCsv.flush();
    orchCsv.close();
    statsCsv.flush();
    statsCsv.close();

    Simulator::Destroy();

    return 0;
}
