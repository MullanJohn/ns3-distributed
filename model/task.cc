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
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Task");
NS_OBJECT_ENSURE_REGISTERED(Task);

TypeId
Task::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Task")
                            .SetParent<Object>()
                            .SetGroupName("Distributed")
                            .AddConstructor<Task>()
                            .AddAttribute("ComputeDemand",
                                          "Compute demand in FLOPS",
                                          DoubleValue(1e9),
                                          MakeDoubleAccessor(&Task::m_computeDemand),
                                          MakeDoubleChecker<double>(0))
                            .AddAttribute("InputSize",
                                          "Input data size in bytes",
                                          UintegerValue(1024),
                                          MakeUintegerAccessor(&Task::m_inputSize),
                                          MakeUintegerChecker<uint64_t>())
                            .AddAttribute("OutputSize",
                                          "Output data size in bytes",
                                          UintegerValue(1024),
                                          MakeUintegerAccessor(&Task::m_outputSize),
                                          MakeUintegerChecker<uint64_t>());
    return tid;
}

Task::Task()
    : m_computeDemand(1e9),
      m_inputSize(1024),
      m_outputSize(1024),
      m_arrivalTime(Seconds(0)),
      m_taskId(0)
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
    Object::DoDispose();
}

void
Task::SetComputeDemand(double flops)
{
    NS_LOG_FUNCTION(this << flops);
    m_computeDemand = flops;
}

void
Task::SetInputSize(uint64_t bytes)
{
    NS_LOG_FUNCTION(this << bytes);
    m_inputSize = bytes;
}

void
Task::SetOutputSize(uint64_t bytes)
{
    NS_LOG_FUNCTION(this << bytes);
    m_outputSize = bytes;
}

void
Task::SetArrivalTime(Time time)
{
    NS_LOG_FUNCTION(this << time);
    m_arrivalTime = time;
}

void
Task::SetTaskId(uint64_t id)
{
    NS_LOG_FUNCTION(this << id);
    m_taskId = id;
}

double
Task::GetComputeDemand() const
{
    NS_LOG_FUNCTION(this);
    return m_computeDemand;
}

uint64_t
Task::GetInputSize() const
{
    NS_LOG_FUNCTION(this);
    return m_inputSize;
}

uint64_t
Task::GetOutputSize() const
{
    NS_LOG_FUNCTION(this);
    return m_outputSize;
}

Time
Task::GetArrivalTime() const
{
    NS_LOG_FUNCTION(this);
    return m_arrivalTime;
}

uint64_t
Task::GetTaskId() const
{
    NS_LOG_FUNCTION(this);
    return m_taskId;
}

} // namespace ns3
