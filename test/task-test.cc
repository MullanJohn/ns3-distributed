/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/double.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simple-task.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
 * @brief Test Task::Sample() falls back to the plain point-value attributes
 * when no distribution attribute is set.
 */
class TaskSampleFallbackTestCase : public TestCase
{
  public:
    TaskSampleFallbackTestCase()
        : TestCase("Test Task::Sample() falls back to plain attributes when unset")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        task->SetInputSize(123);
        task->SetOutputSize(456);
        task->SetComputeDemand(7.5e9);
        task->SetPriority(3);
        task->SetRequiredAcceleratorType("GPU");

        Ptr<Task> sampled = task->Sample();

        NS_TEST_ASSERT_MSG_NE(sampled, task, "Sample() should return a fresh instance");
        NS_TEST_ASSERT_MSG_EQ(sampled->GetInputSize(), 123, "InputSize should fall back unchanged");
        NS_TEST_ASSERT_MSG_EQ(sampled->GetOutputSize(),
                              456,
                              "OutputSize should fall back unchanged");
        NS_TEST_ASSERT_MSG_EQ(sampled->GetComputeDemand(),
                              7.5e9,
                              "ComputeDemand should fall back unchanged");
        NS_TEST_ASSERT_MSG_EQ(sampled->GetPriority(), 3, "Priority should be copied");
        NS_TEST_ASSERT_MSG_EQ(sampled->GetRequiredAcceleratorType(),
                              "GPU",
                              "RequiredAcceleratorType should be copied");

        Simulator::Destroy();
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test Task::Sample() draws from a set distribution attribute instead
 * of the plain point value.
 */
class TaskSampleDistributionTestCase : public TestCase
{
  public:
    TaskSampleDistributionTestCase()
        : TestCase("Test Task::Sample() draws from a set distribution attribute")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        // Plain value left at the default 0 -- Sample() should ignore it and
        // draw from the distribution instead.
        Ptr<ConstantRandomVariable> inputRv = CreateObject<ConstantRandomVariable>();
        inputRv->SetAttribute("Constant", DoubleValue(1000));
        task->SetAttribute("InputSizeDistribution", PointerValue(inputRv));

        Ptr<ConstantRandomVariable> computeRv = CreateObject<ConstantRandomVariable>();
        computeRv->SetAttribute("Constant", DoubleValue(2e9));
        task->SetAttribute("ComputeDemandDistribution", PointerValue(computeRv));

        Ptr<ConstantRandomVariable> outputRv = CreateObject<ConstantRandomVariable>();
        outputRv->SetAttribute("Constant", DoubleValue(500));
        task->SetAttribute("OutputSizeDistribution", PointerValue(outputRv));

        Ptr<Task> sampled = task->Sample();

        NS_TEST_ASSERT_MSG_EQ(sampled->GetInputSize(), 1000, "InputSize should be sampled");
        NS_TEST_ASSERT_MSG_EQ(sampled->GetComputeDemand(), 2e9, "ComputeDemand should be sampled");
        NS_TEST_ASSERT_MSG_EQ(sampled->GetOutputSize(), 500, "OutputSize should be sampled");

        Simulator::Destroy();
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test Task::AssignStreams() assigns distinct streams only to
 * whichever distributions are actually set.
 */
class TaskAssignStreamsTestCase : public TestCase
{
  public:
    TaskAssignStreamsTestCase()
        : TestCase("Test Task::AssignStreams() only counts set distributions")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        NS_TEST_ASSERT_MSG_EQ(task->AssignStreams(0),
                              0,
                              "No distributions set: no streams assigned");

        Ptr<ConstantRandomVariable> inputRv = CreateObject<ConstantRandomVariable>();
        task->SetAttribute("InputSizeDistribution", PointerValue(inputRv));
        Ptr<ConstantRandomVariable> outputRv = CreateObject<ConstantRandomVariable>();
        task->SetAttribute("OutputSizeDistribution", PointerValue(outputRv));

        NS_TEST_ASSERT_MSG_EQ(task->AssignStreams(10),
                              2,
                              "Two distributions set: two streams assigned");

        Simulator::Destroy();
    }
};

} // namespace

TestCase*
CreateTaskSampleFallbackTestCase()
{
    return new TaskSampleFallbackTestCase;
}

TestCase*
CreateTaskSampleDistributionTestCase()
{
    return new TaskSampleDistributionTestCase;
}

TestCase*
CreateTaskAssignStreamsTestCase()
{
    return new TaskAssignStreamsTestCase;
}

} // namespace ns3
