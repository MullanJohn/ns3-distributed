/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "compute-task.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ComputeTask");
NS_OBJECT_ENSURE_REGISTERED(ComputeTask);

TypeId
ComputeTask::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ComputeTask")
                            .SetParent<Task>()
                            .SetGroupName("Distributed")
                            .AddConstructor<ComputeTask>()
                            .AddAttribute("ComputeDemand",
                                          "Compute demand in FLOPS",
                                          DoubleValue(1e9),
                                          MakeDoubleAccessor(&ComputeTask::m_computeDemand),
                                          MakeDoubleChecker<double>(0))
                            .AddAttribute("InputSize",
                                          "Input data size in bytes",
                                          UintegerValue(1024),
                                          MakeUintegerAccessor(&ComputeTask::m_inputSize),
                                          MakeUintegerChecker<uint64_t>())
                            .AddAttribute("OutputSize",
                                          "Output data size in bytes",
                                          UintegerValue(1024),
                                          MakeUintegerAccessor(&ComputeTask::m_outputSize),
                                          MakeUintegerChecker<uint64_t>());
    return tid;
}

ComputeTask::ComputeTask()
    : m_taskId(0),
      m_computeDemand(1e9),
      m_inputSize(1024),
      m_outputSize(1024)
{
    NS_LOG_FUNCTION(this);
}

ComputeTask::~ComputeTask()
{
    NS_LOG_FUNCTION(this);
}

void
ComputeTask::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_taskId = 0;
    m_computeDemand = 0;
    m_inputSize = 0;
    m_outputSize = 0;
    Task::DoDispose();
}

uint64_t
ComputeTask::GetTaskId() const
{
    return m_taskId;
}

std::string
ComputeTask::GetName() const
{
    return "ComputeTask";
}

void
ComputeTask::SetTaskId(uint64_t id)
{
    NS_LOG_FUNCTION(this << id);
    m_taskId = id;
}

void
ComputeTask::SetComputeDemand(double flops)
{
    NS_LOG_FUNCTION(this << flops);
    m_computeDemand = flops;
}

double
ComputeTask::GetComputeDemand() const
{
    return m_computeDemand;
}

void
ComputeTask::SetInputSize(uint64_t bytes)
{
    NS_LOG_FUNCTION(this << bytes);
    m_inputSize = bytes;
}

uint64_t
ComputeTask::GetInputSize() const
{
    return m_inputSize;
}

void
ComputeTask::SetOutputSize(uint64_t bytes)
{
    NS_LOG_FUNCTION(this << bytes);
    m_outputSize = bytes;
}

uint64_t
ComputeTask::GetOutputSize() const
{
    return m_outputSize;
}

} // namespace ns3
