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
 * @ingroup continuum-tests
 * @brief Test DvfsEnergyModel computes static and dynamic power components.
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
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("Voltage", DoubleValue(1.0));
        gpu->SetAttribute("Frequency", DoubleValue(1.0e9));

        Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
        energy->SetAttribute("EffectiveCapacitance", DoubleValue(1e-9));
        energy->SetAttribute("StaticPower", DoubleValue(10.0));

        EnergyModel::PowerState active = energy->CalculateActivePower(gpu, 1.0);
        NS_TEST_ASSERT_MSG_EQ_TOL(active.staticPower, 10.0, 1e-9, "Static power should be 10W");
        NS_TEST_ASSERT_MSG_EQ_TOL(active.dynamicPower, 1.0, 1e-9, "Dynamic power should be 1W");
        NS_TEST_ASSERT_MSG_EQ_TOL(active.GetTotalPower(), 11.0, 1e-9, "Total power should be 11W");

        EnergyModel::PowerState idle = energy->CalculateIdlePower(gpu);
        NS_TEST_ASSERT_MSG_EQ_TOL(idle.dynamicPower, 0.0, 1e-9, "Idle dynamic power should be 0W");
        NS_TEST_ASSERT_MSG_EQ_TOL(idle.GetTotalPower(), 10.0, 1e-9, "Idle total power should be 10W");

        NS_TEST_ASSERT_MSG_EQ(energy->GetName(), "DVFS", "Energy model name should be DVFS");
        Simulator::Destroy();
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test Accelerator accumulates total energy over the duration of a task.
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
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e12));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e12));
        gpu->SetAttribute("Voltage", DoubleValue(1.0));
        gpu->SetAttribute("Frequency", DoubleValue(1.0e9));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));

        Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
        energy->SetAttribute("EffectiveCapacitance", DoubleValue(1e-9));
        energy->SetAttribute("StaticPower", DoubleValue(10.0));
        gpu->SetAttribute("EnergyModel", PointerValue(energy));

        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetComputeDemand(1e12);
        task->SetInputSize(0);
        task->SetOutputSize(0);

        gpu->SubmitTask(task);

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetTotalEnergy(),
                                  11.0,
                                  1e-6,
                                  "Total energy = (10W static + 1W dynamic) * 1s = 11J");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test Accelerator accrues static energy while idle.
 */
class AcceleratorIdleStaticEnergyTestCase : public TestCase
{
  public:
    AcceleratorIdleStaticEnergyTestCase()
        : TestCase("Test Accelerator accrues idle static energy without task activity")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("Voltage", DoubleValue(1.0));
        gpu->SetAttribute("Frequency", DoubleValue(1.0e9));

        Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
        energy->SetAttribute("StaticPower", DoubleValue(10.0));
        gpu->SetAttribute("EnergyModel", PointerValue(energy));

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetCurrentPower(),
                                  10.0,
                                  1e-9,
                                  "Idle current power should match static power");
        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetTotalEnergy(),
                                  20.0,
                                  1e-9,
                                  "Idle energy should accrue from simulation start");

        Simulator::Destroy();
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test Accelerator dynamic energy is gated by task-local compute utilization.
 */
class AcceleratorTaskUtilizationEnergyTestCase : public TestCase
{
  public:
    AcceleratorTaskUtilizationEnergyTestCase()
        : TestCase("Test Accelerator energy uses task-local compute utilization during service")
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
        gpu->SetAttribute("Voltage", DoubleValue(1.0));
        gpu->SetAttribute("Frequency", DoubleValue(1.0e9));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));

        Ptr<DvfsEnergyModel> energy = CreateObject<DvfsEnergyModel>();
        energy->SetAttribute("EffectiveCapacitance", DoubleValue(1e-9));
        energy->SetAttribute("StaticPower", DoubleValue(0.0));
        gpu->SetAttribute("EnergyModel", PointerValue(energy));

        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetInputSize(50);
        task->SetComputeDemand(100);
        task->SetOutputSize(50);
        gpu->SubmitTask(task);

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ_TOL(gpu->GetTotalEnergy(),
                                  1.0,
                                  1e-6,
                                  "Dynamic energy should be proportional to compute time, not total service time");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test Accelerator fires CurrentPower, TotalEnergy and TaskEnergy traces with the expected values.
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
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e12));
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
        NS_TEST_ASSERT_MSG_EQ_TOL(m_lastTaskEnergy,
                                  11.0,
                                  1e-6,
                                  "Per-task energy = 11W * 1s = 11J (default model defaults)");
    }

    void PowerCallback(double) { m_powerTraceCount++; }
    void EnergyCallback(double) { m_energyTraceCount++; }
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
 * @ingroup continuum-tests
 * @brief Test Accelerator runs tasks and reports zero power when no EnergyModel is attached.
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

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e12));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e12));
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

    void TaskCompleted(Ptr<const Task>, Time) { m_taskCompletedCount++; }

    uint32_t m_taskCompletedCount;
};

} // namespace

TestCase* CreateDvfsEnergyModelTestCase() { return new DvfsEnergyModelTestCase; }
TestCase* CreateAcceleratorEnergyTrackingTestCase() { return new AcceleratorEnergyTrackingTestCase; }
TestCase* CreateAcceleratorIdleStaticEnergyTestCase()
{
    return new AcceleratorIdleStaticEnergyTestCase;
}
TestCase* CreateAcceleratorTaskUtilizationEnergyTestCase() { return new AcceleratorTaskUtilizationEnergyTestCase; }
TestCase* CreateAcceleratorEnergyTracesTestCase() { return new AcceleratorEnergyTracesTestCase; }
TestCase* CreateEnergyModelNotConfiguredTestCase() { return new EnergyModelNotConfiguredTestCase; }

} // namespace ns3
