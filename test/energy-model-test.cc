/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/double.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/energy-model.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/pointer.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test DvfsEnergyModel power calculation (P = C * V^2 * f)
 *
 * Validates the core DVFS formula for both active and idle states.
 */
class DvfsEnergyModelTestCase : public TestCase
{
  public:
    DvfsEnergyModelTestCase()
        : TestCase("Test DvfsEnergyModel power calculation")
    {
    }

  private:
    void DoRun() override
    {
        // Create a GPU accelerator with known voltage and frequency
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("Voltage", DoubleValue(1.0));     // 1.0V
        gpu->SetAttribute("Frequency", DoubleValue(1.0e9)); // 1 GHz

        // Create DVFS energy model
        Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
        energy->SetAttribute("EffectiveCapacitance", DoubleValue(1e-9)); // 1nF
        energy->SetAttribute("StaticPower", DoubleValue(10.0));          // 10W static

        // Calculate active power with utilization = 1.0
        // P_dynamic = C * V^2 * f * util = 1e-9 * 1.0 * 1.0e9 * 1.0 = 1.0W
        // P_total = P_static + P_dynamic = 10.0 + 1.0 = 11.0W
        EnergyModel::PowerState active = energy->CalculateActivePower(gpu, 1.0);

        NS_TEST_ASSERT_MSG_EQ(active.valid, true, "PowerState should be valid");
        NS_TEST_ASSERT_MSG_EQ_TOL(active.staticPower, 10.0, 1e-9, "Static power should be 10W");
        NS_TEST_ASSERT_MSG_EQ_TOL(active.dynamicPower, 1.0, 1e-9, "Dynamic power should be 1W");
        NS_TEST_ASSERT_MSG_EQ_TOL(active.GetTotalPower(), 11.0, 1e-9, "Total power should be 11W");

        // Calculate idle power (P_static only)
        EnergyModel::PowerState idle = energy->CalculateIdlePower(gpu);

        NS_TEST_ASSERT_MSG_EQ(idle.valid, true, "Idle PowerState should be valid");
        NS_TEST_ASSERT_MSG_EQ_TOL(idle.dynamicPower, 0.0, 1e-9, "Idle dynamic power should be 0W");
        NS_TEST_ASSERT_MSG_EQ_TOL(idle.GetTotalPower(),
                                  10.0,
                                  1e-9,
                                  "Idle total power should be 10W");

        NS_TEST_ASSERT_MSG_EQ(energy->GetName(), "DVFS", "Energy model name should be DVFS");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test Accelerator energy accumulation during task processing
 *
 * Validates that energy is correctly accumulated over time when processing tasks.
 */
class AcceleratorEnergyTrackingTestCase : public TestCase
{
  public:
    AcceleratorEnergyTrackingTestCase()
        : TestCase("Test Accelerator energy tracking during task processing")
    {
    }

  private:
    void DoRun() override
    {
        // Create processing model and queue scheduler
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        // Create GPU accelerator with energy model
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));     // 1 TFLOPS
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e12)); // 1 TB/s
        gpu->SetAttribute("Voltage", DoubleValue(1.0));          // 1.0V
        gpu->SetAttribute("Frequency", DoubleValue(1.0e9));      // 1 GHz
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));

        // Create energy model: P_active = 10 + 1*1*1e9*1e-9 = 11W
        Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
        energy->SetAttribute("EffectiveCapacitance", DoubleValue(1e-9));
        energy->SetAttribute("StaticPower", DoubleValue(10.0));
        gpu->SetAttribute("EnergyModel", PointerValue(energy));

        // Create a task that takes 1 second to process (1e12 FLOP / 1e12 FLOPS)
        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetComputeDemand(1e12);
        task->SetInputSize(0);
        task->SetOutputSize(0);

        gpu->SubmitTask(task);

        Simulator::Run();
        Simulator::Destroy();

        // After 1 second at 11W, energy should be ~11J
        double totalEnergy = gpu->GetTotalEnergy();
        NS_TEST_ASSERT_MSG_GT(totalEnergy, 10.0, "Energy should be > 10J");
        NS_TEST_ASSERT_MSG_LT(totalEnergy, 12.0, "Energy should be < 12J");
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test that energy traces fire correctly
 *
 * Validates that CurrentPower, TotalEnergy, and TaskEnergy traces fire.
 */
