/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "dag-task.h"

#include "ns3/log.h"

#include <set>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DagTask");
NS_OBJECT_ENSURE_REGISTERED(DagTask);

TypeId
DagTask::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DagTask")
                            .SetParent<Object>()
                            .SetGroupName("Distributed")
                            .AddConstructor<DagTask>();
    return tid;
}

DagTask::DagTask()
{
    NS_LOG_FUNCTION(this);
}

DagTask::~DagTask()
{
    NS_LOG_FUNCTION(this);
}

void
DagTask::DoDispose()
{
    NS_LOG_FUNCTION(this);
    for (auto& node : m_nodes)
    {
        node.task = nullptr;
        node.successors.clear();
        node.dataSuccessors.clear();
    }
    m_nodes.clear();
    m_taskIdToIndex.clear();
    m_readySet.clear();
    m_completedCount = 0;
    Object::DoDispose();
}

uint32_t
DagTask::AddTask(Ptr<Task> task)
{
    NS_LOG_FUNCTION(this << task);
    DagNode node;
    node.task = task;
    node.inDegree = 0;
    node.completed = false;
    m_nodes.push_back(node);

    uint32_t idx = static_cast<uint32_t>(m_nodes.size() - 1);

    // New tasks have inDegree=0, so they are ready
    m_readySet.insert(idx);

    // Add to taskId index for O(1) lookup
    if (task)
    {
        m_taskIdToIndex[task->GetTaskId()] = idx;
    }

    return idx;
}

void
DagTask::AddDependency(uint32_t fromIdx, uint32_t toIdx)
{
    NS_LOG_FUNCTION(this << fromIdx << toIdx);
    if (fromIdx >= m_nodes.size() || toIdx >= m_nodes.size())
    {
        NS_LOG_ERROR("Invalid task index: fromIdx=" << fromIdx << " toIdx=" << toIdx
                                                    << " size=" << m_nodes.size());
        return;
    }
    if (fromIdx == toIdx)
    {
        NS_LOG_ERROR("Self-dependency not allowed: idx=" << fromIdx);
        return;
    }
    m_nodes[fromIdx].successors.push_back(toIdx);
    m_nodes[toIdx].inDegree++;
    if (m_nodes[toIdx].inDegree == 1)
    {
        m_readySet.erase(toIdx);
    }
}

void
DagTask::AddDataDependency(uint32_t fromIdx, uint32_t toIdx)
{
    NS_LOG_FUNCTION(this << fromIdx << toIdx);
    if (fromIdx >= m_nodes.size() || toIdx >= m_nodes.size())
    {
        NS_LOG_ERROR("Invalid task index: fromIdx=" << fromIdx << " toIdx=" << toIdx
                                                    << " size=" << m_nodes.size());
        return;
    }
    if (fromIdx == toIdx)
    {
        NS_LOG_ERROR("Self-dependency not allowed: idx=" << fromIdx);
        return;
    }
    // Create ordering dependency
    m_nodes[fromIdx].successors.push_back(toIdx);
    m_nodes[toIdx].inDegree++;
    if (m_nodes[toIdx].inDegree == 1)
    {
        m_readySet.erase(toIdx);
    }
    // Mark data flow
    m_nodes[fromIdx].dataSuccessors.push_back(toIdx);
}

std::vector<uint32_t>
DagTask::GetReadyTasks() const
{
    NS_LOG_FUNCTION(this);
    return std::vector<uint32_t>(m_readySet.begin(), m_readySet.end());
}

std::vector<uint32_t>
DagTask::GetSinkTasks() const
{
    NS_LOG_FUNCTION(this);
    std::vector<uint32_t> sinks;
    for (uint32_t i = 0; i < m_nodes.size(); i++)
    {
        if (m_nodes[i].successors.empty())
        {
            sinks.push_back(i);
        }
    }
    return sinks;
}

void
DagTask::MarkCompleted(uint32_t idx)
{
    NS_LOG_FUNCTION(this << idx);
    if (idx >= m_nodes.size())
    {
        NS_LOG_ERROR("Invalid task index: " << idx);
        return;
    }
    if (m_nodes[idx].completed)
    {
        NS_LOG_WARN("Task already completed: " << idx);
        return;
    }
    m_nodes[idx].completed = true;
    m_completedCount++;
    m_readySet.erase(idx);
    // Decrement in-degree of all successors
    for (uint32_t successorIdx : m_nodes[idx].successors)
    {
        if (m_nodes[successorIdx].inDegree > 0)
        {
            m_nodes[successorIdx].inDegree--;
            if (m_nodes[successorIdx].inDegree == 0 && !m_nodes[successorIdx].completed)
            {
                m_readySet.insert(successorIdx);
            }
        }
    }
    // Propagate data to data-dependent successors
    uint64_t outputSize = m_nodes[idx].task->GetOutputSize();
    for (uint32_t successorIdx : m_nodes[idx].dataSuccessors)
    {
        uint64_t currentInput = m_nodes[successorIdx].task->GetInputSize();
        m_nodes[successorIdx].task->SetInputSize(currentInput + outputSize);
    }
}

