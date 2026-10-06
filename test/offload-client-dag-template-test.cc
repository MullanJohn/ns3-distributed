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
#include "ns3/offload-client-helper.h"
#include "ns3/offload-client.h"
#include "ns3/orchestrator.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/round-robin-scheduler.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
 * @brief End-to-end test that OffloadClient completes a 2-task chain DAG
 * built via SetLinearChainDagTemplate, and that the downstream task's final
 * input size reflects only the upstream task's output -- not an inflated
 * value from an independently-sampled base input (the bug this template
 * mechanism replaces the old DagTopology + flat client RVs to fix).
 */
class OffloadClientLinearChainEndToEndTestCase : public TestCase
{
  public:
    OffloadClientLinearChainEndToEndTestCase()
        : TestCase("OffloadClient completes a 2-task chain DAG built via SetLinearChainDagTemplate"),
          m_workloadsAdmitted(0),
          m_admittedTaskCount(0),
          m_workloadsProcessed(0),
          m_processedTaskCount(0),
          m_task1InputSize(0)
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(3);
        Ptr<Node> clientNode = nodes.Get(0);
        Ptr<Node> orchNode = nodes.Get(1);
        Ptr<Node> serverNode = nodes.Get(2);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devClientOrch = p2p.Install(clientNode, orchNode);
        NetDeviceContainer devOrchServer = p2p.Install(orchNode, serverNode);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ifClientOrch = ipv4.Assign(devClientOrch);

        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer ifOrchServer = ipv4.Assign(devOrchServer);

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
        server->SetStopTime(Seconds(10.0));

        Cluster cluster;
        cluster.AddBackend(serverNode, InetSocketAddress(ifOrchServer.GetAddress(1), serverPort));

        uint16_t orchPort = 8080;
        Ptr<Orchestrator> orchestrator = CreateObject<Orchestrator>();
        orchestrator->SetAttribute("Port", UintegerValue(orchPort));
        orchestrator->SetAttribute("Scheduler", PointerValue(CreateObject<RoundRobinScheduler>()));
        orchestrator->SetCluster(cluster);
        orchNode->AddApplication(orchestrator);
        orchestrator->SetStartTime(Seconds(0.0));
        orchestrator->SetStopTime(Seconds(10.0));

        orchestrator->TraceConnectWithoutContext(
            "WorkloadAdmitted",
            MakeCallback(&OffloadClientLinearChainEndToEndTestCase::OnWorkloadAdmitted, this));

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        OffloadClientHelper clientHelper(
            InetSocketAddress(ifClientOrch.GetAddress(1), orchPort));
        Ptr<ConstantRandomVariable> interArrival = CreateObject<ConstantRandomVariable>();
        interArrival->SetAttribute("Constant", DoubleValue(1.0));
        clientHelper.SetInterArrival(interArrival);

        Ptr<ConstantRandomVariable> inputSize = CreateObject<ConstantRandomVariable>();
        inputSize->SetAttribute("Constant", DoubleValue(1000));
        Ptr<ConstantRandomVariable> compute = CreateObject<ConstantRandomVariable>();
        compute->SetAttribute("Constant", DoubleValue(1e9));
        Ptr<ConstantRandomVariable> output = CreateObject<ConstantRandomVariable>();
        output->SetAttribute("Constant", DoubleValue(100));
        clientHelper.SetLinearChainDagTemplate(2, inputSize, compute, output);

        Ptr<OffloadClient> client = DynamicCast<OffloadClient>(
            clientHelper.Install(clientNode).Get(0));

        client->TraceConnectWithoutContext(
            "WorkloadProcessed",
            MakeCallback(&OffloadClientLinearChainEndToEndTestCase::OnWorkloadProcessed, this));

        client->SetStartTime(Seconds(0.1));
        client->SetStopTime(Seconds(1.5));

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        uint64_t responsesReceived = client->GetResponsesReceived();

        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_workloadsAdmitted, 1, "WorkloadAdmitted should fire once");
        NS_TEST_ASSERT_MSG_EQ(m_admittedTaskCount,
                              2,
                              "Admitted workload should carry both chain tasks");
        NS_TEST_ASSERT_MSG_EQ(responsesReceived, 1, "Client should receive one response");
        NS_TEST_ASSERT_MSG_EQ(m_workloadsProcessed, 1, "WorkloadProcessed should fire once");
        NS_TEST_ASSERT_MSG_EQ(m_processedTaskCount,
                              2,
                              "Completed DAG returned to client should carry both tasks");
        NS_TEST_ASSERT_MSG_EQ(m_task1InputSize,
                              100,
                              "Task 1's input should equal exactly task 0's output (100), "
                              "not task 0's output plus an independently-sampled base "
                              "input -- the double-count this template mechanism fixes");
    }

    void OnWorkloadAdmitted(uint64_t workloadId, uint32_t taskCount)
    {
        m_workloadsAdmitted++;
        m_admittedTaskCount = taskCount;
    }

    void OnWorkloadProcessed(Ptr<const DagTask> dag, Time latency)
    {
        m_workloadsProcessed++;
        m_processedTaskCount = dag->GetTaskCount();

        for (uint32_t i = 0; i < dag->GetTaskCount(); i++)
        {
            Ptr<Task> task = dag->GetTask(i);
            NS_TEST_ASSERT_MSG_NE(task, nullptr, "Every chain task should survive the round trip");
            NS_TEST_ASSERT_MSG_GT(task->GetComputeTime().GetNanoSeconds(),
                                  0,
                                  "Every chain task should have actually executed");
        }

        if (dag->GetTaskCount() > 1)
        {
            m_task1InputSize = dag->GetTask(1)->GetInputSize();
        }
    }

    uint32_t m_workloadsAdmitted;
    uint32_t m_admittedTaskCount;
    uint32_t m_workloadsProcessed;
    uint32_t m_processedTaskCount;
    uint64_t m_task1InputSize;
};

} // namespace

TestCase*
CreateOffloadClientLinearChainEndToEndTestCase()
{
    return new OffloadClientLinearChainEndToEndTestCase;
}

} // namespace ns3
