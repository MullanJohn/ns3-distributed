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
 * @ingroup continuum-tests
 * @brief Test GpuAccelerator processes multiple tasks queued back-to-back.
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
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
        Ptr<FifoQueueScheduler> scheduler = CreateObject<FifoQueueScheduler>();
        Ptr<DvfsEnergyModel> energyModel = CreateObject<DvfsEnergyModel>();

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e12));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e12));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));
        gpu->SetAttribute("QueueScheduler", PointerValue(scheduler));
        gpu->SetAttribute("EnergyModel", PointerValue(energyModel));

        gpu->TraceConnectWithoutContext(
            "TaskCompleted",
            MakeCallback(&GpuAcceleratorQueueTestCase::TaskCompleted, this));

        for (uint32_t i = 0; i < 5; i++)
        {
            Ptr<Task> task = CreateObject<SimpleTask>();
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
