/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "simple-task.h"

#include "simple-task-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SimpleTask");
NS_OBJECT_ENSURE_REGISTERED(SimpleTask);

TypeId
SimpleTask::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SimpleTask")
                            .SetParent<Task>()
                            .SetGroupName("Distributed")
                            .AddConstructor<SimpleTask>();
    return tid;
}

SimpleTask::SimpleTask()
{
    NS_LOG_FUNCTION(this);
}

SimpleTask::~SimpleTask()
{
    NS_LOG_FUNCTION(this);
}

void
SimpleTask::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Task::DoDispose();
}

std::string
SimpleTask::GetName() const
{
    return "SimpleTask";
}

Ptr<Packet>
SimpleTask::Serialize(bool isResponse) const
{
    NS_LOG_FUNCTION(this << isResponse);

    SimpleTaskHeader header;
    header.SetMessageType(isResponse ? TaskHeader::TASK_RESPONSE : TaskHeader::TASK_REQUEST);
    header.SetTaskId(m_taskId);
    header.SetComputeDemand(m_computeDemand);
    header.SetInputSize(m_inputSize);
    header.SetOutputSize(m_outputSize);
    header.SetDeadlineNs(m_deadline.IsNegative() ? -1 : m_deadline.GetNanoSeconds());

    // Create packet with header
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(header);

    // Add payload padding (simulates actual data transfer)
    uint64_t payloadSize = isResponse ? m_outputSize : m_inputSize;
    if (payloadSize > 0)
    {
        Ptr<Packet> payload = Create<Packet>(payloadSize);
        packet->AddAtEnd(payload);
    }

    return packet;
}

uint32_t
SimpleTask::GetSerializedHeaderSize() const
{
    return SimpleTaskHeader::SERIALIZED_SIZE;
}

Ptr<SimpleTask>
SimpleTask::Deserialize(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(packet);

    SimpleTaskHeader header;
    packet->RemoveHeader(header);

    Ptr<SimpleTask> task = CreateObject<SimpleTask>();
    task->SetTaskId(header.GetTaskId());
    task->SetComputeDemand(header.GetComputeDemand());
    task->SetInputSize(header.GetInputSize());
    task->SetOutputSize(header.GetOutputSize());

    if (header.HasDeadline())
    {
        task->SetDeadline(NanoSeconds(header.GetDeadlineNs()));
    }

    return task;
}

} // namespace ns3
