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
#include "ns3/nstime.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test GpuAccelerator submits, runs and completes a single task.
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
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();
        energyModel->SetAttribute("StaticPower", DoubleValue(10.0));
        energyModel->SetAttribute("EffectiveCapacitance", DoubleValue(1e-9));

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e11));
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
        task->SetComputeDemand(1e11);
        task->SetInputSize(1e10);
        task->SetOutputSize(1e10);
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

    void TaskStarted(Ptr<const Task>) { m_startedCount++; }
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
 * @brief Test GpuAccelerator fails a task when no QueueScheduler is configured.
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
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e11));

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

    void TaskStarted(Ptr<const Task>) { m_startedCount++; }

    uint32_t m_failedCount;
    uint32_t m_startedCount;
    std::string m_lastFailReason;
};

/**
 * @ingroup distributed-tests
 * @brief Test GpuAccelerator SetFrequency scales the compute rate proportionally.
 */
class GpuAcceleratorFrequencyScalingTestCase : public TestCase
{
  public:
    GpuAcceleratorFrequencyScalingTestCase()
        : TestCase("Test GpuAccelerator SetFrequency scales ComputeRate proportionally")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("Frequency", DoubleValue(1.5e9));

        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetComputeRate(), 1e12, 1e-9, "Initial compute rate");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetFrequency(), 1.5e9, 1e-9, "Initial frequency");

        gpu->SetFrequency(0.75e9);
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetFrequency(), 0.75e9, 1e-9, "Frequency after scale down");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetComputeRate(),
                                  0.5e12,
                                  1e-9,
                                  "Compute rate should halve when frequency halves");

        gpu->SetFrequency(1.5e9);
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetFrequency(), 1.5e9, 1e-9, "Frequency after scale up");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetComputeRate(),
                                  1e12,
                                  1e-9,
                                  "Compute rate should restore when frequency restores");

        gpu->SetVoltage(0.8);
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetVoltage(), 0.8, 1e-9, "Voltage after SetVoltage");

        double rateBefore = gpu->GetComputeRate();
        gpu->SetFrequency(1.5e9);
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetComputeRate(),
                                  rateBefore,
                                  1e-9,
                                  "Same frequency should not change compute rate");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test GpuAccelerator rolling utilization tracks the compute phase.
 */
class GpuAcceleratorUtilizationTestCase : public TestCase
{
  public:
    GpuAcceleratorUtilizationTestCase()
        : TestCase("Test GpuAccelerator rolling utilization follows compute phase")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();
        m_gpu = CreateObject<GpuAccelerator>();
        m_gpu->SetAttribute("ComputeRate", DoubleValue(100.0));
        m_gpu->SetAttribute("MemoryBandwidth", DoubleValue(100.0));
        m_gpu->SetAttribute("TransferBandwidth", DoubleValue(100.0));
        m_gpu->SetAttribute("UtilizationWindow", TimeValue(Seconds(1.0)));
        m_gpu->SetAttribute("ProcessingModel", PointerValue(model));
        m_gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));

        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetInputSize(25);
        task->SetComputeDemand(50);
        task->SetOutputSize(25);
        m_gpu->SubmitTask(task);

        Simulator::Schedule(Seconds(0.10), &GpuAcceleratorUtilizationTestCase::CheckUtilization, this, 0.0);
        Simulator::Schedule(Seconds(0.50), &GpuAcceleratorUtilizationTestCase::CheckUtilization, this, 0.25);
        Simulator::Schedule(Seconds(0.90), &GpuAcceleratorUtilizationTestCase::CheckUtilization, this, 0.50);
        Simulator::Schedule(Seconds(1.50), &GpuAcceleratorUtilizationTestCase::CheckUtilization, this, 0.25);
        Simulator::Schedule(Seconds(1.80), &GpuAcceleratorUtilizationTestCase::CheckUtilization, this, 0.0);
        Simulator::Stop(Seconds(1.80));
        Simulator::Run();
        Simulator::Destroy();
    }

    void CheckUtilization(double expected)
    {
        NS_TEST_ASSERT_MSG_EQ_TOL(m_gpu->GetUtilization(),
                                  expected,
                                  1e-6,
                                  "Rolling utilization should match compute overlap in the window");
    }

    Ptr<GpuAccelerator> m_gpu;
};

