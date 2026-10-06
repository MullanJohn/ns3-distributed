/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/admission-policy.h"
#include "ns3/always-retry-admission-policy.h"
#include "ns3/backend-server.h"
#include "ns3/cluster.h"
#include "ns3/dag-task.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/never-retry-admission-policy.h"
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
 * @brief Test-only AdmissionPolicy that counts invocations and rejects the
 * first `rejectFirstN` calls before admitting every call after that.
 *
 * Used to observe exactly how many times the orchestrator actually consults
 * the admission policy under SessionMode vs. WorkloadMode.
 */
class CountingAdmissionPolicy : public AdmissionPolicy
{
  public:
    explicit CountingAdmissionPolicy(uint32_t rejectFirstN = 0)
        : m_rejectFirstN(rejectFirstN),
          m_callCount(0)
    {
    }

    bool ShouldAdmit(Ptr<DagTask> dag, const Cluster& cluster, const ClusterState& state) override
    {
        m_callCount++;
        return m_callCount > m_rejectFirstN;
    }

    std::string GetName() const override
    {
        return "CountingAdmissionPolicy";
    }

    uint32_t GetCallCount() const
    {
        return m_callCount;
    }

  private:
    uint32_t m_rejectFirstN;
    uint32_t m_callCount;
};

/**
 * @brief Shared client/orchestrator/backend topology used by every admission-mode test.
 */
struct AdmissionModeTopology
{
    NodeContainer nodes;
    Ptr<Node> clientNode;
    Ptr<Node> orchNode;
    Ipv4InterfaceContainer ifClientOrch;
    Ipv4InterfaceContainer ifOrchServer;
    Cluster cluster;
    uint16_t orchPort{8080};
};

AdmissionModeTopology
BuildTopology()
{
    AdmissionModeTopology topo;
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
    topo.ifOrchServer = ipv4.Assign(devOrchServer);

    Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
    Ptr<FifoQueueScheduler> queueSched = CreateObject<FifoQueueScheduler>();

    Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
    gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
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
    server->SetStopTime(Seconds(20.0));

    topo.cluster.AddBackend(serverNode,
                            InetSocketAddress(topo.ifOrchServer.GetAddress(1), serverPort));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    return topo;
}

Ptr<Orchestrator>
MakeOrchestrator(AdmissionModeTopology& topo, Ptr<AdmissionPolicy> policy)
{
    Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
    orchestrator->SetAttribute("Port", UintegerValue(topo.orchPort));
    orchestrator->SetAttribute("Scheduler", PointerValue(CreateObject<RoundRobinScheduler>()));
    if (policy)
    {
        orchestrator->SetAttribute("AdmissionPolicy", PointerValue(policy));
    }
    orchestrator->SetCluster(topo.cluster);
    topo.orchNode->AddApplication(orchestrator);
    orchestrator->SetStartTime(Seconds(0.0));
    orchestrator->SetStopTime(Seconds(20.0));
    return orchestrator;
}

