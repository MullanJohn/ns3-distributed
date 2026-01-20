/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/simple-task.h"
#include "ns3/double.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/pointer.h"
#include "ns3/processing-model.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup distributed-tests
 * @brief Test FixedRatioProcessingModel time calculation with known values
 */
class FixedRatioProcessingModelTestCase : public TestCase
{
  public:
    FixedRatioProcessingModelTestCase()
        : TestCase("Test FixedRatioProcessingModel time calculation")
    {
    }

  private:
    void DoRun() override
    {
        // Create processing model
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();

        // Create GPU accelerator with known values
        Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
        gpu->SetAttribute("ComputeRate", DoubleValue(1e12));     // 1 TFLOPS
        gpu->SetAttribute("MemoryBandwidth", DoubleValue(1e11)); // 100 GB/s
        gpu->SetAttribute("ProcessingModel", PointerValue(model));

        // Create task with known values
        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetInputSize(1e10);     // 10 GB
        task->SetComputeDemand(1e11); // 100 GFLOP
        task->SetOutputSize(1e10);    // 10 GB

        ProcessingModel::Result result = model->Process(task, gpu);

        NS_TEST_ASSERT_MSG_EQ(result.success, true, "Result should be successful");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.processingTime.GetSeconds(),
                                  0.3,
                                  1e-9,
                                  "Processing time should be 0.3 seconds");
        NS_TEST_ASSERT_MSG_EQ(result.outputSize, 1e10, "Output size should match task");

        // Utilization = computeTime / totalTime = 0.1 / 0.3 = 0.333...
        // This task has balanced compute and memory transfer
        NS_TEST_ASSERT_MSG_EQ_TOL(result.utilization,
                                  1.0 / 3.0,
                                  1e-9,
                                  "Utilization should be ~0.33 for balanced task");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test FixedRatioProcessingModel with different hardware configurations
 */
class FixedRatioVariedHardwareTestCase : public TestCase
{
  public:
    FixedRatioVariedHardwareTestCase()
        : TestCase("Test FixedRatioProcessingModel with varied hardware")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();

        // Create GPU with faster hardware
        Ptr<GpuAccelerator> fastGpu = CreateObject<GpuAccelerator>();
        fastGpu->SetAttribute("ComputeRate", DoubleValue(2e12));     // 2 TFLOPS
        fastGpu->SetAttribute("MemoryBandwidth", DoubleValue(2e11)); // 200 GB/s
        fastGpu->SetAttribute("ProcessingModel", PointerValue(model));

        Ptr<Task> task = CreateObject<SimpleTask>();
        task->SetTaskId(1);
        task->SetInputSize(1e10);     // 10 GB
        task->SetComputeDemand(1e11); // 100 GFLOP
        task->SetOutputSize(1e10);    // 10 GB

        ProcessingModel::Result result = model->Process(task, fastGpu);

        NS_TEST_ASSERT_MSG_EQ(result.success, true, "Result should be successful");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.processingTime.GetSeconds(),
                                  0.15,
                                  1e-9,
                                  "Processing time should be 0.15 seconds");

        Simulator::Destroy();
    }
};

/**
 * @ingroup distributed-tests
 * @brief Test ProcessingModel::Result default constructor creates failed result
 */
class ProcessingModelResultTestCase : public TestCase
{
  public:
    ProcessingModelResultTestCase()
        : TestCase("Test ProcessingModel::Result default behavior")
    {
    }

  private:
    void DoRun() override
    {
        // Test default constructor (failed result)
        ProcessingModel::Result failedResult;
        NS_TEST_ASSERT_MSG_EQ(failedResult.success, false, "Default result should be failure");
        NS_TEST_ASSERT_MSG_EQ(failedResult.processingTime.IsZero(),
                              true,
                              "Default time should be zero");
        NS_TEST_ASSERT_MSG_EQ(failedResult.outputSize, 0, "Default output size should be zero");
        NS_TEST_ASSERT_MSG_EQ_TOL(failedResult.utilization,
                                  0.0,
                                  1e-9,
                                  "Default utilization should be zero");

        // Test success constructor with default utilization (1.0)
        ProcessingModel::Result successResult(Seconds(1.5), 1000);
        NS_TEST_ASSERT_MSG_EQ(successResult.success, true, "Success result should be successful");
        NS_TEST_ASSERT_MSG_EQ(successResult.processingTime.GetSeconds(),
                              1.5,
                              "Time should be 1.5s");
        NS_TEST_ASSERT_MSG_EQ(successResult.outputSize, 1000, "Output size should be 1000");
        NS_TEST_ASSERT_MSG_EQ_TOL(successResult.utilization,
                                  1.0,
                                  1e-9,
                                  "Default utilization should be 1.0");

        // Test success constructor with custom utilization
        ProcessingModel::Result customUtilResult(Seconds(2.0), 500, 0.75);
        NS_TEST_ASSERT_MSG_EQ(customUtilResult.success, true, "Custom result should be successful");
        NS_TEST_ASSERT_MSG_EQ_TOL(customUtilResult.utilization,
                                  0.75,
                                  1e-9,
                                  "Custom utilization should be 0.75");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateFixedRatioProcessingModelTestCase()
{
    return new FixedRatioProcessingModelTestCase;
}

TestCase*
CreateFixedRatioVariedHardwareTestCase()
{
    return new FixedRatioVariedHardwareTestCase;
}

TestCase*
CreateProcessingModelResultTestCase()
{
    return new ProcessingModelResultTestCase;
}

} // namespace ns3
