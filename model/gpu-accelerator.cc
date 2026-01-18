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
                            .AddAttribute("QueueScheduler",
                                          "Queue scheduler for task management",
                                          PointerValue(),
                                          MakePointerAccessor(&GpuAccelerator::m_queueScheduler),
                                          MakePointerChecker<QueueScheduler>())
                            .AddAttribute("Frequency",
                                          "Operating frequency in Hz",
                                          DoubleValue(1.5e9),
                                          MakeDoubleAccessor(&GpuAccelerator::m_frequency),
                                          MakeDoubleChecker<double>(0.0))
                            .AddAttribute("Voltage",
                                          "Operating voltage in Volts",
                                          DoubleValue(1.0),
                                          MakeDoubleAccessor(&GpuAccelerator::m_voltage),
                                          MakeDoubleChecker<double>(0.0))
                            .AddTraceSource("QueueLength",
                                            "Current number of tasks in queue",
                                            MakeTraceSourceAccessor(&GpuAccelerator::m_queueLength),
                                            "ns3::TracedValueCallback::Uint32");
    return tid;
}

GpuAccelerator::GpuAccelerator()
    : m_computeRate(1e12),
      m_memoryBandwidth(900e9),
      m_frequency(1.5e9),
      m_voltage(1.0),
      m_processingModel(nullptr),
      m_queueScheduler(nullptr),
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
    if (m_queueScheduler)
    {
        m_queueScheduler->Clear();
        m_queueScheduler = nullptr;
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

    if (!m_queueScheduler)
    {
        NS_LOG_ERROR("GpuAccelerator requires a QueueScheduler to be set");
        m_taskFailedTrace(task, "No QueueScheduler configured");
        return;
    }

    m_queueScheduler->Enqueue(task);
    m_queueLength = m_queueScheduler->GetLength() + (m_busy ? 1 : 0);

    NS_LOG_DEBUG("Task " << task->GetTaskId()
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

    if (m_queueScheduler->IsEmpty())
    {
        m_busy = false;
        UpdateEnergyState(false, 0.0);  // Transition to idle
        return;
    }

    m_currentTask = m_queueScheduler->Dequeue();

    if (!m_processingModel)
    {
        NS_LOG_ERROR("GpuAccelerator requires a ProcessingModel to be set");
        m_taskFailedTrace(m_currentTask, "No ProcessingModel configured");
        m_currentTask = nullptr;
        m_queueLength = m_queueScheduler->GetLength();
        StartNextTask();
        return;
    }
    m_busy = true;
    m_taskStartTime = Simulator::Now();

    // Update energy state: transition to active
    UpdateEnergyState(true, 1.0);
    RecordTaskStartEnergy();

    NS_LOG_INFO("Starting task " << m_currentTask->GetTaskId() << " at " << Simulator::Now());

    // Fire task started trace
    m_taskStartedTrace(m_currentTask);

    // Update queue length after trace (includes current task being processed)
    m_queueLength = m_queueScheduler->GetLength() + 1;

    // Use ProcessingModel for timing calculation, passing this accelerator
    ProcessingModel::Result result = m_processingModel->Process(m_currentTask, this);
    if (!result.success)
    {
        NS_LOG_ERROR("ProcessingModel failed for task " << m_currentTask->GetTaskId());
        m_taskFailedTrace(m_currentTask, "ProcessingModel returned failure");
        m_currentTask = nullptr;
        m_busy = false;
        m_queueLength = m_queueScheduler->GetLength();
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

    // Update energy state: transition to idle (accumulates energy from active period)
    UpdateEnergyState(false, 0.0);

    // Fire per-task energy trace
    double taskEnergy = GetTaskEnergy();
    m_taskEnergyTrace(m_currentTask, taskEnergy);

    NS_LOG_INFO("Task " << m_currentTask->GetTaskId() << " completed in " << duration
                        << ", energy: " << taskEnergy << "J");

    // Fire completion trace
    m_taskCompletedTrace(m_currentTask, duration);

    m_tasksCompleted++;
    m_currentTask = nullptr;
    m_queueLength = m_queueScheduler->GetLength();

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

double
GpuAccelerator::GetVoltage() const
{
    return m_voltage;
}

double
GpuAccelerator::GetFrequency() const
{
    return m_frequency;
}

} // namespace ns3