class AcceleratorEnergyTracesTestCase : public TestCase
{
  public:
    AcceleratorEnergyTracesTestCase()
        : TestCase("Test Accelerator energy traces fire correctly"),
          m_powerTraceCount(0),
          m_energyTraceCount(0),
          m_taskEnergyTraceCount(0),
          m_lastTaskEnergy(0.0)
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e12));
        gpu->SetAttribute("Voltage", DoubleValue(1.0));
        gpu->SetAttribute("Frequency", DoubleValue(1.0e9));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));

        Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
        gpu->SetAttribute("EnergyModel", PointerValue(energy));

        gpu->TraceConnectWithoutContext(
            "CurrentPower",
            MakeCallback(&AcceleratorEnergyTracesTestCase::PowerCallback, this));
        gpu->TraceConnectWithoutContext(
            "TotalEnergy",
            MakeCallback(&AcceleratorEnergyTracesTestCase::EnergyCallback, this));
        gpu->TraceConnectWithoutContext(
            "TaskEnergy",
            MakeCallback(&AcceleratorEnergyTracesTestCase::TaskEnergyCallback, this));

        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetComputeDemand(1e12);
        task->SetInputSize(0);
        task->SetOutputSize(0);

        gpu->SubmitTask(task);

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_GT(m_powerTraceCount, 0u, "CurrentPower trace should fire");
        NS_TEST_ASSERT_MSG_GT(m_energyTraceCount, 0u, "TotalEnergy trace should fire");
        NS_TEST_ASSERT_MSG_EQ(m_taskEnergyTraceCount, 1u, "TaskEnergy trace should fire once");
        NS_TEST_ASSERT_MSG_GT(m_lastTaskEnergy, 0.0, "Task energy should be > 0");
    }

    void PowerCallback(double)
    {
        m_powerTraceCount++;
    }

    void EnergyCallback(double)
    {
        m_energyTraceCount++;
    }

    void TaskEnergyCallback(Ptr<const Task>, double energy)
    {
        m_taskEnergyTraceCount++;
        m_lastTaskEnergy = energy;
    }

    uint32_t m_powerTraceCount;
    uint32_t m_energyTraceCount;
    uint32_t m_taskEnergyTraceCount;
    double m_lastTaskEnergy;
};

/**
 * @ingroup distributed-tests
 * @brief Test graceful behavior without EnergyModel configured
 *
 * Validates that tasks complete normally when no energy model is set.
 */
class EnergyModelNotConfiguredTestCase : public TestCase
{
  public:
    EnergyModelNotConfiguredTestCase()
        : TestCase("Test graceful behavior without EnergyModel"),
          m_taskCompletedCount(0)
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        // Create GPU accelerator WITHOUT energy model
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e12));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));

        gpu->TraceConnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&EnergyModelNotConfiguredTestCase::TaskCompleted, this));

        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetComputeDemand(1e9);
        task->SetInputSize(0);
        task->SetOutputSize(0);

        gpu->SubmitTask(task);

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_taskCompletedCount, 1, "Task should complete without energy model");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetTotalEnergy(), 0.0, 1e-9, "Energy should be 0");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetCurrentPower(), 0.0, 1e-9, "Power should be 0");
    }

    void TaskCompleted(Ptr<const Task>, Time)
    {
        m_taskCompletedCount++;
    }

    uint32_t m_taskCompletedCount;
};

} // namespace

// Factory functions for test registration
TestCase*
CreateDvfsEnergyModelTestCase()
{
    return new DvfsEnergyModelTestCase;
}

TestCase*
CreateAcceleratorEnergyTrackingTestCase()
{
    return new AcceleratorEnergyTrackingTestCase;
}

TestCase*
CreateAcceleratorEnergyTracesTestCase()
{
    return new AcceleratorEnergyTracesTestCase;
}

TestCase*
CreateEnergyModelNotConfiguredTestCase()
{
    return new EnergyModelNotConfiguredTestCase;
}

} // namespace ns3
