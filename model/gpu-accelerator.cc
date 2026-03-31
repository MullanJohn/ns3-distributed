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
#include "ns3/nstime.h"
#include "ns3/trace-source-accessor.h"

#include <algorithm>

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
                            .AddAttribute("ComputeEfficiency",
                                          "Fraction of theoretical compute rate achieved "
                                          "by the workload.",
                                          DoubleValue(1.0),
                                          MakeDoubleAccessor(&GpuAccelerator::m_computeEfficiency),
                                          MakeDoubleChecker<double>(0.0, 1.0))
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
                            .AddAttribute("UtilizationWindow",
                                          "Observation window used for rolling compute utilization.",
                                          TimeValue(MilliSeconds(100)),
                                          MakeTimeAccessor(&GpuAccelerator::m_utilizationWindow),
                                          MakeTimeChecker())
                            .AddTraceSource("QueueLength",
                                            "Current number of tasks in queue",
                                            MakeTraceSourceAccessor(&GpuAccelerator::m_queueLength),
                                            "ns3::TracedValueCallback::Uint32");
    return tid;
}

GpuAccelerator::GpuAccelerator()
    : m_computeRate(1e12),
      m_computeEfficiency(1.0),
      m_memoryBandwidth(900e9),
      m_frequency(1.5e9),
      m_voltage(1.0),
      m_utilizationWindow(MilliSeconds(100)),
      m_processingModel(nullptr),
      m_queueScheduler(nullptr),
      m_currentTask(nullptr),
      m_taskStartTime(Seconds(0)),
      m_currentSegmentStartTime(Seconds(0)),
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
    ClearCurrentTask();
    m_computeIntervals.clear();
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

        Time now = Simulator::Now();
        m_taskStartTime = now;
        m_currentSegmentStartTime = now;
        m_currentTaskProfile = {result.inputTransferTime,
                                result.computeTime,
                                result.outputTransferTime,
                                result.processingTime};

        UpdateEnergyState(true, CalculateTaskUtilization(m_currentTaskProfile));
        RecordTaskStartEnergy();

        m_currentTask->SetState(TASK_RUNNING);
        m_taskStartedTrace(m_currentTask);

        m_queueLength = m_queueScheduler->GetLength() + 1;
        m_currentEvent = Simulator::Schedule(m_currentTaskProfile.processingTime,
                                             &GpuAccelerator::ProcessingComplete,
                                             this);
        return;
    }

    ClearCurrentTask();
    UpdateEnergyState(false, 0.0);
}

