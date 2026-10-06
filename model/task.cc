/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "task.h"

#include "task-header.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Task");

std::ostream&
operator<<(std::ostream& os, TaskState state)
{
    switch (state)
    {
    case TASK_CREATED:
        os << "CREATED";
        break;
    case TASK_SUBMITTED:
        os << "SUBMITTED";
        break;
    case TASK_ADMITTED:
        os << "ADMITTED";
        break;
    case TASK_DISPATCHED:
        os << "DISPATCHED";
        break;
    case TASK_RUNNING:
        os << "RUNNING";
        break;
    case TASK_COMPLETED:
        os << "COMPLETED";
        break;
    case TASK_FAILED:
        os << "FAILED";
        break;
    case TASK_REJECTED:
        os << "REJECTED";
        break;
    }
    return os;
}

NS_OBJECT_ENSURE_REGISTERED(Task);

TypeId
Task::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Task")
            .SetParent<Object>()
            .SetGroupName("Continuum")
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
                          MakeStringChecker())
            .AddAttribute("InputSizeDistribution",
                          "Random variable Sample() draws InputSize from. When unset, "
                          "Sample() uses the plain InputSize attribute unchanged (default "
                          "0, i.e. this task's input is entirely relayed from an upstream "
                          "task via a data dependency).",
                          PointerValue(),
                          MakePointerAccessor(&Task::m_inputSizeDistribution),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("OutputSizeDistribution",
                          "Random variable Sample() draws OutputSize from. When unset, "
                          "Sample() uses the plain OutputSize attribute unchanged.",
                          PointerValue(),
                          MakePointerAccessor(&Task::m_outputSizeDistribution),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("ComputeDemandDistribution",
                          "Random variable Sample() draws ComputeDemand from. When unset, "
                          "Sample() uses the plain ComputeDemand attribute unchanged.",
                          PointerValue(),
                          MakePointerAccessor(&Task::m_computeDemandDistribution),
                          MakePointerChecker<RandomVariableStream>())
            .AddTraceSource("State",
                            "Task lifecycle state transitions",
                            MakeTraceSourceAccessor(&Task::m_state),
                            "ns3::TaskStateTracedCallback");
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

Ptr<Packet>
Task::Serialize(bool isResponse) const
{
    NS_LOG_FUNCTION(this << isResponse);

    TaskHeader header;
    header.SetMessageType(isResponse ? TaskHeader::TASK_RESPONSE : TaskHeader::TASK_REQUEST);
    header.SetTaskId(m_taskId);
    header.SetComputeDemand(m_computeDemand);
    header.SetInputSize(m_inputSize);
    header.SetOutputSize(m_outputSize);
    header.SetDeadlineNs(m_deadline.IsNegative() ? -1 : m_deadline.GetNanoSeconds());
    header.SetPriority(m_priority);
    header.SetAcceleratorType(GetRequiredAcceleratorType());
    header.SetBackendTimeNs(m_backendTime.GetNanoSeconds());
    header.SetComputeTimeNs(m_computeTime.GetNanoSeconds());

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(header);

    uint64_t payloadSize = isResponse ? m_outputSize : m_inputSize;
    if (payloadSize > 0)
    {
        Ptr<Packet> payload = Create<Packet>(payloadSize);
        packet->AddAtEnd(payload);
    }

    return packet;
}

uint32_t
Task::GetSerializedHeaderSize() const
{
    return TaskHeader::SERIALIZED_SIZE;
}

void
Task::PopulateFromHeader(const TaskHeader& header)
{
    m_taskId = header.GetTaskId();
    m_computeDemand = header.GetComputeDemand();
    m_inputSize = header.GetInputSize();
    m_outputSize = header.GetOutputSize();
    m_priority = header.GetPriority();
    m_requiredAcceleratorType = header.GetAcceleratorType();
    if (header.HasDeadline())
    {
        m_deadline = NanoSeconds(header.GetDeadlineNs());
    }
    m_backendTime = NanoSeconds(header.GetBackendTimeNs());
    m_computeTime = NanoSeconds(header.GetComputeTimeNs());
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
    m_computeTime = Seconds(0);
    m_backendTime = Seconds(0);
    m_inputSizeDistribution = nullptr;
    m_outputSizeDistribution = nullptr;
    m_computeDemandDistribution = nullptr;
    Object::DoDispose();
}

Ptr<Task>
Task::Sample() const
{
    NS_LOG_FUNCTION(this);

    ObjectFactory factory;
    factory.SetTypeId(GetInstanceTypeId());
    Ptr<Task> fresh = factory.Create<Task>();

    fresh->SetPriority(m_priority);
    fresh->SetRequiredAcceleratorType(m_requiredAcceleratorType);
    fresh->SetInputSize(m_inputSizeDistribution
                             ? static_cast<uint64_t>(m_inputSizeDistribution->GetValue())
                             : m_inputSize);
    fresh->SetOutputSize(m_outputSizeDistribution
                              ? static_cast<uint64_t>(m_outputSizeDistribution->GetValue())
                              : m_outputSize);
    fresh->SetComputeDemand(m_computeDemandDistribution ? m_computeDemandDistribution->GetValue()
                                                        : m_computeDemand);

    return fresh;
}

int64_t
Task::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);

    int64_t current = stream;
    if (m_inputSizeDistribution)
    {
        m_inputSizeDistribution->SetStream(current++);
    }
    if (m_outputSizeDistribution)
    {
        m_outputSizeDistribution->SetStream(current++);
    }
    if (m_computeDemandDistribution)
    {
        m_computeDemandDistribution->SetStream(current++);
    }
    return current - stream;
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

Time
Task::GetComputeTime() const
{
    return m_computeTime;
}

void
Task::SetComputeTime(Time time)
{
    NS_LOG_FUNCTION(this << time);
    m_computeTime = time;
}

Time
Task::GetBackendTime() const
{
    return m_backendTime;
}

void
Task::SetBackendTime(Time time)
{
    NS_LOG_FUNCTION(this << time);
    m_backendTime = time;
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

TaskState
Task::GetState() const
{
    return m_state;
}

void
Task::SetState(TaskState newState)
{
    NS_LOG_FUNCTION(this << newState);

    TaskState current = m_state;

    bool valid = false;
    switch (current)
    {
    case TASK_CREATED:
        valid = (newState == TASK_SUBMITTED || newState == TASK_REJECTED);
        break;
    case TASK_SUBMITTED:
        valid = (newState == TASK_ADMITTED || newState == TASK_REJECTED);
        break;
    case TASK_ADMITTED:
        valid = (newState == TASK_DISPATCHED || newState == TASK_FAILED);
        break;
    case TASK_DISPATCHED:
        valid = (newState == TASK_RUNNING || newState == TASK_FAILED);
        break;
    case TASK_RUNNING:
        valid = (newState == TASK_COMPLETED || newState == TASK_FAILED);
        break;
    case TASK_COMPLETED:
    case TASK_FAILED:
    case TASK_REJECTED:
        break;
    }

    if (!valid)
    {
        NS_LOG_WARN("Invalid task state transition: " << current << " -> " << newState
                                                      << " for task " << m_taskId);
        return;
    }

    m_state = newState;
}

} // namespace ns3
