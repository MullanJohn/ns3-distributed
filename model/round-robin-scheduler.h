/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ROUND_ROBIN_SCHEDULER_H
#define ROUND_ROBIN_SCHEDULER_H

#include "cluster-scheduler.h"
#include "cluster-state.h"

#include <map>
#include <string>

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Scheduler that dispatches tasks across backends in round-robin order.
 *
 * RoundRobinScheduler keeps a per-accelerator-type index that advances with
 * every scheduled task, distributing workloads evenly across all matching
 * backends in the cluster.
 */
class RoundRobinScheduler : public ClusterScheduler
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    RoundRobinScheduler();
    ~RoundRobinScheduler() override;

    /**
     * @brief Select a backend for the task in round-robin order.
     *
     * Advances the per-accelerator-type index and returns the next matching
     * backend. Returns -1 if the cluster has no backend matching the task's
     * accelerator requirement.
     *
     * @param task The task to schedule.
     * @param cluster The cluster of backends.
     * @param state The current state of the cluster.
     * @return Backend index, or -1 if no suitable backend.
     */
    int32_t ScheduleTask(Ptr<Task> task,
                         const Cluster& cluster,
                         const ClusterState& state) override;

    /**
     * @brief Get the scheduler name.
     * @return "RoundRobin"
     */
    std::string GetName() const override;

  protected:
    void DoDispose() override;

  private:
    std::map<std::string, uint32_t> m_nextIndexByType; //!< Per-type round-robin indices
};

} // namespace ns3

#endif // ROUND_ROBIN_SCHEDULER_H
