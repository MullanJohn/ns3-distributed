/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: a.h.zahran@gmail.com
 */

#include "ns3/backend-server.h"
#include "ns3/buildings-module.h"
#include "ns3/cluster.h"
#include "ns3/core-module.h"
#include "ns3/device-manager.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/energy-module.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/internet-module.h"
#include "ns3/least-loaded-scheduler.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/orchestrator.h"
#include "ns3/point-to-point-module.h"
#include "ns3/propagation-module.h"
#include "ns3/simple-task.h"
#include "ns3/spectrum-module.h"
#include "ns3/system-path.h"
#include "ns3/task.h"
#include "ns3/wifi-module.h"
#include "ns3/wifi-radio-energy-model-helper.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <vector>

// ===========================================================================
//
//  Continuum WiFi Showcase: depth-estimation offload over WiFi 6/7
//
//  n clients (STAs) generate periodic camera frames and offload a single
//  depth-estimation task per frame, over a WiFi 7 (802.11be) AP, to one
//  backend node (Orchestrator + BackendServer, RTX 5080) reached via one
//  wired backhaul hop:
//
//    STA_0 ──┐
//    STA_1 ──┤ WiFi7, single 6GHz link         P2P backhaul
//     ...    ├──────────────── AP ──────────────────── Server
//    STA_n ──┘                                (Orchestrator + BackendServer,
//                                               talking loopback)
//
//  Single 6GHz link, --channelWidthMhz wide (default 160 -- not the
//  nominally-EHT 320MHz, which this ns-3.44 build's ChannelSettings checker
//  rejects outright; confirmed even the project's own flagship EHT example
//  never exceeds 160MHz).
//  --macVariant selects baseline (EDCA contention) vs ulofdma
//  (RrMultiUserScheduler with UL-OFDMA + BSRP).
//  --inputSizeMode/--outputSizeMode/--computeDemandMode select fixed vs
//  uniform distributions for uplink/downlink/compute; the "uncompressed" vs
//  "compressed" experiment scenarios are sweep-script presets over this same
//  general surface (see run_sweep.py), not special-cased here.
//
// ===========================================================================

using namespace ns3;
using namespace ns3::energy;

NS_LOG_COMPONENT_DEFINE("ContinuumWifiShowcaseExample");

namespace
{

/**
 * @brief Shared cross-application timing store: taskId -> (dispatch, complete)
 * instants, filled from Orchestrator traces and read back by each client's
 * FrameLogger to derive uplink/inference/downlink latency components.
 */
class TimingRecorder
{
  public:
    void RecordDispatched(uint64_t /* workloadId */, uint64_t taskId, uint32_t /* backendIdx */)
    {
        m_dispatch[taskId] = Simulator::Now();
    }

    void RecordCompleted(uint64_t /* workloadId */, uint64_t taskId, uint32_t /* backendIdx */)
    {
        m_complete[taskId] = Simulator::Now();
    }

    bool Lookup(uint64_t taskId, Time& dispatchTime, Time& completeTime) const
    {
        auto d = m_dispatch.find(taskId);
        auto c = m_complete.find(taskId);
        if (d == m_dispatch.end() || c == m_complete.end())
        {
            return false;
        }
        dispatchTime = d->second;
        completeTime = c->second;
        return true;
    }

    void Erase(uint64_t taskId)
    {
        m_dispatch.erase(taskId);
        m_complete.erase(taskId);
    }

  private:
    std::map<uint64_t, Time> m_dispatch;
    std::map<uint64_t, Time> m_complete;
};

/**
 * @brief Buffers WifiPhy::MonitorSnifferRx samples and averages SNR/reports
 * the last MCS observed within a caller-specified time window.
 *
 * Connected once per relevant node (AP for uplink RX, each STA for downlink
 * RX). Samples are not pruned: run lengths here (seconds) keep this bounded.
 */
class PhySampler
{
  public:
    void OnMonitorSniffRx(Ptr<const Packet> /* packet */,
                          uint16_t /* channelFreqMhz */,
                          WifiTxVector txVector,
                          MpduInfo /* aMpdu */,
                          SignalNoiseDbm signalNoise,
                          uint16_t staId)
    {
        // GetMode() defaults staId to SU_STA_ID (65535) when omitted, which
        // WifiTxVector::GetMode() rejects for genuinely MU-format PPDUs
        // (wifi-tx-vector.cc:133, "STA-ID should be correctly set for MU") --
        // must pass the real staId this callback already receives.
        m_samples.push_back({Simulator::Now(),
                             signalNoise.signal - signalNoise.noise,
                             txVector.GetMode(staId).GetUniqueName()});
    }

    // m_samples is append-only in non-decreasing Simulator::Now() order, so
    // the [start,end] window is a contiguous range findable by binary search
    // -- a linear scan here would be O(total samples ever seen) per frame,
    // which is quadratic overall as the run progresses.
    bool AverageWindow(Time start, Time end, double& avgSnrDb, std::string& lastMcs) const
    {
        auto lo = std::lower_bound(m_samples.begin(),
                                   m_samples.end(),
                                   start,
                                   [](const Sample& s, Time t) { return s.time < t; });
        auto hi = std::upper_bound(m_samples.begin(),
                                   m_samples.end(),
                                   end,
                                   [](Time t, const Sample& s) { return t < s.time; });
        double sum = 0.0;
        uint32_t n = 0;
        for (auto it = lo; it != hi; ++it)
        {
            sum += it->snrDb;
            n++;
            lastMcs = it->mcs;
        }
        if (n == 0)
        {
            return false;
        }
        avgSnrDb = sum / n;
        return true;
    }

  private:
    struct Sample
    {
        Time time;
        double snrDb;
        std::string mcs;
    };

