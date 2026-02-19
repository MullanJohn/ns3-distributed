/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef LEAST_LOADED_SCHEDULER_H
#define LEAST_LOADED_SCHEDULER_H

#include "cluster-scheduler.h"

#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Scheduler that selects the backend with the fewest active tasks.
 *
 * LeastLoadedScheduler picks the backend with the minimum number of active
 * tasks (dispatched but not yet completed), as tracked in ClusterState.
 * If the task specifies a required accelerator type, only matching backends
 * are considered. Ties are broken by lowest index.
 */
class LeastLoadedScheduler : public ClusterScheduler
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    LeastLoadedScheduler();
    ~LeastLoadedScheduler() override;

    /**
     * @brief Select the least-loaded backend for the task.
     *
     * @param task The task to schedule.
     * @param cluster The cluster of backends.
     * @param state Per-backend load state.
     * @return Backend index, or -1 if no suitable backend.
     */
    int32_t ScheduleTask(Ptr<Task> task,
                         const Cluster& cluster,
                         const ClusterState& state) override;

    /**
     * @brief Get the scheduler name.
     * @return "LeastLoaded"
     */
    std::string GetName() const override;
};

} // namespace ns3

#endif // LEAST_LOADED_SCHEDULER_H