Ptr<OffloadClient>
MakeClient(AdmissionModeTopology& topo,
          OffloadClient::AdmissionMode mode,
          Ptr<AdmissionRetryPolicy> retryPolicy,
          Time stopTime)
{
    Ptr<OffloadClient> client = CreateObject<OffloadClient>();
    client->SetAttribute(
        "Remote",
        AddressValue(InetSocketAddress(topo.ifClientOrch.GetAddress(1), topo.orchPort)));
    client->SetAttribute("InterArrival", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    client->SetAttribute("AdmissionMode", EnumValue(mode));
    if (retryPolicy)
    {
        client->SetAttribute("AdmissionRetryPolicy", PointerValue(retryPolicy));
    }

    Ptr<ConstantRandomVariable> inputSize = CreateObject<ConstantRandomVariable>();
    inputSize->SetAttribute("Constant", DoubleValue(1000));
    Ptr<ConstantRandomVariable> compute = CreateObject<ConstantRandomVariable>();
    compute->SetAttribute("Constant", DoubleValue(1e9));
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
    client->SetStopTime(stopTime);
    return client;
}

/**
 * @ingroup continuum-tests
 * @brief WorkloadMode: every workload is individually checked and admitted.
 */
class WorkloadModeEndToEndTestCase : public TestCase
{
  public:
    WorkloadModeEndToEndTestCase()
        : TestCase("WorkloadMode admits and completes every workload independently")
    {
    }

  private:
    void DoRun() override
    {
        AdmissionModeTopology topo = BuildTopology();

        Ptr<CountingAdmissionPolicy> policy = CreateObject<CountingAdmissionPolicy>(0);
        Ptr<Orchestrator> orchestrator = MakeOrchestrator(topo, policy);
        Ptr<OffloadClient> client =
            MakeClient(topo, OffloadClient::WORKLOAD_MODE, nullptr, Seconds(4.5));

        Simulator::Stop(Seconds(20.0));
        Simulator::Run();

        uint64_t tasksSent = client->GetTasksSent();
        uint64_t responsesReceived = client->GetResponsesReceived();
        uint64_t workloadsAdmitted = orchestrator->GetWorkloadsAdmitted();
        uint32_t callCount = policy->GetCallCount();

        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_GT(tasksSent, 1, "Client should have submitted multiple workloads");
        NS_TEST_ASSERT_MSG_EQ(callCount,
                              tasksSent,
                              "WorkloadMode must check admission for every workload");
        NS_TEST_ASSERT_MSG_EQ(workloadsAdmitted,
                              tasksSent,
                              "Every workload should be independently admitted");
        NS_TEST_ASSERT_MSG_EQ(responsesReceived,
                              tasksSent,
                              "Every admitted workload should complete and return a response");
    }
};

/**
 * @ingroup continuum-tests
 * @brief SessionMode: only the first workload triggers an admission check;
 * every subsequent workload is implicitly admitted and still completes.
 */
class SessionModeEndToEndTestCase : public TestCase
{
  public:
    SessionModeEndToEndTestCase()
        : TestCase("SessionMode checks admission once and implicitly admits the rest")
    {
    }

  private:
    void DoRun() override
    {
        AdmissionModeTopology topo = BuildTopology();

        Ptr<CountingAdmissionPolicy> policy = CreateObject<CountingAdmissionPolicy>(0);
        Ptr<Orchestrator> orchestrator = MakeOrchestrator(topo, policy);
        Ptr<OffloadClient> client =
            MakeClient(topo, OffloadClient::SESSION_MODE, nullptr, Seconds(4.5));

        Simulator::Stop(Seconds(20.0));
        Simulator::Run();

        uint64_t tasksSent = client->GetTasksSent();
        uint64_t responsesReceived = client->GetResponsesReceived();
        uint64_t workloadsAdmitted = orchestrator->GetWorkloadsAdmitted();
        uint32_t callCount = policy->GetCallCount();

        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_GT(tasksSent, 1, "Client should have submitted multiple workloads");
        NS_TEST_ASSERT_MSG_EQ(callCount,
                              1,
                              "SessionMode must only check admission for the first workload");
        NS_TEST_ASSERT_MSG_EQ(workloadsAdmitted,
                              tasksSent,
                              "Every workload should still be dispatched and completed");
        NS_TEST_ASSERT_MSG_EQ(responsesReceived,
                              tasksSent,
                              "Every submitted workload should complete and return a response");
    }
};

/**
 * @ingroup continuum-tests
 * @brief SessionMode rejection with the default NeverRetryAdmissionPolicy:
 * the client abandons session establishment permanently after one rejection.
 */
class SessionModeGiveUpTestCase : public TestCase
{
  public:
    SessionModeGiveUpTestCase()
        : TestCase("SessionMode gives up permanently on rejection with NeverRetryAdmissionPolicy")
    {
    }

  private:
    void DoRun() override
    {
        AdmissionModeTopology topo = BuildTopology();

        // Reject every call.
        Ptr<CountingAdmissionPolicy> policy = CreateObject<CountingAdmissionPolicy>(UINT32_MAX);
        Ptr<Orchestrator> orchestrator = MakeOrchestrator(topo, policy);
        Ptr<OffloadClient> client = MakeClient(topo,
                                               OffloadClient::SESSION_MODE,
                                               CreateObject<NeverRetryAdmissionPolicy>(),
                                               Seconds(9.5));

        Simulator::Stop(Seconds(20.0));
        Simulator::Run();

        uint64_t tasksSent = client->GetTasksSent();
        uint64_t workloadsRejected = orchestrator->GetWorkloadsRejected();
        uint32_t callCount = policy->GetCallCount();

        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(tasksSent,
                              1,
                              "Client should stay permanently quiet after the first rejection");
        NS_TEST_ASSERT_MSG_EQ(callCount, 1, "Only the first attempt should reach admission");
        NS_TEST_ASSERT_MSG_EQ(workloadsRejected, 1, "Exactly one workload should be rejected");
    }
};

/**
 * @ingroup continuum-tests
 * @brief SessionMode rejection with AlwaysRetryAdmissionPolicy: the client
 * keeps re-attempting session establishment until the policy admits it.
 */
class SessionModeRetryUntilAdmittedTestCase : public TestCase
{
  public:
    SessionModeRetryUntilAdmittedTestCase()
        : TestCase("SessionMode retries after rejection with AlwaysRetryAdmissionPolicy "
                   "until admitted")
    {
    }

  private:
    void DoRun() override
    {
        AdmissionModeTopology topo = BuildTopology();

        // Reject the first two attempts, then admit every one after.
        Ptr<CountingAdmissionPolicy> policy = CreateObject<CountingAdmissionPolicy>(2);
        Ptr<Orchestrator> orchestrator = MakeOrchestrator(topo, policy);
        Ptr<OffloadClient> client = MakeClient(topo,
                                               OffloadClient::SESSION_MODE,
                                               CreateObject<AlwaysRetryAdmissionPolicy>(),
                                               Seconds(4.5));

        Simulator::Stop(Seconds(20.0));
        Simulator::Run();

        uint64_t tasksSent = client->GetTasksSent();
        uint64_t responsesReceived = client->GetResponsesReceived();
        uint64_t workloadsAdmitted = orchestrator->GetWorkloadsAdmitted();
        uint64_t workloadsRejected = orchestrator->GetWorkloadsRejected();
        uint32_t callCount = policy->GetCallCount();

        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_GT(tasksSent,
                              2,
                              "Client should keep retrying past the two rejections");
        NS_TEST_ASSERT_MSG_EQ(callCount,
                              3,
                              "Admission should be checked on each retry until it succeeds, "
                              "then never again");
        NS_TEST_ASSERT_MSG_EQ(workloadsRejected, 2, "The first two attempts should be rejected");
        NS_TEST_ASSERT_MSG_EQ(workloadsAdmitted,
                              tasksSent - 2,
                              "Every workload from the third attempt onward should be admitted");
        NS_TEST_ASSERT_MSG_EQ(responsesReceived,
                              workloadsAdmitted,
                              "Every admitted workload should complete and return a response");
    }
};

} // namespace

TestCase*
CreateWorkloadModeEndToEndTestCase()
{
    return new WorkloadModeEndToEndTestCase;
}

TestCase*
CreateSessionModeEndToEndTestCase()
{
    return new SessionModeEndToEndTestCase;
}

TestCase*
CreateSessionModeGiveUpTestCase()
{
    return new SessionModeGiveUpTestCase;
}

TestCase*
CreateSessionModeRetryUntilAdmittedTestCase()
{
    return new SessionModeRetryUntilAdmittedTestCase;
}

} // namespace ns3