    std::vector<Sample> m_samples;
};

/**
 * @brief Buffers QosTxop::TxopTrace (start, duration) intervals for one
 * node's Best-Effort queue and counts how many overlap a given window.
 *
 * For a STA's own queue this is an exact per-client uplink TXOP count (a STA
 * only ever transmits its own traffic). For the AP's shared downlink queue
 * this is an approximation under concurrent multi-client load: a TXOP that
 * also served another client's frame in the same window is still counted
 * (no per-destination TXOP attribution exists in this trace) -- see
 * `downlinkTxopsInWindow`'s column note in the plan/README.
 */
class TxopCounter
{
  public:
    void OnTxop(Time start, Time duration, uint8_t /* linkId */)
    {
        m_intervals.push_back({start, start + duration});
    }

    // m_intervals is append-only ordered by TXOP start time. A single TXOP
    // is bounded well under kMaxTxopDuration, so [start-guard, end] brackets
    // every interval that could possibly overlap [start,end]; only that
    // bounded slice is scanned instead of the full (unboundedly growing)
    // history.
    uint32_t CountOverlapping(Time start, Time end) const
    {
        static const Time kMaxTxopDuration = MilliSeconds(10);
        auto lo = std::lower_bound(
            m_intervals.begin(),
            m_intervals.end(),
            start - kMaxTxopDuration,
            [](const std::pair<Time, Time>& iv, Time t) { return iv.first < t; });
        auto hi = std::upper_bound(
            m_intervals.begin(),
            m_intervals.end(),
            end,
            [](Time t, const std::pair<Time, Time>& iv) { return t < iv.first; });
        uint32_t count = 0;
        for (auto it = lo; it != hi; ++it)
        {
            if (it->first <= end && it->second >= start)
            {
                count++;
            }
        }
        return count;
    }

  private:
    std::vector<std::pair<Time, Time>> m_intervals;
};

/**
 * @brief Writes one CSV row per generation attempt (success/late/dropped/
 * rejected) for one client, joining orchestrator-side dispatch/completion
 * timestamps (via TimingRecorder) and WiFi PHY/MAC instrumentation (via
 * PhySampler/TxopCounter) onto the task's own recorded fields.
 */
class FrameLogger
{
  public:
    FrameLogger(uint32_t clientId,
               std::ofstream& out,
               double deadlineBudgetMs,
               TimingRecorder& timing,
               const PhySampler& uplinkPhy,
               const PhySampler& downlinkPhy,
               const TxopCounter& uplinkTxop,
               const TxopCounter& downlinkTxop)
        : m_clientId(clientId),
          m_out(out),
          m_deadlineBudgetMs(deadlineBudgetMs),
          m_timing(timing),
          m_uplinkPhy(uplinkPhy),
          m_downlinkPhy(downlinkPhy),
          m_uplinkTxop(uplinkTxop),
          m_downlinkTxop(downlinkTxop)
    {
    }

    void OnWorkloadViolated(Ptr<const DagTask> dag, Time /* latency */)
    {
        m_pendingLate.insert(dag->GetTask(0)->GetTaskId());
    }

    void OnWorkloadProcessed(Ptr<const DagTask> dag, Time latency)
    {
        Ptr<Task> task = dag->GetTask(0);
        uint64_t taskId = task->GetTaskId();
        bool late = m_pendingLate.erase(taskId) > 0;
        WriteRow(task, taskId, late ? "late" : "success", latency);
    }

    void OnTaskRejected(Ptr<const Task> task)
    {
        WriteRow(task, task->GetTaskId(), "rejected", Seconds(0));
    }

    void OnTaskDropped(uint64_t taskIndex)
    {
        m_out << m_clientId << ',' << taskIndex << ",dropped,,,,,,,,,,,,,,,," << m_deadlineBudgetMs
              << '\n';
    }

  private:
    static std::string Fmt(double v)
    {
        std::ostringstream oss;
        oss << v;
        return oss.str();
    }

    void WriteRow(Ptr<const Task> task, uint64_t taskId, const std::string& status, Time latency)
    {
        double backendMs = task->GetBackendTime().GetSeconds() * 1000.0;
        double computeMs = task->GetComputeTime().GetSeconds() * 1000.0;
        double queueingMs = backendMs - computeMs;

        std::string uplinkMs;
        std::string downlinkMs;
        std::string uplinkSnr;
        std::string uplinkMcs;
        std::string downlinkSnr;
        std::string downlinkMcs;
        std::string uplinkTxops;
        std::string downlinkTxops;

        Time dispatchTime;
        Time completeTime;
        if (m_timing.Lookup(taskId, dispatchTime, completeTime))
        {
            // dispatchTime/completeTime are t1/t2; t0 (uplink start) is not
            // separately available here, so uplink is derived from the
            // task's own backend-side accounting instead: latency minus
            // backend time minus downlink gives uplink, which is exact
            // since latency, backendTime, and downlink (now - t2) are all
            // independently known.
            Time now = Simulator::Now();
            double downlinkSeconds = (now - completeTime).GetSeconds();
            double uplinkSeconds =
                latency.GetSeconds() - (backendMs / 1000.0) - downlinkSeconds;
            uplinkMs = Fmt(uplinkSeconds * 1000.0);
            downlinkMs = Fmt(downlinkSeconds * 1000.0);

            double snr;
            std::string mcs;
            // Uplink window: [dispatchTime - backendTime-free upload gap] is
            // unknown precisely without t0, so sample using [now - latency,
            // dispatchTime] as the uplink transfer window.
            Time t0Approx = Simulator::Now() - latency;
            if (m_uplinkPhy.AverageWindow(t0Approx, dispatchTime, snr, mcs))
            {
                uplinkSnr = Fmt(snr);
                uplinkMcs = mcs;
            }
            if (m_downlinkPhy.AverageWindow(completeTime, now, snr, mcs))
            {
                downlinkSnr = Fmt(snr);
                downlinkMcs = mcs;
            }
            uplinkTxops = Fmt(m_uplinkTxop.CountOverlapping(t0Approx, dispatchTime));
            downlinkTxops = Fmt(m_downlinkTxop.CountOverlapping(completeTime, now));
        }

        m_out << m_clientId << ',' << taskId << ',' << status << ',' << task->GetInputSize() << ','
              << task->GetOutputSize() << ',' << task->GetComputeDemand() << ',' << uplinkMs << ','
              << backendMs << ',' << queueingMs << ',' << computeMs << ',' << downlinkMs << ','
              << (latency.GetSeconds() * 1000.0) << ',' << uplinkSnr << ',' << uplinkMcs << ','
              << downlinkSnr << ',' << downlinkMcs << ',' << uplinkTxops << ',' << downlinkTxops
              << ',' << m_deadlineBudgetMs << '\n';

        m_timing.Erase(taskId);
    }

