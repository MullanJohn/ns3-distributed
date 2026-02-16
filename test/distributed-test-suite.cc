/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/test.h"

namespace ns3
{

// Factories implemented in the other .cc files
TestCase* CreateGpuAcceleratorTestCase();
TestCase* CreateGpuAcceleratorNoSchedulerTestCase();
TestCase* CreateGpuAcceleratorFrequencyScalingTestCase();
TestCase* CreateGpuAcceleratorQueueTestCase();
TestCase* CreateDvfsEnergyModelTestCase();
TestCase* CreateAcceleratorEnergyTrackingTestCase();
TestCase* CreateAcceleratorEnergyTracesTestCase();
TestCase* CreateEnergyModelNotConfiguredTestCase();
TestCase* CreateTaskHeaderInterfaceTestCase();
TestCase* CreateTaskHeaderPolymorphismTestCase();
TestCase* CreateTaskHeaderPayloadSizeTestCase();
TestCase* CreateSimpleTaskHeaderTestCase();
TestCase* CreateSimpleTaskHeaderResponseTestCase();
TestCase* CreateOffloadServerBasicTestCase();
TestCase* CreateOffloadServerNoAcceleratorTestCase();
TestCase* CreateOffloadClientAdmissionTestCase();
TestCase* CreateOffloadClientMultiClientTestCase();
TestCase* CreateClusterBasicTestCase();
TestCase* CreateClusterIterationTestCase();
TestCase* CreateFixedRatioProcessingModelTestCase();
TestCase* CreateFixedRatioVariedHardwareTestCase();
TestCase* CreateProcessingModelResultTestCase();
TestCase* CreateFifoQueueSchedulerEnqueueDequeueTestCase();
TestCase* CreateFifoQueueSchedulerOrderTestCase();
TestCase* CreateFifoQueueSchedulerEmptyTestCase();
TestCase* CreateFifoQueueSchedulerPeekTestCase();
TestCase* CreateBatchingQueueSchedulerSingleTestCase();
TestCase* CreateBatchingQueueSchedulerBatchTestCase();
TestCase* CreateBatchingQueueSchedulerPartialBatchTestCase();
TestCase* CreateTcpConnectionManagerBasicTestCase();
TestCase* CreateTcpConnectionManagerPoolingTestCase();
TestCase* CreateTcpConnectionManagerClosePeerTestCase();
TestCase* CreateUdpConnectionManagerBasicTestCase();
TestCase* CreateConnectionManagerPropertiesTestCase();
TestCase* CreateTcpConnectionManagerIpv6TestCase();
TestCase* CreateDagTaskDependencyTestCase();
TestCase* CreateDagTaskCycleDetectionTestCase();
TestCase* CreateDagTaskDataDependencyTestCase();
TestCase* CreateDagTaskDataAccumulationTestCase();
TestCase* CreateOrchestratorHeaderRequestTestCase();
TestCase* CreateOrchestratorHeaderResponseTestCase();
TestCase* CreateDagTaskSerializeMetadataTestCase();
TestCase* CreateDagTaskSerializeFullDataTestCase();
TestCase* CreateDagTaskDeserializeFailureTestCase();
TestCase* CreateDeviceMetricsHeaderTestCase();
TestCase* CreateScalingCommandHeaderTestCase();
TestCase* CreateLeastLoadedSchedulerTestCase();
TestCase* CreateLeastLoadedSchedulerTypeFilterTestCase();
TestCase* CreateSingleTaskEndToEndTestCase();
TestCase* CreateMultiBackendTestCase();

class DistributedTestSuite : public TestSuite
{
  public:
    DistributedTestSuite();
};

DistributedTestSuite::DistributedTestSuite()
    : TestSuite("distributed", Type::UNIT)
{
    AddTestCase(CreateGpuAcceleratorTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorNoSchedulerTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorFrequencyScalingTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorQueueTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDvfsEnergyModelTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateAcceleratorEnergyTrackingTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateAcceleratorEnergyTracesTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateEnergyModelNotConfiguredTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTaskHeaderInterfaceTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTaskHeaderPolymorphismTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTaskHeaderPayloadSizeTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateSimpleTaskHeaderTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateSimpleTaskHeaderResponseTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateOffloadServerBasicTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateOffloadServerNoAcceleratorTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateOffloadClientAdmissionTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateOffloadClientMultiClientTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateClusterBasicTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateClusterIterationTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateFixedRatioProcessingModelTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateFixedRatioVariedHardwareTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateProcessingModelResultTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateFifoQueueSchedulerEnqueueDequeueTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateFifoQueueSchedulerOrderTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateFifoQueueSchedulerEmptyTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateFifoQueueSchedulerPeekTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateBatchingQueueSchedulerSingleTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateBatchingQueueSchedulerBatchTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateBatchingQueueSchedulerPartialBatchTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTcpConnectionManagerBasicTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTcpConnectionManagerPoolingTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTcpConnectionManagerClosePeerTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateUdpConnectionManagerBasicTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateConnectionManagerPropertiesTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTcpConnectionManagerIpv6TestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDagTaskDependencyTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDagTaskCycleDetectionTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDagTaskDataDependencyTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDagTaskDataAccumulationTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateOrchestratorHeaderRequestTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateOrchestratorHeaderResponseTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDagTaskSerializeMetadataTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDagTaskSerializeFullDataTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDagTaskDeserializeFailureTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDeviceMetricsHeaderTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateScalingCommandHeaderTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateLeastLoadedSchedulerTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateLeastLoadedSchedulerTypeFilterTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateSingleTaskEndToEndTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateMultiBackendTestCase(), TestCase::Duration::QUICK);
}

static DistributedTestSuite sDistributedTestSuite;

} // namespace ns3