Ptr<Task>
DagTask::GetTask(uint32_t idx) const
{
    if (idx >= m_nodes.size())
    {
        return nullptr;
    }
    return m_nodes[idx].task;
}

int32_t
DagTask::GetTaskIndex(uint64_t taskId) const
{
    NS_LOG_FUNCTION(this << taskId);
    auto it = m_taskIdToIndex.find(taskId);
    if (it != m_taskIdToIndex.end())
    {
        return static_cast<int32_t>(it->second);
    }
    return -1;
}

bool
DagTask::SetTask(uint32_t idx, Ptr<Task> task)
{
    NS_LOG_FUNCTION(this << idx << task);
    if (idx >= m_nodes.size())
    {
        NS_LOG_ERROR("Invalid task index: " << idx);
        return false;
    }

    // Update taskId index if task ID changed
    Ptr<Task> oldTask = m_nodes[idx].task;
    if (oldTask && task && oldTask->GetTaskId() != task->GetTaskId())
    {
        m_taskIdToIndex.erase(oldTask->GetTaskId());
        m_taskIdToIndex[task->GetTaskId()] = idx;
    }
    else if (!oldTask && task)
    {
        m_taskIdToIndex[task->GetTaskId()] = idx;
    }
    else if (oldTask && !task)
    {
        m_taskIdToIndex.erase(oldTask->GetTaskId());
    }

    m_nodes[idx].task = task;
    return true;
}

uint32_t
DagTask::GetTaskCount() const
{
    return static_cast<uint32_t>(m_nodes.size());
}

bool
DagTask::IsComplete() const
{
    NS_LOG_FUNCTION(this);
    return m_completedCount == m_nodes.size();
}

bool
DagTask::Validate() const
{
    NS_LOG_FUNCTION(this);
    if (m_nodes.empty())
    {
        return true;
    }

    // DFS-based cycle detection using coloring:
    // 0 = white (unvisited), 1 = gray (in current path), 2 = black (done)
    std::vector<int> color(m_nodes.size(), 0);

    // Helper lambda for DFS - returns true if cycle found
    std::function<bool(uint32_t)> hasCycle = [&](uint32_t u) -> bool {
        color[u] = 1; // Mark as in-progress
        for (uint32_t v : m_nodes[u].successors)
        {
            if (color[v] == 1)
            {
                // Back edge found - cycle detected
                return true;
            }
            if (color[v] == 0 && hasCycle(v))
            {
                return true;
            }
        }
        color[u] = 2; // Mark as done
        return false;
    };

    // Check all nodes (handles disconnected components)
    for (uint32_t i = 0; i < m_nodes.size(); i++)
    {
        if (color[i] == 0 && hasCycle(i))
        {
            NS_LOG_WARN("Cycle detected in DAG");
            return false;
        }
    }
    return true;
}

Ptr<Packet>
DagTask::SerializeMetadata() const
{
    NS_LOG_FUNCTION(this);
    return SerializeInternal(true);
}

Ptr<Packet>
DagTask::SerializeFullData() const
{
    NS_LOG_FUNCTION(this);
    return SerializeInternal(false);
}

Ptr<DagTask>
DagTask::DeserializeMetadata(Ptr<Packet> packet,
                             Callback<Ptr<Task>, Ptr<Packet>, uint64_t&> deserializer,
                             uint64_t& consumedBytes)
{
    NS_LOG_FUNCTION(packet);
    return DeserializeInternal(packet, deserializer, consumedBytes);
}

Ptr<DagTask>
DagTask::DeserializeFullData(Ptr<Packet> packet,
                             Callback<Ptr<Task>, Ptr<Packet>, uint64_t&> deserializer,
                             uint64_t& consumedBytes)
{
    NS_LOG_FUNCTION(packet);
    return DeserializeInternal(packet, deserializer, consumedBytes);
}

