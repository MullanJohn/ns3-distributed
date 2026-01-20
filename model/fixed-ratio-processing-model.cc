/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "fixed-ratio-processing-model.h"

#include "gpu-accelerator.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FixedRatioProcessingModel");

NS_OBJECT_ENSURE_REGISTERED(FixedRatioProcessingModel);

TypeId
FixedRatioProcessingModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::FixedRatioProcessingModel")
            .SetParent<ProcessingModel>()
            .SetGroupName("Distributed")
            .AddConstructor<FixedRatioProcessingModel>();
    return tid;
}

FixedRatioProcessingModel::FixedRatioProcessingModel()
{
    NS_LOG_FUNCTION(this);
}

FixedRatioProcessingModel::~FixedRatioProcessingModel()
{
    NS_LOG_FUNCTION(this);
}

void
FixedRatioProcessingModel::DoDispose()
{
    NS_LOG_FUNCTION(this);
    ProcessingModel::DoDispose();
}

std::string
FixedRatioProcessingModel::GetName() const
{
    return "FixedRatio";
}

ProcessingModel::Result
FixedRatioProcessingModel::Process(Ptr<const Task> task, Ptr<const Accelerator> accelerator) const
{
    NS_LOG_FUNCTION(this << task << accelerator);

    // Cast accelerator to GpuAccelerator to get hardware properties
    Ptr<const GpuAccelerator> gpu = DynamicCast<const GpuAccelerator>(accelerator);
    if (!gpu)
    {
        NS_LOG_WARN("FixedRatioProcessingModel requires GpuAccelerator, received: "
                    << accelerator->GetName());
        return Result();  // Returns success=false
    }

    // Get hardware properties from accelerator
    double computeRate = gpu->GetComputeRate();
    double memoryBandwidth = gpu->GetMemoryBandwidth();

    // Get task parameters from base Task class
    uint64_t inputSize = task->GetInputSize();
    double computeDemand = task->GetComputeDemand();
    uint64_t outputSize = task->GetOutputSize();

    // Calculate three-phase timing
    double inputTransferSeconds = static_cast<double>(inputSize) / memoryBandwidth;
    double computeSeconds = computeDemand / computeRate;
    double outputTransferSeconds = static_cast<double>(outputSize) / memoryBandwidth;
    double totalSeconds = inputTransferSeconds + computeSeconds + outputTransferSeconds;

    // Utilization = fraction of time spent computing (vs memory transfer)
    // Higher ratio means more compute-bound (higher GPU core utilization)
    double utilization = (totalSeconds > 0) ? (computeSeconds / totalSeconds) : 1.0;

    NS_LOG_DEBUG("Task " << task->GetTaskId() << " processing:"
                         << " input=" << Seconds(inputTransferSeconds)
                         << " compute=" << Seconds(computeSeconds)
                         << " output=" << Seconds(outputTransferSeconds)
                         << " total=" << Seconds(totalSeconds)
                         << " utilization=" << utilization);

    return Result(Seconds(totalSeconds), outputSize, utilization);
}

} // namespace ns3