    uint32_t m_clientId;
    std::ofstream& m_out;
    double m_deadlineBudgetMs;
    TimingRecorder& m_timing;
    const PhySampler& m_uplinkPhy;
    const PhySampler& m_downlinkPhy;
    const TxopCounter& m_uplinkTxop;
    const TxopCounter& m_downlinkTxop;
    std::set<uint64_t> m_pendingLate;
};

} // namespace

int
main(int argc, char* argv[])
{
    // -----------------------------------------------------------------
    // 1. Parameters
    // -----------------------------------------------------------------
    uint32_t numClients = 4;
    double simTime = 5.0;

    std::string propagationVariant = "3GPP-Indoor";
    bool enableShadowing = true;
    double referenceLoss = 40.0;

    std::string errorModelVariant = "TableBased";
    double noiseFigureDb = 7.0;
    double ccaEdThresholdDbm = -62.0;
    double rxSensitivityDbm = -82.0;

    std::string rateControlVariant = "Ideal";
    std::string constantDataMode = "EhtMcs11";
    std::string constantControlMode = "OfdmRate6Mbps";

    // baseline: no MultiUserScheduler (pure EDCA contention). ulofdma:
    // RrMultiUserScheduler with UL-OFDMA + BSRP. (A prior "STA-ID should be
    // correctly set for MU" abort under ulofdma was a bug in this file's own
    // PhySampler -- GetMode() defaulted staId to SU_STA_ID instead of using
    // the real staId the MonitorSnifferRx callback receives -- now fixed.)
    std::string macVariant = "baseline";
    std::string schedulerType = "ns3::RrMultiUserScheduler";
    bool enableUlOfdma = true;
    bool enableBsrp = true;
    uint32_t mpduBufferSize = 1024;
    uint32_t maxAmpduSize = 3839148;

    // NOTE: this ns-3.44 build's WifiPhy::ChannelSettings attribute hard-caps
    // channel width to <=160MHz (MakeUintegerChecker<MHz_u>(0,160) in
    // wifi-phy.cc) -- confirmed 320MHz is not configurable via this API even
    // in the project's own flagship EHT example. Defaulting to 160 (the real
    // ceiling in this build) rather than the nominal EHT 320.
    uint16_t channelWidthMhz = 160;

    std::string mobilityVariant = "RandomDisc";
    double areaRadius = 15.0;
    double staSpeedMin = 1.0;
    double staSpeedMax = 2.0;

    double backhaulDelayMs = 1.0;
    std::string backhaulDataRate = "1Gbps";

    double frameRateHz = 30.0;
    std::string inputSizeMode = "fixed";
    double inputSizeMean = 691200.0; // 640x360 RGB uncompressed frame
    double inputSizeRangeFraction = 0.10;
    std::string outputSizeMode = "fixed";
    double outputSizeMean = 460800.0; // 640x360 single-channel 16-bit depth map
    double outputSizeRangeFraction = 0.10;
    std::string computeDemandMode = "uniform";
    double computeDemandMean = 1.04e11; // MiDaS-class monocular depth model (~103.9 GFLOPs)
    double computeDemandRangeFraction = 0.10;
    double deadlineBudgetMs = 0.0; // 0 = auto (1/frameRateHz)

    double gpuComputeRate = 56.3e12;     // RTX 5080 FP32 TFLOPS
    double gpuMemoryBandwidth = 960e9;   // RTX 5080 GDDR7 bandwidth
    double gpuTransferBandwidth = 63e9;  // PCIe 5.0 x16 effective throughput

    double apRadioCurrentMultiplier = 2.0;

    std::string outputDir = "contrib/continuum/examples/wifi-showcase/results";
    // Free-text tag written into the output filename and as a `scenario`
    // stats.csv column -- set by the sweep scripts (e.g. "uncompressed",
    // "compressed", "bufferStudy") so downstream plotting reads scenario
    // directly instead of inferring it from byte-size heuristics.
    std::string scenarioLabel = "custom";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numClients", "Number of client STAs", numClients);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("propagationVariant",
                 "Propagation: 3GPP-Indoor, HybridBuildings, LogDistance",
                 propagationVariant);
    cmd.AddValue("enableShadowing", "Toggle shadow fading", enableShadowing);
    cmd.AddValue("referenceLoss", "LogDistance reference loss", referenceLoss);
    cmd.AddValue("errorModelVariant", "Error model: TableBased, Nist", errorModelVariant);
    cmd.AddValue("noiseFigureDb", "PHY RxNoiseFigure in dB", noiseFigureDb);
    cmd.AddValue("ccaEdThresholdDbm", "CCA energy-detect threshold in dBm", ccaEdThresholdDbm);
    cmd.AddValue("rxSensitivityDbm", "PHY RX sensitivity in dBm", rxSensitivityDbm);
    cmd.AddValue("rateControlVariant",
                 "Rate manager: Ideal, Constant, ThompsonSampling",
                 rateControlVariant);
    cmd.AddValue("constantDataMode", "Fixed MCS string if using Constant", constantDataMode);
    cmd.AddValue("constantControlMode", "Fixed control-frame rate if using Constant", constantControlMode);
    cmd.AddValue("macVariant", "AP MAC variant: baseline (EDCA contention) or ulofdma", macVariant);
    cmd.AddValue("schedulerType", "AP MultiUserScheduler TypeId (ulofdma only)", schedulerType);
    cmd.AddValue("enableUlOfdma", "Enable OFDMA triggering for uplink (ulofdma only)", enableUlOfdma);
    cmd.AddValue("enableBsrp", "Enable Buffer Status Report polling (ulofdma only)", enableBsrp);
    cmd.AddValue("mpduBufferSize", "BlockAck reception buffer depth", mpduBufferSize);
    cmd.AddValue("maxAmpduSize", "Maximum A-MPDU aggregation limit (bytes)", maxAmpduSize);
    cmd.AddValue("channelWidthMhz", "6 GHz channel width in MHz", channelWidthMhz);
    cmd.AddValue("mobilityVariant", "Mobility: RandomDisc, Grid, RandomWalk", mobilityVariant);
    cmd.AddValue("areaRadius", "Distribution radius/boundary for STAs", areaRadius);
    cmd.AddValue("staSpeedMin", "RandomWalk min speed (m/s)", staSpeedMin);
    cmd.AddValue("staSpeedMax", "RandomWalk max speed (m/s)", staSpeedMax);
    cmd.AddValue("backhaulDelayMs", "AP<->Server backhaul link delay in ms", backhaulDelayMs);
    cmd.AddValue("backhaulDataRate", "AP<->Server backhaul link data rate", backhaulDataRate);
    cmd.AddValue("frameRateHz", "Per-client frame generation rate in Hz", frameRateHz);
    cmd.AddValue("inputSizeMode", "fixed or uniform: uplink frame size distribution", inputSizeMode);
    cmd.AddValue("inputSizeMean", "Mean uplink frame size in bytes", inputSizeMean);
    cmd.AddValue("inputSizeRangeFraction",
                 "Uniform uplink half-width as a fraction of the mean",
                 inputSizeRangeFraction);
    cmd.AddValue("outputSizeMode",
                 "fixed or uniform: downlink depth-map size distribution",
                 outputSizeMode);
    cmd.AddValue("outputSizeMean", "Mean downlink depth-map size in bytes", outputSizeMean);
    cmd.AddValue("outputSizeRangeFraction",
                 "Uniform downlink half-width as a fraction of the mean",
                 outputSizeRangeFraction);
    cmd.AddValue("computeDemandMode",
                 "fixed or uniform: compute-demand distribution",
                 computeDemandMode);
    cmd.AddValue("computeDemandMean", "Mean compute demand in FLOPs", computeDemandMean);
    cmd.AddValue("computeDemandRangeFraction",
                 "Uniform compute-demand half-width as a fraction of the mean",
                 computeDemandRangeFraction);
    cmd.AddValue("deadlineBudgetMs", "End-to-end deadline budget in ms (0 = auto)", deadlineBudgetMs);
    cmd.AddValue("gpuComputeRate", "Backend GPU compute rate in FLOPS", gpuComputeRate);
    cmd.AddValue("gpuMemoryBandwidth", "Backend GPU memory bandwidth in bytes/sec", gpuMemoryBandwidth);
    cmd.AddValue("gpuTransferBandwidth",
                 "Backend GPU host-device transfer bandwidth in bytes/sec",
                 gpuTransferBandwidth);
    cmd.AddValue("apRadioCurrentMultiplier",
                 "AP radio current draw as a multiple of the STA radio's base currents",
                 apRadioCurrentMultiplier);
    cmd.AddValue("outputDir", "Root directory for per-run CSV logs", outputDir);
    cmd.AddValue("scenarioLabel",
                 "Free-text scenario tag written into the output filename and stats.csv",
                 scenarioLabel);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(macVariant != "baseline" && macVariant != "ulofdma",
                    "Unsupported --macVariant '" << macVariant << "'; expected baseline or ulofdma");

    // ns-3's TCP socket rejects (rather than partial-accepts) any single
    // Send() call whose payload exceeds SndBufSize/RcvBufSize (default
    // 128KB) -- far smaller than an uncompressed camera frame. Size for the
    // largest configured payload with headroom.
    uint32_t maxPayloadBytes = static_cast<uint32_t>(
        std::max({inputSizeMean * (1.0 + inputSizeRangeFraction),
                  outputSizeMean * (1.0 + outputSizeRangeFraction),
                  1.0e5}) *
        1.5);
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(maxPayloadBytes));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(maxPayloadBytes));
    // 1500-byte MTU minus IP/TCP headers, so AMPDU-aggregation tuning (bytes
    // per aggregate vs. MSS) has a concrete, realistic MSDU count to reason
    // about instead of ns-3's conservative 536-byte TCP default.
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));

    Time deadlineBudget =
        deadlineBudgetMs > 0.0 ? MilliSeconds(deadlineBudgetMs) : Seconds(0.0);
    double effectiveDeadlineBudgetMs =
        deadlineBudgetMs > 0.0 ? deadlineBudgetMs : (1000.0 / frameRateHz);

    // -----------------------------------------------------------------
    // 2. Nodes
    // -----------------------------------------------------------------
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(numClients);
    NodeContainer serverNode;
    serverNode.Create(1);

    // -----------------------------------------------------------------
    // 3. Multi-link spectrum channel + PHY
    // -----------------------------------------------------------------
    // Built manually per link (not via SpectrumChannelHelper::Default(),
    // which also silently chains a FriisSpectrumPropagationLossModel on top
    // of whatever loss model is added -- double-counting free-space loss on
    // top of an already-complete model like LogDistance/ThreeGpp and pushing
    // received power below sensitivity even at short range). Matches the
    // manual-construction pattern in examples/wireless/wifi-eht-network.cc.
    auto makeChannel = [&]() {
        Ptr<MultiModelSpectrumChannel> channel = CreateObject<MultiModelSpectrumChannel>();
        channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
        Ptr<PropagationLossModel> lossModel;
        if (propagationVariant == "3GPP-Indoor")
        {
            Ptr<ThreeGppIndoorOfficePropagationLossModel> m =
                CreateObject<ThreeGppIndoorOfficePropagationLossModel>();
            m->SetAttribute("ShadowingEnabled", BooleanValue(enableShadowing));
            lossModel = m;
        }
        else if (propagationVariant == "HybridBuildings")
        {
            Ptr<HybridBuildingsPropagationLossModel> m =
                CreateObject<HybridBuildingsPropagationLossModel>();
            m->SetAttribute("ShadowSigmaIndoor", DoubleValue(enableShadowing ? 7.0 : 0.0));
            lossModel = m;
        }
        else if (propagationVariant == "LogDistance")
        {
            Ptr<LogDistancePropagationLossModel> m = CreateObject<LogDistancePropagationLossModel>();
            m->SetAttribute("ReferenceLoss", DoubleValue(referenceLoss));
            m->SetAttribute("Exponent", DoubleValue(3.0));
            lossModel = m;
        }
        else
        {
            NS_FATAL_ERROR("Unsupported propagation model: " << propagationVariant);
        }
        channel->AddPropagationLossModel(lossModel);
        return channel;
    };

    // Channel number 0 = let ns-3 auto-select a valid primary channel for the
    // given band/width (matches examples/wireless/wifi-eht-network.cc).
    std::string channelSettings = "{0, " + std::to_string(channelWidthMhz) + ", BAND_6GHZ, 0}";

    std::string errModelClass =
        (errorModelVariant == "TableBased") ? "ns3::TableBasedErrorRateModel" : "ns3::NistErrorRateModel";

    SpectrumWifiPhyHelper phy;
    phy.SetErrorRateModel(errModelClass);
    phy.Set("RxNoiseFigure", DoubleValue(noiseFigureDb));
    phy.Set("CcaEdThreshold", DoubleValue(ccaEdThresholdDbm));
    phy.Set("RxSensitivity", DoubleValue(rxSensitivityDbm));
    phy.Set("Antennas", UintegerValue(4));
    phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
    phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));
    phy.Set("ChannelSettings", StringValue(channelSettings));
    phy.AddChannel(makeChannel(), WIFI_SPECTRUM_6_GHZ);

    // -----------------------------------------------------------------
    // 4. Wifi standard, rate control, MAC (AP MLD + STA MLD)
    // -----------------------------------------------------------------
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);

    std::string rateManagerType;
    if (rateControlVariant == "Ideal")
    {
        rateManagerType = "ns3::IdealWifiManager";
    }
    else if (rateControlVariant == "Constant")
    {
        rateManagerType = "ns3::ConstantRateWifiManager";
    }
    else if (rateControlVariant == "ThompsonSampling")
    {
        rateManagerType = "ns3::ThompsonSamplingWifiManager";
    }
    else
    {
        NS_FATAL_ERROR("Unsupported rate manager: " << rateControlVariant);
    }

    if (rateControlVariant == "Constant")
    {
        wifi.SetRemoteStationManager(rateManagerType,
                                     "DataMode",
                                     StringValue(constantDataMode),
                                     "ControlMode",
                                     StringValue(constantControlMode));
    }
    else
    {
        wifi.SetRemoteStationManager(rateManagerType);
    }

    Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(maxAmpduSize));
    Config::SetDefault("ns3::WifiMac::MpduBufferSize", UintegerValue(mpduBufferSize));

    Ssid ssid = Ssid("continuum-wifi-showcase");
    WifiMacHelper macHelper;

    macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, macHelper, staNodes);

    if (macVariant == "ulofdma")
    {
        macHelper.SetMultiUserScheduler(schedulerType,
                                        "EnableUlOfdma",
                                        BooleanValue(enableUlOfdma),
                                        "EnableBsrp",
                                        BooleanValue(enableBsrp));
    }
    macHelper.SetType("ns3::ApWifiMac",
                      "EnableBeaconJitter",
                      BooleanValue(true),
                      "BeaconInterval",
                      TimeValue(MicroSeconds(102400)), // multiple of 1024us per 802.11-2012
                      "Ssid",
                      SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(phy, macHelper, apNode);

    // -----------------------------------------------------------------
    // 5. Mobility (+ Building, if HybridBuildings)
    // -----------------------------------------------------------------
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> apPosition = CreateObject<ListPositionAllocator>();
    apPosition->Add(Vector(0.0, 0.0, 3.0));
    mobility.SetPositionAllocator(apPosition);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    if (mobilityVariant == "RandomDisc")
    {
        mobility.SetPositionAllocator(
            "ns3::RandomDiscPositionAllocator",
            "X",
            StringValue("0.0"),
            "Y",
            StringValue("0.0"),
            "Rho",
            StringValue("ns3::UniformRandomVariable[Min=1.0|Max=" + std::to_string(areaRadius) + "]"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }
    else if (mobilityVariant == "Grid")
    {
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "GridWidth",
                                      UintegerValue(5),
                                      "MinX",
                                      DoubleValue(-areaRadius),
                                      "MinY",
                                      DoubleValue(-areaRadius),
                                      "DeltaX",
                                      DoubleValue(areaRadius / 2.0),
                                      "DeltaY",
                                      DoubleValue(areaRadius / 2.0));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }
    else if (mobilityVariant == "RandomWalk")
    {
        mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                      "X",
                                      StringValue("0.0"),
                                      "Y",
                                      StringValue("0.0"),
                                      "Rho",
                                      StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"));
        mobility.SetMobilityModel(
            "ns3::RandomWalk2dMobilityModel",
            "Mode",
            StringValue("Time"),
            "Time",
            StringValue("2s"),
            "Speed",
            StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(staSpeedMin) +
                       "|Max=" + std::to_string(staSpeedMax) + "]"),
            "Bounds",
            RectangleValue(Rectangle(-areaRadius, areaRadius, -areaRadius, areaRadius)));
    }
    else
    {
        NS_FATAL_ERROR("Unsupported mobility model: " << mobilityVariant);
    }
    mobility.Install(staNodes);

    if (propagationVariant == "HybridBuildings")
    {
        Ptr<Building> building = CreateObject<Building>();
        building->SetBoundaries(
            Box(-areaRadius, areaRadius, -areaRadius, areaRadius, 0.0, 4.0));
        BuildingsHelper::Install(apNode);
        BuildingsHelper::Install(staNodes);
    }

    // -----------------------------------------------------------------
    // 6. Backhaul (AP <-> Server), Internet stack, addressing, routing
    // -----------------------------------------------------------------
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(backhaulDataRate));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(backhaulDelayMs)));
    NetDeviceContainer backhaulDevices = p2p.Install(apNode.Get(0), serverNode.Get(0));

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);
    stack.Install(serverNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiApInterfaces = address.Assign(apDevices);
    address.Assign(staDevices);

    address.SetBase("10.1.2.0", "255.255.255.252");
    Ipv4InterfaceContainer backhaulInterfaces = address.Assign(backhaulDevices);

    // Static default routes through the AP (the only router) rather than
    // Ipv4GlobalRoutingHelper: the topology is a simple star (STA -> AP ->
    // Server) needing no shortest-path computation, and this sidesteps
    // global routing's NetDevice::GetChannel()-based neighbor discovery
    // entirely (which historically broke for multi-link WiFi devices when
    // this example still supported MLO).
    Ipv4StaticRoutingHelper staticRouting;
    for (uint32_t i = 0; i < numClients; i++)
    {
        Ptr<Ipv4StaticRouting> staRouting =
            staticRouting.GetStaticRouting(staNodes.Get(i)->GetObject<Ipv4>());
        staRouting->SetDefaultRoute(wifiApInterfaces.GetAddress(0), 1);
    }
    Ptr<Ipv4StaticRouting> serverRouting =
        staticRouting.GetStaticRouting(serverNode.Get(0)->GetObject<Ipv4>());
    serverRouting->SetDefaultRoute(backhaulInterfaces.GetAddress(0), 1);

    // -----------------------------------------------------------------
    // 7. Backend (RTX 5080) + Orchestrator + BackendServer (one server node)
    // -----------------------------------------------------------------
    Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
    gpu->SetAttribute("ComputeRate", DoubleValue(gpuComputeRate));
    gpu->SetAttribute("MemoryBandwidth", DoubleValue(gpuMemoryBandwidth));
    gpu->SetAttribute("TransferBandwidth", DoubleValue(gpuTransferBandwidth));
    gpu->SetAttribute("ProcessingModel", PointerValue(CreateObject<FixedRatioProcessingModel>()));
    gpu->SetAttribute("QueueScheduler", PointerValue(CreateObject<FifoQueueScheduler>()));

    // RTX 5080 energy: TDP=360W sourced; StaticPower/EffectiveCapacitance are
    // back-derived (not a published spec) so 100%-utilization power matches
    // the 360W TDP at the accelerator's default 1.5GHz/1.0V operating point.
    Ptr<DvfsEnergyModel> gpuEnergyModel = CreateObject<DvfsEnergyModel>();
    double gpuStaticPowerW = 20.0;
    double gpuTdpW = 360.0;
    double gpuDefaultFrequencyHz = 1.5e9;
    double gpuDefaultVoltageV = 1.0;
    gpuEnergyModel->SetAttribute("StaticPower", DoubleValue(gpuStaticPowerW));
    gpuEnergyModel->SetAttribute(
        "EffectiveCapacitance",
        DoubleValue((gpuTdpW - gpuStaticPowerW) /
                    (gpuDefaultVoltageV * gpuDefaultVoltageV * gpuDefaultFrequencyHz)));
    gpu->SetAttribute("EnergyModel", PointerValue(gpuEnergyModel));

    serverNode.Get(0)->AggregateObject(gpu);

    uint16_t backendPort = 9000;
    Ptr<BackendServer> backendServer = CreateObject<BackendServer>();
    backendServer->SetAttribute("Port", UintegerValue(backendPort));
    serverNode.Get(0)->AddApplication(backendServer);
    backendServer->SetStartTime(Seconds(0.0));
    backendServer->SetStopTime(Seconds(simTime + 2.0));

    Cluster cluster;
    cluster.AddBackend(serverNode.Get(0),
                       InetSocketAddress(Ipv4Address("127.0.0.1"), backendPort),
                       "",
                       MilliSeconds(0.01),
                       100e9);

    uint16_t orchPort = 8080;
    Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(CreateObject<LeastLoadedScheduler>()));
    // Seeds ClusterState's compute capacity from the GpuAccelerator's
    // ComputeRate; no ScalingPolicy is set, so this is capacity-seeding only.
    orchestrator->SetAttribute("DeviceManager", PointerValue(CreateObject<DeviceManager>()));
    orchestrator->SetCluster(cluster);
    serverNode.Get(0)->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(simTime + 2.0));

    // -----------------------------------------------------------------
    // 8. Communication energy: WiFi radio energy models (AP + stations)
    // -----------------------------------------------------------------
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1e9)); // effectively unlimited
    EnergySourceContainer apEnergySources = energySourceHelper.Install(apNode);
    EnergySourceContainer staEnergySources = energySourceHelper.Install(staNodes);

    // STA radios use WifiRadioEnergyModel's own library defaults (idle
    // 273mA/tx 380mA/rx 313mA/sleep 33mA @ 3.0V) -- ns-3's own documented
    // figures, not invented here.
    WifiRadioEnergyModelHelper staRadioEnergyHelper;
    DeviceEnergyModelContainer staDeviceEnergyModels =
        staRadioEnergyHelper.Install(staDevices, staEnergySources);

    // AP radio: same base currents scaled by --apRadioCurrentMultiplier to
    // reflect a higher-power, multi-antenna AP radio -- an engineering
    // estimate, not a sourced spec, same as the GPU energy parameters above.
    WifiRadioEnergyModelHelper apRadioEnergyHelper;
    apRadioEnergyHelper.Set("IdleCurrentA", DoubleValue(0.273 * apRadioCurrentMultiplier));
    apRadioEnergyHelper.Set("CcaBusyCurrentA", DoubleValue(0.273 * apRadioCurrentMultiplier));
    apRadioEnergyHelper.Set("TxCurrentA", DoubleValue(0.380 * apRadioCurrentMultiplier));
    apRadioEnergyHelper.Set("RxCurrentA", DoubleValue(0.313 * apRadioCurrentMultiplier));
    apRadioEnergyHelper.Set("SwitchingCurrentA", DoubleValue(0.273 * apRadioCurrentMultiplier));
    apRadioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.033 * apRadioCurrentMultiplier));
    DeviceEnergyModelContainer apDeviceEnergyModels =
        apRadioEnergyHelper.Install(apDevices, apEnergySources);

    // -----------------------------------------------------------------
    // 9. Clients (depth-estimation offload, SimpleTask/NumTasks=1)
    // -----------------------------------------------------------------
    auto makeSizeRv = [](const std::string& mode, double mean, double rangeFraction) {
        if (mode == "fixed")
        {
            Ptr<ConstantRandomVariable> rv = CreateObject<ConstantRandomVariable>();
            rv->SetAttribute("Constant", DoubleValue(mean));
            return StaticCast<RandomVariableStream>(rv);
        }
        Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable>();
        rv->SetAttribute("Min", DoubleValue(mean * (1.0 - rangeFraction)));
        rv->SetAttribute("Max", DoubleValue(mean * (1.0 + rangeFraction)));
        return StaticCast<RandomVariableStream>(rv);
    };

    TimingRecorder timingRecorder;
    PhySampler uplinkPhy; // AP-side: aggregate across all connected STAs' uplinks
    std::vector<PhySampler> downlinkPhy(numClients);
    TxopCounter apTxop; // AP-side downlink queue: shared across all STAs (approximate)
    std::vector<TxopCounter> staTxop(numClients);

    DynamicCast<WifiNetDevice>(apDevices.Get(0))
        ->GetPhy(0)
        ->TraceConnectWithoutContext("MonitorSnifferRx",
                                     MakeCallback(&PhySampler::OnMonitorSniffRx, &uplinkPhy));
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(apNode.Get(0)->GetId()) +
                                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/BE_Txop/TxopTrace",
                                  MakeCallback(&TxopCounter::OnTxop, &apTxop));

    std::string runDir = SystemPath::Append(outputDir, macVariant);
    SystemPath::MakeDirectories(runDir);
    uint32_t rngRun = RngSeedManager::GetRun();
    std::ostringstream baseName;
    baseName << "n" << numClients << "_" << macVariant << "_" << scenarioLabel << "_seed_" << rngRun;

    std::string frameCsvPath = SystemPath::Append(runDir, baseName.str() + "_frames.csv");
    std::ofstream frameCsv(frameCsvPath);
    NS_ABORT_MSG_IF(!frameCsv.is_open(), "Failed to open " << frameCsvPath);
    frameCsv << "clientId,frameIndex,status,inputSizeBytes,outputSizeBytes,computeDemandFlops,"
                "uplinkLatencyMs,backendLatencyMs,queueingLatencyMs,computeLatencyMs,"
                "downlinkLatencyMs,totalLatencyMs,uplinkSnrDb,uplinkMcs,downlinkSnrDb,"
                "downlinkMcs,uplinkTxops,downlinkTxopsInWindow,deadlineBudgetMs\n";

    std::vector<Ptr<OffloadClient>> clients;
    std::vector<std::unique_ptr<FrameLogger>> frameLoggers;

    Ptr<UniformRandomVariable> startTimeRv = CreateObject<UniformRandomVariable>();
    startTimeRv->SetAttribute("Min", DoubleValue(0.0));
    startTimeRv->SetAttribute("Max", DoubleValue(2.0));

    for (uint32_t i = 0; i < numClients; i++)
    {
        OffloadClientHelper clientHelper(InetSocketAddress(backhaulInterfaces.GetAddress(1), orchPort));
        clientHelper.SetTaskRate(frameRateHz);
        clientHelper.SetLinearChainDagTemplate(
            1,
            makeSizeRv(inputSizeMode, inputSizeMean, inputSizeRangeFraction),
            makeSizeRv(computeDemandMode, computeDemandMean, computeDemandRangeFraction),
            makeSizeRv(outputSizeMode, outputSizeMean, outputSizeRangeFraction));
        clientHelper.SetDeadlineBudget(deadlineBudget);

        ApplicationContainer clientApps = clientHelper.Install(staNodes.Get(i));
        Ptr<OffloadClient> client = DynamicCast<OffloadClient>(clientApps.Get(0));
        clients.push_back(client);

        Ptr<WifiNetDevice> staDev = DynamicCast<WifiNetDevice>(staDevices.Get(i));
        staDev->GetPhy(0)->TraceConnectWithoutContext(
            "MonitorSnifferRx", MakeCallback(&PhySampler::OnMonitorSniffRx, &downlinkPhy[i]));
        Config::ConnectWithoutContext(
            "/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) +
                "/DeviceList/0/$ns3::WifiNetDevice/Mac/BE_Txop/TxopTrace",
            MakeCallback(&TxopCounter::OnTxop, &staTxop[i]));

        auto logger = std::make_unique<FrameLogger>(i,
                                                    frameCsv,
                                                    effectiveDeadlineBudgetMs,
                                                    timingRecorder,
                                                    uplinkPhy,
                                                    downlinkPhy[i],
                                                    staTxop[i],
                                                    apTxop);
        client->TraceConnectWithoutContext(
            "WorkloadProcessed",
            MakeCallback(&FrameLogger::OnWorkloadProcessed, logger.get()));
        client->TraceConnectWithoutContext(
            "WorkloadViolated", MakeCallback(&FrameLogger::OnWorkloadViolated, logger.get()));
        client->TraceConnectWithoutContext("TaskRejected",
                                           MakeCallback(&FrameLogger::OnTaskRejected, logger.get()));
        client->TraceConnectWithoutContext("TaskDropped",
                                           MakeCallback(&FrameLogger::OnTaskDropped, logger.get()));
        frameLoggers.push_back(std::move(logger));

        clientApps.Start(Seconds(startTimeRv->GetValue()));
        clientApps.Stop(Seconds(simTime));
    }

    orchestrator->TraceConnectWithoutContext(
        "TaskDispatched", MakeCallback(&TimingRecorder::RecordDispatched, &timingRecorder));
    orchestrator->TraceConnectWithoutContext(
        "TaskCompleted", MakeCallback(&TimingRecorder::RecordCompleted, &timingRecorder));

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();

    // -----------------------------------------------------------------
    // 10. Run-level stats: latency mean/stddev, outcome counts, energy
    // -----------------------------------------------------------------
    frameCsv.flush();
    frameCsv.close();

    std::vector<double> uplinkMs;
    std::vector<double> backendMs;
    std::vector<double> queueingMs;
    std::vector<double> computeMs;
    std::vector<double> downlinkMs;
    std::vector<double> totalMs;
    uint64_t nSuccess = 0;
    uint64_t nLate = 0;
    uint64_t nDropped = 0;
    uint64_t nRejected = 0;

    {
        std::ifstream in(frameCsvPath);
        std::string header;
        std::getline(in, header);
        std::string line;
        while (std::getline(in, line))
        {
            std::vector<std::string> f;
            std::stringstream ss(line);
            std::string field;
            while (std::getline(ss, field, ','))
            {
                f.push_back(field);
            }
            f.resize(19);
            const std::string& status = f[2];
            if (status == "success")
            {
                nSuccess++;
            }
            else if (status == "late")
            {
                nLate++;
            }
            else if (status == "dropped")
            {
                nDropped++;
                continue;
            }
            else if (status == "rejected")
            {
                nRejected++;
            }
            if (!f[6].empty())
            {
                uplinkMs.push_back(std::stod(f[6]));
            }
            if (!f[7].empty())
            {
                backendMs.push_back(std::stod(f[7]));
            }
            if (!f[8].empty())
            {
                queueingMs.push_back(std::stod(f[8]));
            }
            if (!f[9].empty())
            {
                computeMs.push_back(std::stod(f[9]));
            }
            if (!f[10].empty())
            {
                downlinkMs.push_back(std::stod(f[10]));
            }
            if (!f[11].empty())
            {
                totalMs.push_back(std::stod(f[11]));
            }
        }
    }

    auto meanOf = [](const std::vector<double>& v) {
        return v.empty() ? 0.0 : std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    };
    auto stddevOf = [&meanOf](const std::vector<double>& v) {
        if (v.size() < 2)
        {
            return 0.0;
        }
        double m = meanOf(v);
        double sq = 0.0;
        for (double x : v)
        {
            sq += (x - m) * (x - m);
        }
        return std::sqrt(sq / (v.size() - 1));
    };

    double apRadioEnergyJ =
        apDeviceEnergyModels.GetN() > 0 ? apDeviceEnergyModels.Get(0)->GetTotalEnergyConsumption() : 0.0;
    double staRadioEnergyTotalJ = 0.0;
    for (auto iter = staDeviceEnergyModels.Begin(); iter != staDeviceEnergyModels.End(); iter++)
    {
        staRadioEnergyTotalJ += (*iter)->GetTotalEnergyConsumption();
    }
    double computeEnergyJ = gpu->GetTotalEnergy();
    double totalEnergyJ = apRadioEnergyJ + staRadioEnergyTotalJ + computeEnergyJ;

    std::string statsCsvPath = SystemPath::Append(runDir, baseName.str() + "_stats.csv");
    std::ofstream statsCsv(statsCsvPath);
    NS_ABORT_MSG_IF(!statsCsv.is_open(), "Failed to open " << statsCsvPath);
    statsCsv << "numClients,macVariant,scenario,mpduBufferSize,maxAmpduSize,rngRun,nSuccess,"
                "nLate,nDropped,nRejected,"
                "uplinkMeanMs,uplinkStddevMs,backendMeanMs,backendStddevMs,queueingMeanMs,"
                "queueingStddevMs,computeMeanMs,computeStddevMs,downlinkMeanMs,downlinkStddevMs,"
                "totalMeanMs,totalStddevMs,computeEnergyJ,apRadioEnergyJ,staRadioEnergyTotalJ,"
                "totalEnergyJ\n";
    statsCsv << numClients << ',' << macVariant << ',' << scenarioLabel << ',' << mpduBufferSize
             << ',' << maxAmpduSize << ',' << rngRun << ',' << nSuccess << ',' << nLate
             << ',' << nDropped << ',' << nRejected << ',' << meanOf(uplinkMs) << ','
             << stddevOf(uplinkMs) << ',' << meanOf(backendMs) << ',' << stddevOf(backendMs) << ','
             << meanOf(queueingMs) << ',' << stddevOf(queueingMs) << ',' << meanOf(computeMs) << ','
             << stddevOf(computeMs) << ',' << meanOf(downlinkMs) << ',' << stddevOf(downlinkMs) << ','
             << meanOf(totalMs) << ',' << stddevOf(totalMs) << ',' << computeEnergyJ << ','
             << apRadioEnergyJ << ',' << staRadioEnergyTotalJ << ',' << totalEnergyJ << '\n';
    statsCsv.flush();
    statsCsv.close();

    NS_LOG_UNCOND("Logs written to: " << frameCsvPath << ", " << statsCsvPath);

    Simulator::Destroy();
    return 0;
}
