/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/double.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/pointer.h"
#include "ns3/processing-model.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
 * @brief Test FixedRatioProcessingModel computes input, compute and output phase timings.
 */
class FixedRatioProcessingModelTestCase : public TestCase
{
  public:
    FixedRatioProcessingModelTestCase()
        : TestCase("Test FixedRatioProcessingModel phase timing calculation")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();

        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("TransferBandwidth", DoubleValue(1e11));
        gpu->SetAttribute("ProcessingModel", PointerValue(model));

        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetInputSize(1e10);
        task->SetComputeDemand(1e11);
        task->SetOutputSize(1e10);

        ProcessingModel::Result result = model->Process(task, gpu);

        NS_TEST_ASSERT_MSG_EQ(result.success, true, "Result should be successful");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.inputTransferTime.GetSeconds(),
                                  0.1,
                                  1e-9,
                                  "Input transfer time should be 0.1 seconds");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.computeTime.GetSeconds(),
                                  0.1,
                                  1e-9,
                                  "Compute time should be 0.1 seconds");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.outputTransferTime.GetSeconds(),
                                  0.1,
                                  1e-9,
                                  "Output transfer time should be 0.1 seconds");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.processingTime.GetSeconds(),
                                  0.3,
                                  1e-9,
                                  "Processing time should be 0.3 seconds");
        NS_TEST_ASSERT_MSG_EQ(result.outputSize, 1e10, "Output size should match task");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateFixedRatioProcessingModelTestCase()
{
    return new FixedRatioProcessingModelTestCase;
}

} // namespace ns3
