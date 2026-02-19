/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "task.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Task");
NS_OBJECT_ENSURE_REGISTERED(Task);

TypeId
Task::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Task")
            .SetParent<Object>()
            .SetGroupName("Distributed")
            .AddAttribute("InputSize",
                          "Input data size in bytes",
                          UintegerValue(0),
                          MakeUintegerAccessor(&Task::m_inputSize),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("OutputSize",
                          "Output data size in bytes",
                          UintegerValue(0),
                          MakeUintegerAccessor(&Task::m_outputSize),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("ComputeDemand",
                          "Compute demand in FLOPS",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&Task::m_computeDemand),
                          MakeDoubleChecker<double>(0))
            .AddAttribute("Priority",
                          "Task priority (higher value = higher priority)",
                          UintegerValue(0),
                          MakeUintegerAccessor(&Task::m_priority),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RequiredAcceleratorType",
                          "Required accelerator type (e.g., GPU, TPU). Empty means any.",
                          StringValue(""),
                          MakeStringAccessor(&Task::m_requiredAcceleratorType),
                          MakeStringChecker());
    // Note: No AddConstructor because this is an abstract class
    return tid;
}

Task::Task()
{
    NS_LOG_FUNCTION(this);
}

Task::~Task()
{
    NS_LOG_FUNCTION(this);
}

void
Task::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_taskId = 0;
    m_inputSize = 0;
    m_outputSize = 0;
    m_computeDemand = 0.0;
    m_arrivalTime = Seconds(0);
    m_deadline = Time(-1);
    m_priority = 0;
    m_requiredAcceleratorType.clear();
    Object::DoDispose();
}

uint64_t
Task::GetTaskId() const
{
    return m_taskId;
}

void
Task::SetTaskId(uint64_t id)
{
    NS_LOG_FUNCTION(this << id);
    m_taskId = id;
}

uint64_t
Task::GetInputSize() const
{
    return m_inputSize;
}

void
Task::SetInputSize(uint64_t bytes)
{
    NS_LOG_FUNCTION(this << bytes);
    m_inputSize = bytes;
}

uint64_t
Task::GetOutputSize() const
{
    return m_outputSize;
}

void
Task::SetOutputSize(uint64_t bytes)
{
    NS_LOG_FUNCTION(this << bytes);
    m_outputSize = bytes;
}

double
Task::GetComputeDemand() const
{
    return m_computeDemand;
}

void
Task::SetComputeDemand(double flops)
{
    NS_LOG_FUNCTION(this << flops);
    m_computeDemand = flops;
}

Time
Task::GetArrivalTime() const
{
    return m_arrivalTime;
}

void
Task::SetArrivalTime(Time time)
{
    NS_LOG_FUNCTION(this << time);
    m_arrivalTime = time;
}

bool
Task::HasDeadline() const
{
    return m_deadline >= Seconds(0);
}

Time
Task::GetDeadline() const
{
    return m_deadline;
}

void
Task::SetDeadline(Time deadline)
{
    NS_LOG_FUNCTION(this << deadline);
    m_deadline = deadline;
}

void
Task::ClearDeadline()
{
    NS_LOG_FUNCTION(this);
    m_deadline = Time(-1);
}

uint32_t
Task::GetPriority() const
{
    return m_priority;
}

void
Task::SetPriority(uint32_t priority)
{
    NS_LOG_FUNCTION(this << priority);
    m_priority = priority;
}

std::string
Task::GetRequiredAcceleratorType() const
{
    return m_requiredAcceleratorType;
}

void
Task::SetRequiredAcceleratorType(const std::string& type)
{
    NS_LOG_FUNCTION(this << type);
    m_requiredAcceleratorType = type;
}

} // namespace ns3
