/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/gpu-accelerator.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/task-generator.h"
#include "ns3/task.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test TaskGenerator application with fixed arrivals
 */
class TaskGeneratorTestCase : public TestCase
{
  public:
    TaskGeneratorTestCase()
        : TestCase("Test TaskGenerator with fixed arrivals"),
          m_generatedCount(0)
    {
    }

  private:
    void DoRun() override
    {
        Ptr<Node> node = CreateObject<Node>();
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();

        Ptr<TaskGenerator> generator = CreateObject<TaskGenerator>();
        generator->SetAttribute("MaxTasks", UintegerValue(10));
        generator->SetAttribute("InterArrivalTime",
                                StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));
        generator->SetAttribute("ComputeDemand",
                                StringValue("ns3::ConstantRandomVariable[Constant=1e9]"));
        generator->SetAttribute("InputSize",
                                StringValue("ns3::ConstantRandomVariable[Constant=1024]"));
        generator->SetAttribute("OutputSize",
                                StringValue("ns3::ConstantRandomVariable[Constant=1024]"));
        generator->SetAccelerator(gpu);

        generator->TraceConnectWithoutContext(
            "TaskGenerated",
            MakeCallback(&TaskGeneratorTestCase::TaskGenerated, this));

        node->AddApplication(generator);
        generator->SetStartTime(Seconds(0));
        generator->SetStopTime(Seconds(1));

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_generatedCount, 10, "Should generate exactly 10 tasks");
        NS_TEST_ASSERT_MSG_EQ(generator->GetTaskCount(), 10, "Task count should be 10");
    }

    void TaskGenerated(Ptr<const Task>)
    {
        m_generatedCount++;
    }

    uint32_t m_generatedCount;
};

} // namespace

TestCase*
CreateTaskGeneratorTestCase()
{
    return new TaskGeneratorTestCase;
}

} // namespace ns3
