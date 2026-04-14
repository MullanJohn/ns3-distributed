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
 * @ingroup continuum
 * @brief Scheduler that selects the backend with the lowest predicted compute burden.
 *
 * LeastLoadedScheduler picks the backend with the minimum predicted compute
 * burden, as tracked in ClusterState. The score combines the backend's
 * estimated remaining compute work and current effective compute rate.
 * If the task specifies a required accelerator type, only matching backends
 * are considered. Ties are broken deterministically by backend index.
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
     * @brief Select the backend with the lowest predicted compute burden.
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

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // LEAST_LOADED_SCHEDULER_H
