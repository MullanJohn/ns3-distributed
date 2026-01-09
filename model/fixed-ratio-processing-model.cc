/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "fixed-ratio-processing-model.h"

#include "compute-task.h"
#include "gpu-accelerator.h"

#include "ns3/double.h"
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
            .AddConstructor<FixedRatioProcessingModel>()
            .AddAttribute("FlopsPerByte",
                          "Compute intensity: FLOPS required per byte of input (>= 0)",
                          DoubleValue(100.0),
                          MakeDoubleAccessor(&FixedRatioProcessingModel::m_flopsPerByte),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("OutputRatio",
                          "Ratio of output size to input size (>= 0)",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&FixedRatioProcessingModel::m_outputRatio),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

FixedRatioProcessingModel::FixedRatioProcessingModel()
    : m_flopsPerByte(100.0),
      m_outputRatio(1.0)
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

    // Try to cast to ComputeTask for explicit parameters
    Ptr<const ComputeTask> computeTask = DynamicCast<const ComputeTask>(task);
    if (!computeTask)
    {
        NS_LOG_WARN("FixedRatioProcessingModel requires ComputeTask, received: "
                    << task->GetName());
        return Result();  // Returns success=false
    }

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

    // Get task parameters
    uint64_t inputSize = computeTask->GetInputSize();
    double computeDemand = computeTask->GetComputeDemand();
    uint64_t outputSize = computeTask->GetOutputSize();

    // Calculate three-phase timing
    Time inputTransferTime = Seconds(static_cast<double>(inputSize) / memoryBandwidth);
    Time computeTime = Seconds(computeDemand / computeRate);
    Time outputTransferTime = Seconds(static_cast<double>(outputSize) / memoryBandwidth);

    Time totalTime = inputTransferTime + computeTime + outputTransferTime;

    NS_LOG_DEBUG("Task " << task->GetTaskId() << " processing:"
                         << " input=" << inputTransferTime
                         << " compute=" << computeTime
                         << " output=" << outputTransferTime
                         << " total=" << totalTime);

    return Result(totalTime, outputSize);
}

double
FixedRatioProcessingModel::GetFlopsPerByte() const
{
    return m_flopsPerByte;
}

double
FixedRatioProcessingModel::GetOutputRatio() const
{
    return m_outputRatio;
}

} // namespace ns3