Ptr<Packet>
DagTask::SerializeInternal(bool metadataOnly) const
{
    NS_LOG_FUNCTION(this << metadataOnly);

    Ptr<Packet> result = Create<Packet>();

    // Write task count (4 bytes, network byte order)
    uint32_t taskCount = static_cast<uint32_t>(m_nodes.size());
    NS_ASSERT_MSG(taskCount < (1u << 24), "DAG task count exceeds wire protocol limit");
    uint8_t countBuf[4];
    countBuf[0] = (taskCount >> 24) & 0xFF;
    countBuf[1] = (taskCount >> 16) & 0xFF;
    countBuf[2] = (taskCount >> 8) & 0xFF;
    countBuf[3] = taskCount & 0xFF;
    result->AddAtEnd(Create<Packet>(countBuf, 4));

    // Serialize each task
    for (uint32_t i = 0; i < m_nodes.size(); i++)
    {
        Ptr<Task> task = m_nodes[i].task;
        Ptr<Packet> taskPacket;

        if (metadataOnly)
        {
            // Serialize header only by taking a fragment of the full serialization
            Ptr<Packet> fullPacket = task->Serialize(false);
            uint32_t headerSize = task->GetSerializedHeaderSize();
            taskPacket = fullPacket->CreateFragment(0, headerSize);
        }
        else
        {
            taskPacket = task->Serialize(false);
        }

        // Prepend task type byte for dispatch-based deserialization
        uint8_t typeByte = task->GetTaskType();
        Ptr<Packet> typePrefix = Create<Packet>(&typeByte, 1);
        typePrefix->AddAtEnd(taskPacket);
        taskPacket = typePrefix;

        // Write task serialized size (8 bytes, network byte order)
        uint64_t taskSize = taskPacket->GetSize();
        uint8_t sizeBuf[8];
        sizeBuf[0] = (taskSize >> 56) & 0xFF;
        sizeBuf[1] = (taskSize >> 48) & 0xFF;
        sizeBuf[2] = (taskSize >> 40) & 0xFF;
        sizeBuf[3] = (taskSize >> 32) & 0xFF;
        sizeBuf[4] = (taskSize >> 24) & 0xFF;
        sizeBuf[5] = (taskSize >> 16) & 0xFF;
        sizeBuf[6] = (taskSize >> 8) & 0xFF;
        sizeBuf[7] = taskSize & 0xFF;
        result->AddAtEnd(Create<Packet>(sizeBuf, 8));

        // Write task bytes
        result->AddAtEnd(taskPacket);
    }

    // Count edges (all entries in successors lists)
    uint32_t edgeCount = 0;
    for (const auto& node : m_nodes)
    {
        edgeCount += static_cast<uint32_t>(node.successors.size());
    }

    // Write edge count (4 bytes, network byte order)
    uint8_t edgeCountBuf[4];
    edgeCountBuf[0] = (edgeCount >> 24) & 0xFF;
    edgeCountBuf[1] = (edgeCount >> 16) & 0xFF;
    edgeCountBuf[2] = (edgeCount >> 8) & 0xFF;
    edgeCountBuf[3] = edgeCount & 0xFF;
    result->AddAtEnd(Create<Packet>(edgeCountBuf, 4));

    // Write edges
    for (uint32_t fromIdx = 0; fromIdx < m_nodes.size(); fromIdx++)
    {
        const DagNode& node = m_nodes[fromIdx];

        // Build set of data successors for quick lookup
        std::set<uint32_t> dataSuccSet(node.dataSuccessors.begin(), node.dataSuccessors.end());

        for (uint32_t toIdx : node.successors)
        {
            uint8_t edgeBuf[9];
            edgeBuf[0] = (fromIdx >> 24) & 0xFF;
            edgeBuf[1] = (fromIdx >> 16) & 0xFF;
            edgeBuf[2] = (fromIdx >> 8) & 0xFF;
            edgeBuf[3] = fromIdx & 0xFF;
            edgeBuf[4] = (toIdx >> 24) & 0xFF;
            edgeBuf[5] = (toIdx >> 16) & 0xFF;
            edgeBuf[6] = (toIdx >> 8) & 0xFF;
            edgeBuf[7] = toIdx & 0xFF;
            edgeBuf[8] = dataSuccSet.count(toIdx) ? 1 : 0;
            result->AddAtEnd(Create<Packet>(edgeBuf, 9));
        }
    }

    return result;
}

