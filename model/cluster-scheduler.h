/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef CLUSTER_SCHEDULER_H
#define CLUSTER_SCHEDULER_H

#include "cluster.h"
#include "task.h"

#include "ns3/object.h"
#include "ns3/ptr.h"

namespace ns3
{

class ClusterState;

/**
 * @ingroup distributed
 * @brief Abstract base class for task scheduling policies.
 *
 * ClusterScheduler determines which backend in a cluster should execute a given task.
 *
 * ClusterScheduler is used by EdgeOrchestrator for task placement decisions during
 * DAG execution.
 *
 * Example usage:
 * @code
 * Ptr<ClusterScheduler> scheduler = CreateObject<FirstFitScheduler>();
 *
 * Ptr<Task> task = CreateObject<SimpleTask>();
 * task->SetRequiredAcceleratorType("GPU");
 *
 * int32_t backendIdx = scheduler->ScheduleTask(task, cluster);
 * if (backendIdx >= 0)
 * {
 *     // Dispatch task to cluster.Get(backendIdx)
 * }
 * @endcode
 */
class ClusterScheduler : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ClusterScheduler();
    ~ClusterScheduler() override;

    /**
     * @brief Select a backend to execute the given task.
     *
     * Examines the task's requirements (accelerator type, compute demand, etc.)
     * and cluster state to select an appropriate backend.
     *
     * @param task The task to schedule.
     * @param cluster The cluster of available backends.
     * @param state Per-backend load and device metrics.
     * @return Index into cluster (0 to GetN()-1), or -1 if no suitable backend found.
     */
    virtual int32_t ScheduleTask(Ptr<Task> task,
                                 const Cluster& cluster,
                                 const ClusterState& state) = 0;

    /**
     * @brief Notify scheduler that a task completed on a backend.
     *
     * Called when a task finishes execution. Stateful schedulers can use this
     * to update internal state (e.g., decrement pending count, track latency).
     * Default implementation does nothing.
     *
     * @param backendIdx The backend index where the task completed.
     * @param task The task that completed.
     */
    virtual void NotifyTaskCompleted(uint32_t backendIdx, Ptr<Task> task);

    /**
     * @brief Get the scheduler name for logging and debugging.
     * @return A string identifying this scheduler type.
     */
    virtual std::string GetName() const = 0;

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // CLUSTER_SCHEDULER_H
