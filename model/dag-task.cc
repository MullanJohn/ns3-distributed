/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "dag-task.h"

#include "ns3/log.h"

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
    // Mark data flow
    m_nodes[fromIdx].dataSuccessors.push_back(toIdx);
}

std::vector<uint32_t>
DagTask::GetReadyTasks() const
{
    NS_LOG_FUNCTION(this);
    std::vector<uint32_t> ready;
    for (uint32_t i = 0; i < m_nodes.size(); i++)
    {
        if (!m_nodes[i].completed && m_nodes[i].inDegree == 0)
        {
            ready.push_back(i);
        }
    }
    return ready;
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
    // Decrement in-degree of all successors
    for (uint32_t successorIdx : m_nodes[idx].successors)
    {
        if (m_nodes[successorIdx].inDegree > 0)
        {
            m_nodes[successorIdx].inDegree--;
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

} // namespace ns3
