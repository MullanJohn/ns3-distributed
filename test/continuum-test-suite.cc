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
TestCase* CreateClusterCommPropertiesTestCase();
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
TestCase* CreateDagTaskInstantiateTestCase();
TestCase* CreateTaskSampleFallbackTestCase();
TestCase* CreateTaskSampleDistributionTestCase();
TestCase* CreateTaskAssignStreamsTestCase();
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
TestCase* CreateNetworkAwareSchedulerDefaultTestCase();
TestCase* CreateNetworkAwareSchedulerDelayTestCase();
TestCase* CreateNetworkAwareSchedulerCapacityTestCase();
TestCase* CreateNetworkAwareSchedulerWeightTestCase();
TestCase* CreateNetworkAwareSchedulerLiveOverrideTestCase();
TestCase* CreateNetworkAwareSchedulerNameTestCase();
TestCase* CreateTieredDelaySchedulerStaysUnderThresholdTestCase();
TestCase* CreateTieredDelaySchedulerOverflowsAtThresholdTestCase();
TestCase* CreateTieredDelaySchedulerIntraTierLowestUtilTestCase();
TestCase* CreateTieredDelaySchedulerRandomTieBreakTestCase();
TestCase* CreateTieredDelaySchedulerLastTierCatchAllTestCase();
TestCase* CreateTieredDelaySchedulerTypeFilterTestCase();
TestCase* CreateTieredDelaySchedulerHysteresisTestCase();
TestCase* CreateTieredDelaySchedulerNameTestCase();
TestCase* CreateClusterStateTestCase();
TestCase* CreateClusterStateNetworkTestCase();
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
TestCase* CreateOrchestratorNetworkDelayTrackingTestCase();
TestCase* CreateWorkloadResponseEndToEndTestCase();
TestCase* CreateMultiSinkDagSerializationTestCase();
TestCase* CreateWorkloadModeEndToEndTestCase();
TestCase* CreateSessionModeEndToEndTestCase();
TestCase* CreateSessionModeGiveUpTestCase();
TestCase* CreateSessionModeRetryUntilAdmittedTestCase();
TestCase* CreateOffloadClientLinearChainEndToEndTestCase();
TestCase* CreateGracePeriodDefaultClosesImmediatelyTestCase();
TestCase* CreateGracePeriodLongEnoughReceivesResponseTestCase();
TestCase* CreateGracePeriodTooShortStillClosesTestCase();

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
    AddTestCase(CreateClusterCommPropertiesTestCase(), TestCase::Duration::QUICK);
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
    AddTestCase(CreateDagTaskInstantiateTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTaskSampleFallbackTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTaskSampleDistributionTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTaskAssignStreamsTestCase(), TestCase::Duration::QUICK);
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
    AddTestCase(CreateNetworkAwareSchedulerDefaultTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateNetworkAwareSchedulerDelayTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateNetworkAwareSchedulerCapacityTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateNetworkAwareSchedulerWeightTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateNetworkAwareSchedulerLiveOverrideTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateNetworkAwareSchedulerNameTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTieredDelaySchedulerStaysUnderThresholdTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTieredDelaySchedulerOverflowsAtThresholdTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTieredDelaySchedulerIntraTierLowestUtilTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTieredDelaySchedulerRandomTieBreakTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTieredDelaySchedulerLastTierCatchAllTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTieredDelaySchedulerTypeFilterTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTieredDelaySchedulerHysteresisTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTieredDelaySchedulerNameTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateClusterStateTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateClusterStateNetworkTestCase(), TestCase::Duration::QUICK);
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
    AddTestCase(CreateOrchestratorNetworkDelayTrackingTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateWorkloadResponseEndToEndTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateMultiSinkDagSerializationTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateWorkloadModeEndToEndTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateSessionModeEndToEndTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateSessionModeGiveUpTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateSessionModeRetryUntilAdmittedTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateOffloadClientLinearChainEndToEndTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGracePeriodDefaultClosesImmediatelyTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGracePeriodLongEnoughReceivesResponseTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGracePeriodTooShortStillClosesTestCase(), TestCase::Duration::QUICK);
}

static ContinuumTestSuite sContinuumTestSuite;

} // namespace ns3
