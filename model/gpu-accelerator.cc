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
                                          MakeDoubleChecker<double>(1.0))
                            .AddAttribute("Voltage",
                                          "Operating voltage in Volts",
                                          DoubleValue(1.0),
                                          MakeDoubleAccessor(&GpuAccelerator::m_voltage),
                                          MakeDoubleChecker<double>(0.01))
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
      m_currentUtilization(0.0),
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
        task->SetState(TASK_FAILED);
        m_taskFailedTrace(task, "No QueueScheduler configured");
        return;
    }

    m_queueScheduler->Enqueue(task);
    m_queueLength = m_queueScheduler->GetLength() + (m_currentTask ? 1 : 0);

    NS_LOG_DEBUG("Task " << task->GetTaskId() << " submitted, queue length: " << m_queueLength);

    if (!m_currentTask)
    {
        StartNextTask();
    }
}

void
GpuAccelerator::StartNextTask()
{
    NS_LOG_FUNCTION(this);

    while (!m_queueScheduler->IsEmpty())
    {
        m_currentTask = m_queueScheduler->Dequeue();

        if (!m_processingModel)
        {
            NS_LOG_ERROR("GpuAccelerator requires a ProcessingModel to be set");
            m_currentTask->SetState(TASK_FAILED);
            m_taskFailedTrace(m_currentTask, "No ProcessingModel configured");
            m_currentTask = nullptr;
            m_queueLength = m_queueScheduler->GetLength();
            continue;
        }

        ProcessingModel::Result result = m_processingModel->Process(m_currentTask, this);
        if (!result.success)
        {
            NS_LOG_ERROR("ProcessingModel failed for task " << m_currentTask->GetTaskId());
            m_currentTask->SetState(TASK_FAILED);
            m_taskFailedTrace(m_currentTask, "ProcessingModel returned failure");
            m_currentTask = nullptr;
            m_queueLength = m_queueScheduler->GetLength();
            continue;
        }

        m_taskStartTime = Simulator::Now();
        m_currentUtilization = result.utilization;

        NS_LOG_INFO("Starting task " << m_currentTask->GetTaskId() << " at " << Simulator::Now());

        UpdateEnergyState(true, m_currentUtilization);
        RecordTaskStartEnergy();

        m_currentTask->SetState(TASK_RUNNING);
        m_taskStartedTrace(m_currentTask);

        m_queueLength = m_queueScheduler->GetLength() + 1;

        NS_LOG_DEBUG("Processing time: " << result.processingTime);
        m_currentEvent =
            Simulator::Schedule(result.processingTime, &GpuAccelerator::ProcessingComplete, this);
        return;
    }

    m_currentTask = nullptr;
    UpdateEnergyState(false, 0.0);
}

void
GpuAccelerator::ProcessingComplete()
{
    NS_LOG_FUNCTION(this);

    Time duration = Simulator::Now() - m_taskStartTime;

    UpdateEnergyState(false, 0.0);

    double taskEnergy = GetTaskEnergy();
    m_taskEnergyTrace(m_currentTask, taskEnergy);

    NS_LOG_INFO("Task " << m_currentTask->GetTaskId() << " completed in " << duration
                        << ", energy: " << taskEnergy << "J");

    m_currentTask->SetComputeTime(duration);

    m_currentTask->SetState(TASK_COMPLETED);
    m_taskCompletedTrace(m_currentTask, duration);

    m_currentTask = nullptr;
    m_queueLength = m_queueScheduler->GetLength();

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
    return m_currentTask != nullptr;
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

void
GpuAccelerator::SetFrequency(double frequency)
{
    NS_LOG_FUNCTION(this << frequency);
    if (frequency == m_frequency)
    {
        return;
    }

    if (m_currentTask)
    {
        UpdateEnergyState(true, m_currentUtilization);
    }

    double ratio = frequency / m_frequency;
    m_computeRate *= ratio;
    m_frequency = frequency;

    if (m_currentTask)
    {
        UpdateEnergyState(true, m_currentUtilization);

        if (m_currentEvent.IsPending())
        {
            Time delayLeft = Simulator::GetDelayLeft(m_currentEvent);
            Simulator::Cancel(m_currentEvent);

            if (delayLeft.IsZero())
            {
                m_currentEvent =
                    Simulator::ScheduleNow(&GpuAccelerator::ProcessingComplete, this);
            }
            else
            {
                Time remaining = Seconds(delayLeft.GetSeconds() / ratio);
                m_currentEvent = Simulator::Schedule(
                    remaining,
                    &GpuAccelerator::ProcessingComplete,
                    this);
            }
        }
    }
}

void
GpuAccelerator::SetVoltage(double voltage)
{
    NS_LOG_FUNCTION(this << voltage);
    m_voltage = voltage;
}

} // namespace ns3
