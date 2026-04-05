/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "simple-task.h"

#include "task-header.h"

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

uint8_t
SimpleTask::GetTaskType() const
{
    return TASK_TYPE;
}

Ptr<Task>
SimpleTask::Deserialize(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    NS_LOG_FUNCTION(packet);
    consumedBytes = 0;

    if (packet->GetSize() < TaskHeader::SERIALIZED_SIZE)
    {
        NS_LOG_DEBUG("Not enough data for header: have " << packet->GetSize() << ", need "
                                                         << TaskHeader::SERIALIZED_SIZE);
        return nullptr;
    }

    TaskHeader header;
    packet->PeekHeader(header);

    uint64_t payloadSize =
        header.IsResponse() ? header.GetResponsePayloadSize() : header.GetRequestPayloadSize();
    uint64_t totalSize = TaskHeader::SERIALIZED_SIZE + payloadSize;

    if (packet->GetSize() < totalSize)
    {
        NS_LOG_DEBUG("Not enough data for message: have " << packet->GetSize() << ", need "
                                                          << totalSize);
        return nullptr;
    }

    Ptr<SimpleTask> task = CreateObject<SimpleTask>();
    task->PopulateFromHeader(header);

    consumedBytes = totalSize;
    return task;
}

Ptr<Task>
SimpleTask::DeserializeHeader(Ptr<Packet> packet, uint64_t& consumedBytes)
{
    NS_LOG_FUNCTION(packet);
    consumedBytes = 0;

    if (packet->GetSize() < TaskHeader::SERIALIZED_SIZE)
    {
        NS_LOG_DEBUG("Not enough data for header: have " << packet->GetSize() << ", need "
                                                         << TaskHeader::SERIALIZED_SIZE);
        return nullptr;
    }

    TaskHeader header;
    packet->PeekHeader(header);

    Ptr<SimpleTask> task = CreateObject<SimpleTask>();
    task->PopulateFromHeader(header);

    consumedBytes = TaskHeader::SERIALIZED_SIZE;
    return task;
}

} // namespace ns3
