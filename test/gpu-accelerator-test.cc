/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/double.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/pointer.h"
#include "ns3/simple-task.h"
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
        // Create processing model and queue scheduler
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        // Create energy model
        Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
        energyModel->SetAttribute("StaticPower", DoubleValue(10.0));
        energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(1e-9));

        // Create GPU accelerator with processing model and queue scheduler
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));     // 1 TFLOPS
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11)); // 100 GB/s
        gpu->SetAttribute("Voltage", DoubleValue(1.0));
        gpu->SetAttribute("Frequency", DoubleValue(1.0e9));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));
        gpu->SetAttribute("EnergyModel", PointerValue(energyModel));

        gpu->TraceConnectWithoutContext("TaskStarted",
                                        MakeCallback(&GpuAcceleratorTestCase::TaskStarted, this));
        gpu->TraceConnectWithoutContext("TaskCompleted",
                                        MakeCallback(&GpuAcceleratorTestCase::TaskCompleted, this));

        Ptr<Task> task = CreateObject<SimpleTask>();
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

/**
 * @ingroup distributed-tests
 * @brief Test GpuAccelerator fails gracefully without QueueScheduler
 */
class GpuAcceleratorNoSchedulerTestCase : public TestCase
{
  public:
    GpuAcceleratorNoSchedulerTestCase()
        : TestCase("Test GpuAccelerator fails without QueueScheduler"),
          m_failedCount(0),
          m_startedCount(0)
    {
    }

  private:
    void DoRun() override
    {
        // Create GPU accelerator WITHOUT QueueScheduler
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));

        gpu->TraceConnectWithoutContext(
            "TaskFailed",
            MakeCallback(&GpuAcceleratorNoSchedulerTestCase::TaskFailed, this));
        gpu->TraceConnectWithoutContext(
            "TaskStarted",
            MakeCallback(&GpuAcceleratorNoSchedulerTestCase::TaskStarted, this));

        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetComputeDemand(1e9);
        task->SetInputSize(1e6);
        task->SetOutputSize(1e6);

        gpu->SubmitTask(task);

        // Task should fail immediately, not be queued
        NS_TEST_ASSERT_MSG_EQ(gpu->GetQueueLength(), 0, "Queue should be empty (no scheduler)");
        NS_TEST_ASSERT_MSG_EQ(gpu->IsBusy(), false, "GPU should not be busy");

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_failedCount, 1, "Task should have failed");
        NS_TEST_ASSERT_MSG_EQ(m_startedCount, 0, "No task should have started");
        NS_TEST_ASSERT_MSG_NE(m_lastFailReason.find("QueueScheduler"),
                              std::string::npos,
                              "Failure reason should mention QueueScheduler");
    }

    void TaskFailed(Ptr<const Task>, std::string reason)
    {
        m_failedCount++;
        m_lastFailReason = reason;
    }

    void TaskStarted(Ptr<const Task>)
    {
        m_startedCount++;
    }

    uint32_t m_failedCount;
    uint32_t m_startedCount;
    std::string m_lastFailReason;
};

} // namespace

TestCase*
CreateGpuAcceleratorTestCase()
{
    return new GpuAcceleratorTestCase;
}

TestCase*
CreateGpuAcceleratorNoSchedulerTestCase()
{
    return new GpuAcceleratorNoSchedulerTestCase;
}

} // namespace ns3
