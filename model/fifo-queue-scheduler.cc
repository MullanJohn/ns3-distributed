/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "fifo-queue-scheduler.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FifoQueueScheduler");

NS_OBJECT_ENSURE_REGISTERED(FifoQueueScheduler);

TypeId
FifoQueueScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::FifoQueueScheduler")
                            .SetParent<QueueScheduler>()
                            .SetGroupName("Distributed")
                            .AddConstructor<FifoQueueScheduler>();
    return tid;
}

FifoQueueScheduler::FifoQueueScheduler()
{
    NS_LOG_FUNCTION(this);
}

FifoQueueScheduler::~FifoQueueScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
FifoQueueScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Clear();
    QueueScheduler::DoDispose();
}

void
FifoQueueScheduler::Enqueue(Ptr<Task> task)
{
    NS_LOG_FUNCTION(this << task);
    m_queue.push(task);
    NS_LOG_DEBUG("Enqueued task " << task->GetTaskId() << ", queue length: " << m_queue.size());
}

Ptr<Task>
FifoQueueScheduler::Dequeue()
{
    NS_LOG_FUNCTION(this);

    if (m_queue.empty())
    {
        NS_LOG_DEBUG("Dequeue called on empty queue");
        return nullptr;
    }

    Ptr<Task> task = m_queue.front();
    m_queue.pop();
    NS_LOG_DEBUG("Dequeued task " << task->GetTaskId() << ", queue length: " << m_queue.size());
    return task;
}

Ptr<Task>
FifoQueueScheduler::Peek() const
{
    NS_LOG_FUNCTION(this);

    if (m_queue.empty())
    {
        return nullptr;
    }

    return m_queue.front();
}

bool
FifoQueueScheduler::IsEmpty() const
{
    return m_queue.empty();
}

uint32_t
FifoQueueScheduler::GetLength() const
{
    return static_cast<uint32_t>(m_queue.size());
}

std::string
FifoQueueScheduler::GetName() const
{
    return "FIFO";
}

void
FifoQueueScheduler::Clear()
{
    NS_LOG_FUNCTION(this);
    while (!m_queue.empty())
    {
        m_queue.pop();
    }
}

} // namespace ns3
