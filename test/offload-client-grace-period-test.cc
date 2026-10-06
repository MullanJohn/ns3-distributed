/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/backend-server.h"
#include "ns3/cluster.h"
#include "ns3/dag-task.h"
#include "ns3/double.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/offload-client.h"
#include "ns3/orchestrator.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/pointer.h"
#include "ns3/round-robin-scheduler.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace
{

/**
 * @brief Shared client/orchestrator/backend topology with a deliberately
 * slow backend, so a single submitted workload is still executing well past
 * the client's stop time -- the exact condition GracePeriod is meant to
 * handle.
 */
struct GracePeriodTopology
{
    NodeContainer nodes;
    Ptr<Node> clientNode;
    Ptr<Node> orchNode;
    Ipv4InterfaceContainer ifClientOrch;
    uint16_t orchPort{8080};
};

// computeDemand / computeRate = 1e9 / 2e9 = 0.5s of pure compute time --
// far longer than the client's ~0.1-0.2s active window below, so the one
// submitted workload is guaranteed to still be in flight at client stop.
constexpr double kComputeDemand = 1e9;
constexpr double kComputeRate = 2e9;

GracePeriodTopology
BuildTopology()
{
    GracePeriodTopology topo;
    topo.nodes.Create(3);
    topo.clientNode = topo.nodes.Get(0);
    topo.orchNode = topo.nodes.Get(1);
    Ptr<Node> serverNode = topo.nodes.Get(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devClientOrch = p2p.Install(topo.clientNode, topo.orchNode);
    NetDeviceContainer devOrchServer = p2p.Install(topo.orchNode, serverNode);

    InternetStackHelper internet;
    internet.Install(topo.nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    topo.ifClientOrch = ipv4.Assign(devClientOrch);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifOrchServer = ipv4.Assign(devOrchServer);

    Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
    Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();

    Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
    gpu->SetAttribute("ComputeRate", DoubleValue(kComputeRate));
    gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
    gpu->SetAttribute("TransferBandwidth", DoubleValue(1e11));
    gpu->SetAttribute("ProcessingModel", PointerValue(model));
    gpu->SetAttribute("QueueScheduler", PointerValue(queueSched));
    serverNode->AggregateObject(gpu);

    uint16_t serverPort = 9000;
    Ptr<BackendServer> server = CreateObject<BackendServer>();
    server->SetAttribute("Port", UintegerValue(serverPort));
    serverNode->AddApplication(server);
    server->SetStartTime(Seconds(0.0));
    server->SetStopTime(Seconds(6.0));

    Cluster cluster;
    cluster.AddBackend(serverNode, InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

    uint16_t orchPort = 8080;
    Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(CreateObject<RoundRobinScheduler>()));
    orchestrator->SetCluster(cluster);
    topo.orchNode->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(6.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    topo.orchPort = orchPort;
    return topo;
}

/**
 * @brief Runs one workload against the slow backend with the given
 * GracePeriod, stopping the client well before the workload can complete,
 * and returns the number of responses the client actually received.
 */
uint64_t
RunWithGracePeriod(Time gracePeriod)
{
    GracePeriodTopology topo = BuildTopology();

    Ptr<OffloadClient> client = CreateObject<OffloadClient>();
    client->SetAttribute(
        "Remote",
        AddressValue(InetSocketAddress(topo.ifClientOrch.GetAddress(1), topo.orchPort)));
    // Fast ticks so the *first* tick fires soon after connecting; every
    // later tick is naturally dropped by the default
    // DropWhilePendingSubmissionPolicy since the one workload stays pending
    // for the client's whole active window -- exactly one workload is ever
    // sent.
    client->SetAttribute("InterArrival", StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));
    client->SetAttribute("GracePeriod", TimeValue(gracePeriod));

    Ptr<ConstantRandomVariable> inputSize = CreateObject<ConstantRandomVariable>();
    inputSize->SetAttribute("Constant", DoubleValue(100));
    Ptr<ConstantRandomVariable> compute = CreateObject<ConstantRandomVariable>();
    compute->SetAttribute("Constant", DoubleValue(kComputeDemand));
    Ptr<ConstantRandomVariable> output = CreateObject<ConstantRandomVariable>();
    output->SetAttribute("Constant", DoubleValue(100));

    Ptr<SimpleTask> templateTask = CreateObject<SimpleTask>();
    templateTask->SetAttribute("InputSizeDistribution", PointerValue(inputSize));
    templateTask->SetAttribute("ComputeDemandDistribution", PointerValue(compute));
    templateTask->SetAttribute("OutputSizeDistribution", PointerValue(output));
    Ptr<DagTask> dagTemplate = CreateObject<DagTask>();
    dagTemplate->AddTask(templateTask);
    client->SetAttribute("DagTemplate", PointerValue(dagTemplate));

    topo.clientNode->AddApplication(client);
    client->SetStartTime(Seconds(0.1));
    // Stops at 0.2s: only ~0.09s after the workload was sent, nowhere near
    // the ~0.5s compute time needed -- the workload is still in flight.
    client->SetStopTime(Seconds(0.2));

    Simulator::Stop(Seconds(6.0));
    Simulator::Run();

    uint64_t responsesReceived = client->GetResponsesReceived();

    Simulator::Destroy();
    return responsesReceived;
}

/**
 * @ingroup continuum-tests
 * @brief Default GracePeriod=0 closes immediately: an in-flight response is lost.
 */
class GracePeriodDefaultClosesImmediatelyTestCase : public TestCase
{
  public:
    GracePeriodDefaultClosesImmediatelyTestCase()
        : TestCase("Default GracePeriod=0 loses a still-in-flight response, as before")
    {
    }

  private:
    void DoRun() override
    {
        uint64_t responsesReceived = RunWithGracePeriod(Seconds(0));
        NS_TEST_ASSERT_MSG_EQ(responsesReceived,
                              0,
                              "With no grace period, the response arrives after the client "
                              "has already closed its connection and must be lost");
    }
};

/**
 * @ingroup continuum-tests
 * @brief A GracePeriod long enough to cover the round trip lets the response arrive.
 */
class GracePeriodLongEnoughReceivesResponseTestCase : public TestCase
{
  public:
    GracePeriodLongEnoughReceivesResponseTestCase()
        : TestCase("GracePeriod long enough to cover the round trip receives the response")
    {
    }

  private:
    void DoRun() override
    {
        uint64_t responsesReceived = RunWithGracePeriod(Seconds(2.0));
        NS_TEST_ASSERT_MSG_EQ(responsesReceived,
                              1,
                              "A sufficiently long grace period must let the in-flight "
                              "workload's response be received");
    }
};

/**
 * @ingroup continuum-tests
 * @brief A GracePeriod shorter than the round trip still closes on time, receiving nothing.
 */
class GracePeriodTooShortStillClosesTestCase : public TestCase
{
  public:
    GracePeriodTooShortStillClosesTestCase()
        : TestCase("GracePeriod shorter than the round trip honors its cap")
    {
    }

  private:
    void DoRun() override
    {
        uint64_t responsesReceived = RunWithGracePeriod(Seconds(0.1));
        NS_TEST_ASSERT_MSG_EQ(responsesReceived,
                              0,
                              "A grace period shorter than the remaining round trip must "
                              "still close on time rather than waiting indefinitely");
    }
};

} // namespace

TestCase*
CreateGracePeriodDefaultClosesImmediatelyTestCase()
{
    return new GracePeriodDefaultClosesImmediatelyTestCase;
}

TestCase*
CreateGracePeriodLongEnoughReceivesResponseTestCase()
{
    return new GracePeriodLongEnoughReceivesResponseTestCase;
}

TestCase*
CreateGracePeriodTooShortStillClosesTestCase()
{
    return new GracePeriodTooShortStillClosesTestCase;
}

} // namespace ns3
