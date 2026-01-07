/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/compute-task.h"
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
 * @brief Test GpuAccelerator processing model
 */
class GpuAcceleratorTestCase : public TestCase
{
  public:
    GpuAcceleratorTestCase()
        : TestCase("Test GpuAccelerator task processing"),
          m_startedCount(0),
          m_completedCount(0)
    {
    }

  private:
    void DoRun() override
    {
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));     // 1 TFLOPS
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11)); // 100 GB/s

        gpu->TraceConnectWithoutContext("TaskStarted",
                                        MakeCallback(&GpuAcceleratorTestCase::TaskStarted, this));
        gpu->TraceConnectWithoutContext("TaskCompleted",
                                        MakeCallback(&GpuAcceleratorTestCase::TaskCompleted, this));

        Ptr<ComputeTask> task = CreateObject<ComputeTask>();
        task->SetComputeDemand(1e11); // 100 GFLOP
        task->SetInputSize(1e10);     // 10 GB
        task->SetOutputSize(1e10);    // 10 GB
        task->SetArrivalTime(Seconds(0));

        gpu->SubmitTask(task);

        NS_TEST_ASSERT_MSG_EQ(gpu->GetQueueLength(), 1, "Queue should have 1 task");
        NS_TEST_ASSERT_MSG_EQ(gpu->IsBusy(), true, "GPU should be busy");

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_startedCount, 1, "One task should have started");
        NS_TEST_ASSERT_MSG_EQ(m_completedCount, 1, "One task should complete");
        NS_TEST_ASSERT_MSG_EQ_TOL(m_lastDuration.GetSeconds(),
                                  0.3,
                                  1e-9,
                                  "Task duration should be ~0.3 seconds");
    }

    void TaskStarted(Ptr<const Task>)
    {
        m_startedCount++;
    }

    void TaskCompleted(Ptr<const Task>, Time duration)
    {
        m_completedCount++;
        m_lastDuration = duration;
    }

    uint32_t m_startedCount;
    uint32_t m_completedCount;
    Time m_lastDuration;
};

} // namespace

TestCase*
CreateGpuAcceleratorTestCase()
{
    return new GpuAcceleratorTestCase;
}

} // namespace ns3
