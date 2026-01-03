/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "task-generator.h"

#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TaskGenerator");
NS_OBJECT_ENSURE_REGISTERED(TaskGenerator);

TypeId
TaskGenerator::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TaskGenerator")
            .SetParent<Application>()
            .SetGroupName("Distributed")
            .AddConstructor<TaskGenerator>()
            .AddAttribute("InterArrivalTime",
                          "Random variable for inter-arrival time (seconds)",
                          StringValue("ns3::ExponentialRandomVariable[Mean=0.001]"),
                          MakePointerAccessor(&TaskGenerator::m_interArrivalTime),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("ComputeDemand",
                          "Random variable for compute demand (FLOPS)",
                          StringValue("ns3::ExponentialRandomVariable[Mean=1e9]"),
                          MakePointerAccessor(&TaskGenerator::m_computeDemand),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("InputSize",
                          "Random variable for input data size (bytes)",
                          StringValue("ns3::ExponentialRandomVariable[Mean=1048576]"),
                          MakePointerAccessor(&TaskGenerator::m_inputSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("OutputSize",
                          "Random variable for output data size (bytes)",
                          StringValue("ns3::ExponentialRandomVariable[Mean=1048576]"),
                          MakePointerAccessor(&TaskGenerator::m_outputSize),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("Accelerator",
                          "Target GPU accelerator for task submission",
                          PointerValue(),
                          MakePointerAccessor(&TaskGenerator::m_accelerator),
                          MakePointerChecker<GpuAccelerator>())
            .AddAttribute("MaxTasks",
                          "Maximum number of tasks to generate (0 = unlimited)",
                          UintegerValue(0),
                          MakeUintegerAccessor(&TaskGenerator::m_maxTasks),
                          MakeUintegerChecker<uint64_t>())
            .AddTraceSource("TaskGenerated",
                            "Trace fired when a new task is generated",
                            MakeTraceSourceAccessor(&TaskGenerator::m_taskGeneratedTrace),
                            "ns3::TaskGenerator::TaskTracedCallback");
    return tid;
}

TaskGenerator::TaskGenerator()
    : m_accelerator(nullptr),
      m_taskCount(0),
      m_maxTasks(0)
{
    NS_LOG_FUNCTION(this);
}

TaskGenerator::~TaskGenerator()
{
    NS_LOG_FUNCTION(this);
}

void
TaskGenerator::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(m_generateEvent);
    m_accelerator = nullptr;
    m_interArrivalTime = nullptr;
    m_computeDemand = nullptr;
    m_inputSize = nullptr;
    m_outputSize = nullptr;
    Application::DoDispose();
}

void
TaskGenerator::SetAccelerator(Ptr<GpuAccelerator> accelerator)
{
    NS_LOG_FUNCTION(this << accelerator);
    m_accelerator = accelerator;
}

uint64_t
TaskGenerator::GetTaskCount() const
{
    return m_taskCount;
}

void
TaskGenerator::StartApplication()
{
    NS_LOG_FUNCTION(this);
    m_taskCount = 0;
    ScheduleNextTask();
}

void
TaskGenerator::StopApplication()
{
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(m_generateEvent);
}

void
TaskGenerator::GenerateTask()
{
    NS_LOG_FUNCTION(this);

    // Check if we've reached max tasks
    if (m_maxTasks > 0 && m_taskCount >= m_maxTasks)
    {
        NS_LOG_INFO("Max tasks reached (" << m_maxTasks << "), stopping generation");
        return;
    }

    // Create new task
    Ptr<Task> task = CreateObject<Task>();
    task->SetTaskId(m_taskCount);
    task->SetComputeDemand(m_computeDemand->GetValue());
    task->SetInputSize(static_cast<uint64_t>(m_inputSize->GetValue()));
    task->SetOutputSize(static_cast<uint64_t>(m_outputSize->GetValue()));
    task->SetArrivalTime(Simulator::Now());

    m_taskCount++;

    NS_LOG_INFO("Generated task " << task->GetTaskId() << " at " << Simulator::Now()
                                  << " with compute=" << task->GetComputeDemand()
                                  << " input=" << task->GetInputSize()
                                  << " output=" << task->GetOutputSize());

    // Fire trace
    m_taskGeneratedTrace(task);

    // Submit to accelerator if set
    if (m_accelerator)
    {
        m_accelerator->SubmitTask(task);
    }

    // Schedule next task
    ScheduleNextTask();
}

void
TaskGenerator::ScheduleNextTask()
{
    NS_LOG_FUNCTION(this);

    // Check if we've reached max tasks
    if (m_maxTasks > 0 && m_taskCount >= m_maxTasks)
    {
        return;
    }

    Time nextTime = Seconds(m_interArrivalTime->GetValue());
    NS_LOG_DEBUG("Next task in " << nextTime);
    m_generateEvent = Simulator::Schedule(nextTime, &TaskGenerator::GenerateTask, this);
}

} // namespace ns3
