/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "gpu-accelerator.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("GpuAccelerator");
NS_OBJECT_ENSURE_REGISTERED(GpuAccelerator);

TypeId
GpuAccelerator::GetTypeId()
{
    static TypeId tid = TypeId("ns3::GpuAccelerator")
                            .SetParent<Accelerator>()
                            .SetGroupName("Distributed")
                            .AddConstructor<GpuAccelerator>()
                            .AddAttribute("ComputeRate",
                                          "Compute rate in FLOPS (must be > 0)",
                                          DoubleValue(1e12),
                                          MakeDoubleAccessor(&GpuAccelerator::m_computeRate),
                                          MakeDoubleChecker<double>(1.0))
                            .AddAttribute("MemoryBandwidth",
                                          "Memory bandwidth in bytes/sec (must be > 0)",
                                          DoubleValue(900e9),
                                          MakeDoubleAccessor(&GpuAccelerator::m_memoryBandwidth),
                                          MakeDoubleChecker<double>(1.0))
                            .AddAttribute("ProcessingModel",
                                          "Processing model for timing calculation",
                                          PointerValue(),
                                          MakePointerAccessor(&GpuAccelerator::m_processingModel),
                                          MakePointerChecker<ProcessingModel>())
                            .AddTraceSource("QueueLength",
                                            "Current number of tasks in queue",
                                            MakeTraceSourceAccessor(&GpuAccelerator::m_queueLength),
                                            "ns3::TracedValueCallback::Uint32");
    return tid;
}

GpuAccelerator::GpuAccelerator()
    : m_computeRate(1e12),
      m_memoryBandwidth(900e9),
      m_processingModel(nullptr),
      m_currentTask(nullptr),
      m_busy(false),
      m_tasksCompleted(0),
      m_queueLength(0)
{
    NS_LOG_FUNCTION(this);
}

GpuAccelerator::~GpuAccelerator()
{
    NS_LOG_FUNCTION(this);
}

void
GpuAccelerator::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(m_currentEvent);
    m_currentTask = nullptr;
    m_processingModel = nullptr;
    while (!m_taskQueue.empty())
    {
        m_taskQueue.pop();
    }
    Accelerator::DoDispose();
}

std::string
GpuAccelerator::GetName() const
{
    return "GPU";
}

void
GpuAccelerator::SubmitTask(Ptr<Task> task)
{
    NS_LOG_FUNCTION(this << task);

    // GpuAccelerator requires ComputeTask objects
    Ptr<ComputeTask> computeTask = DynamicCast<ComputeTask>(task);
    if (!computeTask)
    {
        NS_LOG_ERROR("GpuAccelerator requires ComputeTask objects, received: " << task->GetName());
        return;
    }

    m_taskQueue.push(computeTask);
    m_queueLength = m_taskQueue.size() + (m_busy ? 1 : 0);

    NS_LOG_DEBUG("Task " << computeTask->GetTaskId()
                         << " submitted, queue length: " << m_queueLength);

    if (!m_busy)
    {
        StartNextTask();
    }
}

void
GpuAccelerator::StartNextTask()
{
    NS_LOG_FUNCTION(this);

    if (m_taskQueue.empty())
    {
        m_busy = false;
        return;
    }

    if (!m_processingModel)
    {
        NS_LOG_ERROR("GpuAccelerator requires a ProcessingModel to be set");
        return;
    }

    m_currentTask = m_taskQueue.front();
    m_taskQueue.pop();
    m_busy = true;
    m_taskStartTime = Simulator::Now();

    NS_LOG_INFO("Starting task " << m_currentTask->GetTaskId() << " at " << Simulator::Now());

    // Fire task started trace
    m_taskStartedTrace(m_currentTask);

    // Update queue length after trace (includes current task being processed)
    m_queueLength = m_taskQueue.size() + 1;

    // Use ProcessingModel for timing calculation, passing this accelerator
    ProcessingModel::Result result = m_processingModel->Process(m_currentTask, this);
    if (!result.success)
    {
        NS_LOG_ERROR("ProcessingModel failed for task " << m_currentTask->GetTaskId());
        m_currentTask = nullptr;
        m_queueLength = m_taskQueue.size();
        StartNextTask();
        return;
    }

    NS_LOG_DEBUG("Processing time: " << result.processingTime);
    m_currentEvent = Simulator::Schedule(result.processingTime,
                                          &GpuAccelerator::ProcessingComplete,
                                          this);
}

void
GpuAccelerator::ProcessingComplete()
{
    NS_LOG_FUNCTION(this);

    // Calculate total task duration
    Time duration = Simulator::Now() - m_taskStartTime;

    NS_LOG_INFO("Task " << m_currentTask->GetTaskId() << " completed in " << duration);

    // Fire completion trace
    m_taskCompletedTrace(m_currentTask, duration);

    m_tasksCompleted++;
    m_currentTask = nullptr;
    m_queueLength = m_taskQueue.size();

    // Start next task if available
    StartNextTask();
}

uint32_t
GpuAccelerator::GetQueueLength() const
{
    return m_queueLength.Get();
}

bool
GpuAccelerator::IsBusy() const
{
    return m_busy;
}

double
GpuAccelerator::GetComputeRate() const
{
    return m_computeRate;
}

double
GpuAccelerator::GetMemoryBandwidth() const
{
    return m_memoryBandwidth;
}

} // namespace ns3