void
GpuAccelerator::ProcessingComplete()
{
    Time now = Simulator::Now();
    AccumulateCurrentComputeUntil(now);
    Time duration = now - m_taskStartTime;

    UpdateEnergyState(false, 0.0);

    double taskEnergy = GetTaskEnergy();
    m_taskEnergyTrace(m_currentTask, taskEnergy);

    m_currentTask->SetComputeTime(duration);
    m_currentTask->SetState(TASK_COMPLETED);
    m_taskCompletedTrace(m_currentTask, duration);

    ClearCurrentTask();
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
GpuAccelerator::GetUtilization() const
{
    return ComputeUtilizationAt(Simulator::Now());
}

double
GpuAccelerator::GetComputeRate() const
{
    return m_computeRate * m_computeEfficiency;
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

    double ratio = frequency / m_frequency;
    if (!m_currentTask)
    {
        m_computeRate *= ratio;
        m_frequency = frequency;
        return;
    }

    Time now = Simulator::Now();
    UpdateEnergyState(true, CalculateTaskUtilization(m_currentTaskProfile));
    AccumulateCurrentComputeUntil(now);

    Time elapsed = now - m_currentSegmentStartTime;
    TaskProfile remaining = GetRemainingTaskProfile(elapsed, ratio);

    if (m_currentEvent.IsPending())
    {
        Simulator::Cancel(m_currentEvent);
    }

    m_computeRate *= ratio;
    m_frequency = frequency;

    m_currentSegmentStartTime = now;
    m_currentTaskProfile = remaining;
    UpdateEnergyState(true, CalculateTaskUtilization(m_currentTaskProfile));

    if (m_currentTaskProfile.processingTime.IsZero())
    {
        m_currentEvent = Simulator::ScheduleNow(&GpuAccelerator::ProcessingComplete, this);
    }
    else
    {
        m_currentEvent = Simulator::Schedule(m_currentTaskProfile.processingTime,
                                             &GpuAccelerator::ProcessingComplete,
                                             this);
    }
}

void
GpuAccelerator::SetVoltage(double voltage)
{
    NS_LOG_FUNCTION(this << voltage);
    m_voltage = voltage;
}

void
GpuAccelerator::ClearCurrentTask()
{
    m_currentTask = nullptr;
    m_currentTaskProfile = TaskProfile();
}

GpuAccelerator::TaskProfile
GpuAccelerator::GetRemainingTaskProfile(Time elapsed, double ratio) const
{
    TaskProfile remaining;

    const Time& input = m_currentTaskProfile.inputTransferTime;
    const Time& compute = m_currentTaskProfile.computeTime;
    const Time& output = m_currentTaskProfile.outputTransferTime;

    if (elapsed < input)
    {
        remaining.inputTransferTime = input - elapsed;
        remaining.computeTime = Seconds(compute.GetSeconds() / ratio);
        remaining.outputTransferTime = output;
    }
    else if (elapsed < input + compute)
    {
        Time computeElapsed = elapsed - input;
        remaining.computeTime = Seconds((compute - computeElapsed).GetSeconds() / ratio);
        remaining.outputTransferTime = output;
    }
    else if (elapsed < input + compute + output)
    {
        Time outputElapsed = elapsed - input - compute;
        remaining.outputTransferTime = output - outputElapsed;
    }

    remaining.processingTime = remaining.inputTransferTime + remaining.computeTime + remaining.outputTransferTime;
    return remaining;
}

void
GpuAccelerator::AccumulateCurrentComputeUntil(Time now)
{
    if (!m_currentTask)
    {
        return;
    }

    Time segmentEnd = std::min(now, m_currentSegmentStartTime + m_currentTaskProfile.processingTime);
    Time computeStart = m_currentSegmentStartTime + m_currentTaskProfile.inputTransferTime;
    Time computeEnd = computeStart + m_currentTaskProfile.computeTime;

    Time intervalEnd = std::min(segmentEnd, computeEnd);
    AppendInterval(m_computeIntervals, computeStart, intervalEnd);
    PruneUtilizationHistory(now);
}

void
GpuAccelerator::PruneUtilizationHistory(Time now) const
{
    Time cutoff = now - m_utilizationWindow;
    while (!m_computeIntervals.empty() && m_computeIntervals.front().end <= cutoff)
    {
        m_computeIntervals.pop_front();
    }
}

double
GpuAccelerator::ComputeUtilizationAt(Time now) const
{
    if (m_utilizationWindow.IsZero())
    {
        return 0.0;
    }

    PruneUtilizationHistory(now);

    Time windowStart = now - m_utilizationWindow;
    double computeSeconds = SumWindowedIntervalSeconds(m_computeIntervals, windowStart, now);

    if (m_currentTask)
    {
        Time computeStart = m_currentSegmentStartTime + m_currentTaskProfile.inputTransferTime;
        Time computeEnd = computeStart + m_currentTaskProfile.computeTime;
        Time intervalStart = std::max(computeStart, windowStart);
        Time intervalEnd = std::min(computeEnd, now);
        if (intervalEnd > intervalStart)
        {
            computeSeconds += (intervalEnd - intervalStart).GetSeconds();
        }
    }

    double utilization = computeSeconds / m_utilizationWindow.GetSeconds();
    return std::max(0.0, std::min(1.0, utilization));
}

double
GpuAccelerator::CalculateTaskUtilization(const TaskProfile& profile)
{
    if (profile.processingTime.IsZero())
    {
        return 0.0;
    }
    return profile.computeTime.GetSeconds() / profile.processingTime.GetSeconds();
}

double
GpuAccelerator::SumWindowedIntervalSeconds(const std::deque<TimeInterval>& intervals,
                                           Time windowStart,
                                           Time windowEnd)
{
    double total = 0.0;
    for (const TimeInterval& interval : intervals)
    {
        Time start = std::max(interval.start, windowStart);
        Time end = std::min(interval.end, windowEnd);
        if (end > start)
        {
            total += (end - start).GetSeconds();
        }
    }
    return total;
}

void
GpuAccelerator::AppendInterval(std::deque<TimeInterval>& intervals, Time start, Time end)
{
    if (end <= start)
    {
        return;
    }
    if (!intervals.empty() && intervals.back().end >= start)
    {
        intervals.back().end = std::max(intervals.back().end, end);
        return;
    }
    intervals.push_back(TimeInterval{start, end});
}

} // namespace ns3
