/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/double.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/simulator.h"
#include "ns3/task.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test GpuAccelerator with multiple tasks in queue
 */
class GpuAcceleratorQueueTestCase : public TestCase
{
  public:
    GpuAcceleratorQueueTestCase()
        : TestCase("Test GpuAccelerator queue processing"),
          m_completedCount(0)
    {
    }

  private:
    void DoRun() override
    {
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e12));

        gpu->TraceConnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&GpuAcceleratorQueueTestCase::TaskCompleted, this));

        for (uint32_t i = 0; i < 5; i++)
        {
            Ptr<Task> task = CreateObject<Task>();
            task->SetTaskId(i);
            task->SetComputeDemand(1e9);
            task->SetInputSize(1e6);
            task->SetOutputSize(1e6);
            task->SetArrivalTime(Simulator::Now());
            gpu->SubmitTask(task);
        }

        NS_TEST_ASSERT_MSG_EQ(gpu->GetQueueLength(), 5, "Queue should have 5 tasks");

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_completedCount, 5, "All 5 tasks should complete");
    }

    void TaskCompleted(Ptr<const Task>, Time)
    {
        m_completedCount++;
    }

    uint32_t m_completedCount;
};

} // namespace

TestCase*
CreateGpuAcceleratorQueueTestCase()
{
    return new GpuAcceleratorQueueTestCase;
}

} // namespace ns3
