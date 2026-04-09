/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster-state.h"
#include "ns3/cluster.h"
#include "ns3/device-manager.h"
#include "ns3/double.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/gpu-device-protocol.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/pointer.h"
#include "ns3/scaling-policy.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/udp-connection-manager.h"
#include "ns3/utilization-scaling-policy.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test DeviceManager Start, HandleMetrics, EvaluateScaling and the StateChanged trace.
 */
class DeviceManagerTestCase : public TestCase
{
  public:
    DeviceManagerTestCase()
        : TestCase("DeviceManager lifecycle: Start, HandleMetrics, EvaluateScaling"),
          m_traceCount(0),
          m_lastBackendIdx(0),
          m_lastOldStateIdx(0),
          m_lastNewStateIdx(0)
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(3);
        Ptr<Node> orchNode = nodes.Get(0);
        Ptr<Node> backend0 = nodes.Get(1);
        Ptr<Node> backend1 = nodes.Get(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));
        NetDeviceContainer dev01 = p2p.Install(orchNode, backend0);
        NetDeviceContainer dev02 = p2p.Install(orchNode, backend1);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.1.0", "255.255.255.0");
        Ipv4InterfaceContainer if01 = ipv4.Assign(dev01);
        ipv4.SetBase("10.0.2.0", "255.255.255.0");
        Ipv4InterfaceContainer if02 = ipv4.Assign(dev02);

        // 3-entry OPP table -> GetNumPerformanceStates() == 3.
        m_gpu0 = CreateObject<GpuAccelerator>();
        m_gpu0->AddOperatingPoint(1.0e9, 0.9);
        m_gpu0->AddOperatingPoint(1.5e9, 1.0);
        m_gpu0->AddOperatingPoint(2.0e9, 1.1);
        Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
        energy->SetAttribute("StaticPower", DoubleValue(42.5));
        m_gpu0->SetAttribute("EnergyModel", PointerValue(energy));
        backend0->AggregateObject(m_gpu0);

        Ptr<GpuAccelerator> gpu1 = CreateObject<GpuAccelerator>();
        gpu1->AddOperatingPoint(1.0e9, 0.9);
        gpu1->AddOperatingPoint(1.5e9, 1.0);
        gpu1->AddOperatingPoint(2.0e9, 1.1);
        backend1->AggregateObject(gpu1);

        uint16_t port = 9000;
        Cluster cluster;
        cluster.AddBackend(backend0, InetSocketAddress(if01.GetAddress(1), port));
        cluster.AddBackend(backend1, InetSocketAddress(if02.GetAddress(1), port));

        // UDP is connectionless: Send returns true regardless of peer state.
        Ptr<UdpConnectionManager> udp = CreateObject<UdpConnectionManager>();
        udp->SetNode(orchNode);
        udp->Bind(port);

        m_protocol = CreateObject<GpuDeviceProtocol>();
        Ptr<UtilizationScalingPolicy> policy = CreateObject<UtilizationScalingPolicy>();

        m_dm = CreateObject<DeviceManager>();
        m_dm->SetAttribute("ScalingPolicy", PointerValue(policy));
        m_dm->SetAttribute("DeviceProtocol", PointerValue(m_protocol));
        m_dm->TraceConnectWithoutContext(
            "StateChanged",
            MakeCallback(&DeviceManagerTestCase::OnStateChanged, this));

        m_state.Resize(2);

        m_dm->Start(cluster, udp, m_state);
        NS_TEST_ASSERT_MSG_EQ(m_state.Get(0).device.numPerformanceStates,
                              3,
                              "Backend 0 should have 3 performance states from OPP table");
        NS_TEST_ASSERT_MSG_EQ_TOL(m_state.Get(0).device.nominalComputeRate,
                                  m_gpu0->GetComputeRate(),
                                  1e-3,
                                  "Backend 0 nominal rate matches GPU compute rate");
        NS_TEST_ASSERT_MSG_EQ(m_state.Get(0).device.currentStateIdx,
                              0,
                              "Initial currentStateIdx is 0");
        NS_TEST_ASSERT_MSG_EQ(m_state.Get(1).device.numPerformanceStates,
                              3,
                              "Backend 1 should have 3 performance states");

        // CreateMetricsPacket + HandleMetrics together exercise the GpuDeviceProtocol round-trip.
        Ptr<Packet> metricsPkt = m_protocol->CreateMetricsPacket(m_gpu0);
        m_dm->HandleMetrics(metricsPkt, 0, m_state);
        NS_TEST_ASSERT_MSG_NE(m_state.Get(0).device.deviceMetrics,
                              nullptr,
                              "Metrics should be stored");
        NS_TEST_ASSERT_MSG_EQ_TOL(m_state.Get(0).device.deviceMetrics->utilization,
                                  m_gpu0->GetUtilization(),
                                  1e-9,
                                  "Stored utilization round-trips through GpuDeviceProtocol");
        NS_TEST_ASSERT_MSG_EQ_TOL(m_state.Get(0).device.deviceMetrics->currentPower,
                                  42.5,
                                  1e-9,
                                  "Stored power round-trips through GpuDeviceProtocol");

        // Scaling commands go through the network stack, which requires Simulator::Run().
        Simulator::Schedule(Seconds(0.1), &DeviceManagerTestCase::PhaseScaleUp, this);
        Simulator::Schedule(Seconds(0.2), &DeviceManagerTestCase::PhaseAtTarget, this);
        Simulator::Schedule(Seconds(0.3), &DeviceManagerTestCase::PhaseScaleDown, this);

        Simulator::Stop(Seconds(0.5));
        Simulator::Run();
        Simulator::Destroy();
    }

    void PhaseScaleUp()
    {
        m_state.NotifyTaskDispatched(0);
        m_dm->EvaluateScaling(m_state);
        NS_TEST_ASSERT_MSG_EQ(m_traceCount, 1u, "StateChanged should fire once for backend 0");
        NS_TEST_ASSERT_MSG_EQ(m_lastBackendIdx, 0u, "Trace backend index");
        NS_TEST_ASSERT_MSG_EQ(m_lastOldStateIdx, 0u, "Trace old state index");
        NS_TEST_ASSERT_MSG_EQ(m_lastNewStateIdx, 2u, "Busy -> max performance state (idx 2)");
        NS_TEST_ASSERT_MSG_EQ(m_state.Get(0).device.currentStateIdx,
                              2,
                              "ClusterState updated to new state");
        NS_TEST_ASSERT_MSG_EQ(m_state.Get(1).device.currentStateIdx,
                              0,
                              "Backend 1 unchanged (still idle)");
    }

    void PhaseAtTarget()
    {
        m_dm->EvaluateScaling(m_state);
        NS_TEST_ASSERT_MSG_EQ(m_traceCount, 1u, "No additional trace fire when at target");
    }

    void PhaseScaleDown()
    {
        m_state.NotifyTaskCompleted(0);
        m_dm->EvaluateScaling(m_state);
        NS_TEST_ASSERT_MSG_EQ(m_traceCount, 2u, "StateChanged should fire on step-down");
        NS_TEST_ASSERT_MSG_EQ(m_lastOldStateIdx, 2u, "Trace old state index for step-down");
        NS_TEST_ASSERT_MSG_EQ(m_lastNewStateIdx, 0u, "Idle -> min performance state (idx 0)");
        NS_TEST_ASSERT_MSG_EQ(m_state.Get(0).device.currentStateIdx,
                              0,
                              "ClusterState back to state 0");
    }

    void OnStateChanged(uint32_t backendIdx, uint32_t oldStateIdx, uint32_t newStateIdx)
    {
        m_traceCount++;
        m_lastBackendIdx = backendIdx;
        m_lastOldStateIdx = oldStateIdx;
        m_lastNewStateIdx = newStateIdx;
    }

    Ptr<DeviceManager> m_dm;
    Ptr<GpuAccelerator> m_gpu0;
    Ptr<GpuDeviceProtocol> m_protocol;
    ClusterState m_state;
    uint32_t m_traceCount;
    uint32_t m_lastBackendIdx;
    uint32_t m_lastOldStateIdx;
    uint32_t m_lastNewStateIdx;
};

} // namespace

TestCase*
CreateDeviceManagerTestCase()
{
    return new DeviceManagerTestCase;
}

} // namespace ns3