/**
 * @ingroup distributed-tests
 * @brief Test GpuAccelerator rescales only the remaining compute phase on mid-task DVFS.
 */
class GpuAcceleratorInFlightFrequencyScalingTestCase : public TestCase
{
  public:
    GpuAcceleratorInFlightFrequencyScalingTestCase()
        : TestCase("Test GpuAccelerator rescales only remaining compute during DVFS"),
          m_completed(false)
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(100.0));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(100.0));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(100.0));
        gpu->SetAttribute("Frequency", DoubleValue(1.0e9));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));
        gpu->TraceConnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&GpuAcceleratorInFlightFrequencyScalingTestCase::TaskCompleted, this));

        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetInputSize(20);
        task->SetComputeDemand(60);
        task->SetOutputSize(20);
        gpu->SubmitTask(task);

        Simulator::Schedule(Seconds(0.10), &GpuAccelerator::SetFrequency, gpu, 2.0e9);
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_completed, true, "Task should complete");
        NS_TEST_ASSERT_MSG_EQ_TOL(m_lastDuration.GetSeconds(),
                                  0.7,
                                  1e-9,
                                  "Only the remaining compute phase should be rescaled");
    }

    void TaskCompleted(Ptr<const Task>, Time duration)
    {
        m_completed = true;
        m_lastDuration = duration;
    }

    bool m_completed;
    Time m_lastDuration;
};

/**
 * @ingroup distributed-tests
 * @brief Test GpuAccelerator ApplyState resolves an OPP-table index to frequency and voltage.
 */
class GpuAcceleratorApplyStateTestCase : public TestCase
{
  public:
    GpuAcceleratorApplyStateTestCase()
        : TestCase("Test GpuAccelerator ApplyState resolves OPP table entries")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("Frequency", DoubleValue(1.5e9));
        gpu->SetAttribute("Voltage", DoubleValue(1.0));

        gpu->AddOperatingPoint(2.0e9, 1.1);
        gpu->AddOperatingPoint(1.0e9, 0.9);
        gpu->AddOperatingPoint(1.5e9, 1.0);

        NS_TEST_ASSERT_MSG_EQ(gpu->GetNumPerformanceStates(), 3, "Three OPPs registered");

        gpu->ApplyState(0);
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetFrequency(), 1.0e9, 1e-9, "State 0 frequency");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetVoltage(), 0.9, 1e-9, "State 0 voltage");
        double rateAtLowest = gpu->GetComputeRate();

        gpu->ApplyState(2);
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetFrequency(), 2.0e9, 1e-9, "State 2 frequency");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetVoltage(), 1.1, 1e-9, "State 2 voltage");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetComputeRate(),
                                  rateAtLowest * 2.0,
                                  1e-3,
                                  "Compute rate scales with frequency between states");

        double freqBefore = gpu->GetFrequency();
        double voltBefore = gpu->GetVoltage();
        gpu->ApplyState(99);
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetFrequency(),
                                  freqBefore,
                                  1e-9,
                                  "Out-of-range ApplyState leaves frequency unchanged");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetVoltage(),
                                  voltBefore,
                                  1e-9,
                                  "Out-of-range ApplyState leaves voltage unchanged");

        Simulator::Destroy();
    }
};

} // namespace

TestCase* CreateGpuAcceleratorFrequencyScalingTestCase() { return new GpuAcceleratorFrequencyScalingTestCase; }
TestCase* CreateGpuAcceleratorTestCase() { return new GpuAcceleratorTestCase; }
TestCase* CreateGpuAcceleratorNoSchedulerTestCase() { return new GpuAcceleratorNoSchedulerTestCase; }
TestCase* CreateGpuAcceleratorUtilizationTestCase() { return new GpuAcceleratorUtilizationTestCase; }
TestCase* CreateGpuAcceleratorInFlightFrequencyScalingTestCase() { return new GpuAcceleratorInFlightFrequencyScalingTestCase; }
TestCase* CreateGpuAcceleratorApplyStateTestCase() { return new GpuAcceleratorApplyStateTestCase; }

} // namespace ns3