Ptr<DagTask>
DagTask::DeserializeInternal(Ptr<Packet> packet,
                             Callback<Ptr<Task>, Ptr<Packet>, uint64_t&> deserializer,
                             uint64_t& consumedBytes)
{
    NS_LOG_FUNCTION(packet);
    consumedBytes = 0;

    // Need at least 4 bytes for task count
    if (packet->GetSize() < 4)
    {
        NS_LOG_WARN("Not enough data for task count");
        return nullptr;
    }

    uint64_t offset = 0;

    // Read task count
    uint8_t countBuf[4];
    Ptr<Packet> countFragment = packet->CreateFragment(offset, 4);
    countFragment->CopyData(countBuf, 4);
    uint32_t taskCount =
        (static_cast<uint32_t>(countBuf[0]) << 24) | (static_cast<uint32_t>(countBuf[1]) << 16) |
        (static_cast<uint32_t>(countBuf[2]) << 8) | static_cast<uint32_t>(countBuf[3]);
    offset += 4;

    Ptr<DagTask> dag = CreateObject<DagTask>();

    // Deserialize each task
    for (uint32_t i = 0; i < taskCount; i++)
    {
        // Need 8 bytes for task size
        if (packet->GetSize() < offset + 8)
        {
            NS_LOG_WARN("Not enough data for task size at index " << i);
            return nullptr;
        }

        // Read task serialized size
        uint8_t sizeBuf[8];
        Ptr<Packet> sizeFragment = packet->CreateFragment(offset, 8);
        sizeFragment->CopyData(sizeBuf, 8);
        uint64_t taskSize = 0;
        for (int j = 0; j < 8; j++)
        {
            taskSize = (taskSize << 8) | sizeBuf[j];
        }
        offset += 8;

        // Check we have enough data for the task
        if (packet->GetSize() < offset + taskSize)
        {
            NS_LOG_WARN("Not enough data for task at index " << i);
            return nullptr;
        }

        // Extract task bytes as a fragment and deserialize
        Ptr<Packet> taskPacket = packet->CreateFragment(offset, taskSize);
        uint64_t taskConsumed = 0;
        Ptr<Task> task = deserializer(taskPacket, taskConsumed);

        if (!task)
        {
            NS_LOG_WARN("Failed to deserialize task at index " << i);
            return nullptr;
        }

        dag->AddTask(task);
        offset += taskSize;
    }

    // Need 4 bytes for edge count
    if (packet->GetSize() < offset + 4)
    {
        NS_LOG_WARN("Not enough data for edge count");
        return nullptr;
    }

    // Read edge count
    uint8_t edgeCountBuf[4];
    Ptr<Packet> edgeCountFragment = packet->CreateFragment(offset, 4);
    edgeCountFragment->CopyData(edgeCountBuf, 4);
    uint32_t edgeCount = (static_cast<uint32_t>(edgeCountBuf[0]) << 24) |
                         (static_cast<uint32_t>(edgeCountBuf[1]) << 16) |
                         (static_cast<uint32_t>(edgeCountBuf[2]) << 8) |
                         static_cast<uint32_t>(edgeCountBuf[3]);
    offset += 4;

    // Read edges
    for (uint32_t i = 0; i < edgeCount; i++)
    {
        if (packet->GetSize() < offset + 9)
        {
            NS_LOG_WARN("Not enough data for edge " << i);
            return nullptr;
        }

        uint8_t edgeBuf[9];
        Ptr<Packet> edgeFragment = packet->CreateFragment(offset, 9);
        edgeFragment->CopyData(edgeBuf, 9);

        uint32_t fromIdx =
            (static_cast<uint32_t>(edgeBuf[0]) << 24) | (static_cast<uint32_t>(edgeBuf[1]) << 16) |
            (static_cast<uint32_t>(edgeBuf[2]) << 8) | static_cast<uint32_t>(edgeBuf[3]);
        uint32_t toIdx =
            (static_cast<uint32_t>(edgeBuf[4]) << 24) | (static_cast<uint32_t>(edgeBuf[5]) << 16) |
            (static_cast<uint32_t>(edgeBuf[6]) << 8) | static_cast<uint32_t>(edgeBuf[7]);
        bool isData = (edgeBuf[8] != 0);

        if (fromIdx >= taskCount || toIdx >= taskCount)
        {
            NS_LOG_WARN("Invalid edge indices: " << fromIdx << " -> " << toIdx);
            return nullptr;
        }

        if (isData)
        {
            dag->AddDataDependency(fromIdx, toIdx);
        }
        else
        {
            dag->AddDependency(fromIdx, toIdx);
        }

        offset += 9;
    }

    consumedBytes = offset;
    return dag;
}

} // namespace ns3
