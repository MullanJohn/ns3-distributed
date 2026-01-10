/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "batching-queue-scheduler.h"

#include "ns3/log.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BatchingQueueScheduler");

NS_OBJECT_ENSURE_REGISTERED(BatchingQueueScheduler);

TypeId
BatchingQueueScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::BatchingQueueScheduler")
            .SetParent<QueueScheduler>()
            .SetGroupName("Distributed")
            .AddConstructor<BatchingQueueScheduler>()
            .AddAttribute("MaxBatchSize",
                          "Maximum number of tasks to dequeue in a batch",
                          UintegerValue(1),
                          MakeUintegerAccessor(&BatchingQueueScheduler::m_maxBatchSize),
                          MakeUintegerChecker<uint32_t>(1));
    return tid;
}

BatchingQueueScheduler::BatchingQueueScheduler()
    : m_maxBatchSize(1)
{
    NS_LOG_FUNCTION(this);
}

BatchingQueueScheduler::~BatchingQueueScheduler()
{
    NS_LOG_FUNCTION(this);
}

void
BatchingQueueScheduler::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Clear();
    QueueScheduler::DoDispose();
}

void
BatchingQueueScheduler::Enqueue(Ptr<Task> task)
{
    NS_LOG_FUNCTION(this << task);
    m_queue.push(task);
    NS_LOG_DEBUG("Enqueued task " << task->GetTaskId() << ", queue length: " << m_queue.size());
}

Ptr<Task>
BatchingQueueScheduler::Dequeue()
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

std::vector<Ptr<Task>>
BatchingQueueScheduler::DequeueBatch()
{
    NS_LOG_FUNCTION(this);
    return DequeueBatch(m_maxBatchSize);
}

std::vector<Ptr<Task>>
BatchingQueueScheduler::DequeueBatch(uint32_t maxBatch)
{
    NS_LOG_FUNCTION(this << maxBatch);

    std::vector<Ptr<Task>> batch;

    if (maxBatch == 0)
    {
        return batch;
    }

    uint32_t count = std::min(maxBatch, static_cast<uint32_t>(m_queue.size()));
    batch.reserve(count);

    for (uint32_t i = 0; i < count; i++)
    {
        batch.push_back(m_queue.front());
        m_queue.pop();
    }

    NS_LOG_DEBUG("Dequeued batch of " << batch.size() << " tasks, queue length: " << m_queue.size());
    return batch;
}

Ptr<Task>
BatchingQueueScheduler::Peek() const
{
    NS_LOG_FUNCTION(this);

    if (m_queue.empty())
    {
        return nullptr;
    }

    return m_queue.front();
}

bool
BatchingQueueScheduler::IsEmpty() const
{
    return m_queue.empty();
}

uint32_t
BatchingQueueScheduler::GetLength() const
{
    return static_cast<uint32_t>(m_queue.size());
}

std::string
BatchingQueueScheduler::GetName() const
{
    return "Batching";
}

uint32_t
BatchingQueueScheduler::GetMaxBatchSize() const
{
    return m_maxBatchSize;
}

void
BatchingQueueScheduler::Clear()
{
    NS_LOG_FUNCTION(this);
    while (!m_queue.empty())
    {
        m_queue.pop();
    }
}

} // namespace ns3
