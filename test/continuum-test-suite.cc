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
TestCase* CreateGpuAcceleratorUtilizationTestCase();
TestCase* CreateGpuAcceleratorInFlightFrequencyScalingTestCase();
TestCase* CreateGpuAcceleratorApplyStateTestCase();
TestCase* CreateGpuAcceleratorQueueTestCase();
TestCase* CreateDvfsEnergyModelTestCase();
TestCase* CreateAcceleratorEnergyTrackingTestCase();
TestCase* CreateAcceleratorIdleStaticEnergyTestCase();
TestCase* CreateAcceleratorTaskUtilizationEnergyTestCase();
TestCase* CreateAcceleratorEnergyTracesTestCase();
TestCase* CreateEnergyModelNotConfiguredTestCase();
TestCase* CreateTaskHeaderSerializationTestCase();
TestCase* CreateTaskHeaderPayloadSizeTestCase();
TestCase* CreateClusterBasicTestCase();
TestCase* CreateClusterIterationTestCase();
TestCase* CreateFixedRatioProcessingModelTestCase();
TestCase* CreateFifoQueueSchedulerTestCase();
TestCase* CreateTcpConnectionManagerBasicTestCase();
TestCase* CreateTcpConnectionManagerPoolingTestCase();
TestCase* CreateTcpConnectionManagerClosePeerTestCase();
TestCase* CreateUdpConnectionManagerBasicTestCase();
TestCase* CreateTcpConnectionManagerIpv6TestCase();
TestCase* CreateDagTaskDependencyTestCase();
TestCase* CreateDagTaskCycleDetectionTestCase();
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
TestCase* CreateRoundRobinSchedulerTestCase();
TestCase* CreateClusterStateTestCase();
TestCase* CreateDeviceManagerTestCase();
TestCase* CreateSingleTaskEndToEndTestCase();
TestCase* CreateMultiBackendTestCase();
TestCase* CreateConservativeStepUpTestCase();
TestCase* CreateConservativeStepDownTestCase();
TestCase* CreateConservativeBoundaryStateTestCase();
TestCase* CreateUtilizationScalingOppTestCase();
TestCase* CreateMaxActiveTasksAdmitCapacityTestCase();
TestCase* CreateMaxActiveTasksRejectFullTestCase();
TestCase* CreateMaxActiveTasksPendingReservationTestCase();
TestCase* CreateMaxActiveTasksIncomingWorkloadSizeTestCase();
TestCase* CreateMaxActiveTasksTypeScopedReservationTestCase();
TestCase* CreateDropWhilePendingSubmissionPolicyTestCase();

class ContinuumTestSuite : public TestSuite
{
  public:
    ContinuumTestSuite();
};

ContinuumTestSuite::ContinuumTestSuite()
    : TestSuite("continuum", Type::UNIT)
{
    AddTestCase(CreateGpuAcceleratorTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorNoSchedulerTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorFrequencyScalingTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorUtilizationTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorInFlightFrequencyScalingTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorApplyStateTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorQueueTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDvfsEnergyModelTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateAcceleratorEnergyTrackingTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateAcceleratorIdleStaticEnergyTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateAcceleratorTaskUtilizationEnergyTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateAcceleratorEnergyTracesTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateEnergyModelNotConfiguredTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTaskHeaderSerializationTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTaskHeaderPayloadSizeTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateClusterBasicTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateClusterIterationTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateFixedRatioProcessingModelTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateFifoQueueSchedulerTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTcpConnectionManagerBasicTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTcpConnectionManagerPoolingTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTcpConnectionManagerClosePeerTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateUdpConnectionManagerBasicTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTcpConnectionManagerIpv6TestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDagTaskDependencyTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDagTaskCycleDetectionTestCase(), TestCase::Duration::QUICK);
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
    AddTestCase(CreateRoundRobinSchedulerTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateClusterStateTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDeviceManagerTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateSingleTaskEndToEndTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateMultiBackendTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateConservativeStepUpTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateConservativeStepDownTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateConservativeBoundaryStateTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateUtilizationScalingOppTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateMaxActiveTasksAdmitCapacityTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateMaxActiveTasksRejectFullTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateMaxActiveTasksPendingReservationTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateMaxActiveTasksIncomingWorkloadSizeTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateMaxActiveTasksTypeScopedReservationTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateDropWhilePendingSubmissionPolicyTestCase(), TestCase::Duration::QUICK);
}

static ContinuumTestSuite sContinuumTestSuite;

} // namespace ns3
