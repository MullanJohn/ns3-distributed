/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DAG_TASK_H
#define DAG_TASK_H

#include "task.h"

#include "ns3/object.h"
#include "ns3/ptr.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Container for a directed acyclic graph (DAG) of tasks.
 *
 * DagTask holds multiple Task objects and their dependency edges,
 * enabling scheduling of task graphs where some tasks must complete
 * before others can begin.
 *
 * DagTask is a container, not a Task itself. It tracks task completion
 * and provides efficient access to ready tasks (those with all
 * dependencies satisfied).
 *
 * Example (diamond DAG: a->c, b->c):
 * @code
 * DagTask dag;
 * uint32_t a = dag.AddTask(taskA);
 * uint32_t b = dag.AddTask(taskB);
 * uint32_t c = dag.AddTask(taskC);
 * dag.AddDependency(a, c);  // a must complete before c
 * dag.AddDependency(b, c);  // b must complete before c
 *
 * // Initially: GetReadyTasks() returns {a, b}
 * // After MarkCompleted(a): GetReadyTasks() returns {b}
 * // After MarkCompleted(b): GetReadyTasks() returns {c}
 * @endcode
 */
class DagTask : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    DagTask();
    ~DagTask() override;

    /**
     * @brief Add a task to the DAG.
     * @param task The task to add.
     * @return The index of the added task.
     */
    uint32_t AddTask(Ptr<Task> task);

    /**
     * @brief Add a dependency edge between tasks.
     *
     * The task at fromIdx must complete before the task at toIdx can start.
     *
     * @param fromIdx The index of the predecessor task.
     * @param toIdx The index of the successor task.
     */
    void AddDependency(uint32_t fromIdx, uint32_t toIdx);

    /**
     * @brief Add a data dependency edge between tasks.
     *
     * Creates an ordering dependency (like AddDependency) and also marks
     * that data flows from the predecessor to the successor. When the
     * predecessor completes, its output size is added to the successor's
     * input size.
     *
     * Multiple data predecessors accumulate (outputs are summed).
     *
     * @param fromIdx The index of the predecessor task.
     * @param toIdx The index of the successor task.
     */
    void AddDataDependency(uint32_t fromIdx, uint32_t toIdx);

    /**
     * @brief Get indices of tasks that are ready to execute.
     *
     * A task is ready if all its dependencies are completed and it
     * hasn't been completed itself.
     *
     * @return Vector of ready task indices.
     */
    std::vector<uint32_t> GetReadyTasks() const;

    /**
     * @brief Get indices of sink tasks (tasks with no successors).
     *
     * Sink tasks are the final outputs of the DAG - their results should
     * be returned to the client when the DAG completes.
     *
     * @note Well-designed workload DAGs typically have a single sink task
     * that aggregates results from upstream tasks. If your DAG has multiple
     * sinks, consider adding an aggregation task to produce a single output.
     *
     * @return Vector of sink task indices (usually contains one element).
     */
    std::vector<uint32_t> GetSinkTasks() const;

    /**
     * @brief Mark a task as completed.
     *
     * This decrements the in-degree of all successor tasks.
     *
     * @param idx The index of the completed task.
     */
    void MarkCompleted(uint32_t idx);

    /**
     * @brief Get a task by index.
     * @param idx The task index.
     * @return The task, or nullptr if index is invalid.
     */
    Ptr<Task> GetTask(uint32_t idx) const;

    /**
     * @brief Get the index of a task by its task ID.
     * @param taskId The task ID to search for.
     * @return The task index, or -1 if not found.
     */
    int32_t GetTaskIndex(uint64_t taskId) const;

    /**
     * @brief Update a task at the given index.
     *
     * Used to replace original task with response data after execution.
     *
     * @param idx The task index.
     * @param task The new task to set.
     * @return true if successful, false if index is invalid.
     */
    bool SetTask(uint32_t idx, Ptr<Task> task);

    /**
     * @brief Get the number of tasks in the DAG.
     * @return The task count.
     */
    uint32_t GetTaskCount() const;

    /**
     * @brief Check if all tasks are completed.
     * @return true if all tasks are completed.
     */
    bool IsComplete() const;

    /**
     * @brief Validate that the DAG has no cycles.
     *
     * Uses DFS to detect cycles. A DAG with cycles cannot be scheduled.
     *
     * @return true if the DAG is valid (no cycles).
     */
    bool Validate() const;

  protected:
    void DoDispose() override;

  private:
    /**
     * @brief Internal node structure for the DAG.
     */
    struct DagNode
    {
        Ptr<Task> task;                       //!< The task
        std::vector<uint32_t> successors;     //!< Indices of successor tasks (ordering)
        std::vector<uint32_t> dataSuccessors; //!< Indices of data-dependent successors
        uint32_t inDegree{0};                 //!< Count of incomplete predecessors
        bool completed{false};                //!< Whether this task is completed
    };

    std::vector<DagNode> m_nodes; //!< All nodes in the DAG
    uint32_t m_completedCount{0}; //!< Count of completed tasks for O(1) IsComplete()
    std::unordered_map<uint64_t, uint32_t> m_taskIdToIndex; //!< taskId → index for O(1) lookup
};

} // namespace ns3

#endif // DAG_TASK_H
