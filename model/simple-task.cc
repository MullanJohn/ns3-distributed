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
    header.SetAcceleratorType(GetRequiredAcceleratorType());

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

Ptr<Task>
SimpleTask::Deserialize(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    NS_LOG_FUNCTION(packet);
    consumedBytes = 0;

    // Check if we have enough data for the header
    if (packet->GetSize() < SimpleTaskHeader::SERIALIZED_SIZE)
    {
        NS_LOG_DEBUG("Not enough data for header: have " << packet->GetSize() << ", need "
                                                         << SimpleTaskHeader::SERIALIZED_SIZE);
        return nullptr;
    }

    // Peek at header to determine total message size (non-destructive)
    SimpleTaskHeader header;
    packet->PeekHeader(header);

    uint64_t payloadSize =
        header.IsResponse() ? header.GetResponsePayloadSize() : header.GetRequestPayloadSize();
    uint64_t totalSize = SimpleTaskHeader::SERIALIZED_SIZE + payloadSize;

    // Check if we have the complete message
    if (packet->GetSize() < totalSize)
    {
        NS_LOG_DEBUG("Not enough data for message: have " << packet->GetSize() << ", need "
                                                          << totalSize);
        return nullptr;
    }

    // Create task from header data (non-destructive - caller removes bytes)
    Ptr<SimpleTask> task = CreateObject<SimpleTask>();
    task->SetTaskId(header.GetTaskId());
    task->SetComputeDemand(header.GetComputeDemand());
    task->SetInputSize(header.GetInputSize());
    task->SetOutputSize(header.GetOutputSize());
    task->SetRequiredAcceleratorType(header.GetAcceleratorType());

    if (header.HasDeadline())
    {
        task->SetDeadline(NanoSeconds(header.GetDeadlineNs()));
    }

    // Report consumed bytes - caller is responsible for removing from buffer
    consumedBytes = totalSize;
    return task;
}

} // namespace ns3
