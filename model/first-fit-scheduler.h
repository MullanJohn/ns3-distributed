/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef FIRST_FIT_SCHEDULER_H
#define FIRST_FIT_SCHEDULER_H

#include "cluster-scheduler.h"

#include <map>
#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Simple scheduler that selects the first suitable backend.
 *
 * FirstFitScheduler iterates through backends in round-robin order and selects
 * the first one that matches the task's accelerator requirements.
 *
 * Example with backends [GPU, CPU, GPU] and task requiring "GPU":
 * - First task: Backend 0 (GPU matches)
 * - Second task: Backend 2 (GPU matches, round-robin skips CPU)
 * - Third task: Backend 0 (wraps around)
 */
class FirstFitScheduler : public ClusterScheduler
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    FirstFitScheduler();
    ~FirstFitScheduler() override;

    /**
     * @brief Select a backend for the task using first-fit with round-robin.
     *
     * Starts from the last selected index and finds the first backend that
     * matches the task's accelerator requirement. Returns -1 if no match found.
     *
     * @param task The task to schedule.
     * @param cluster The cluster of backends.
     * @return Backend index, or -1 if no suitable backend.
     */
    int32_t ScheduleTask(Ptr<Task> task, const Cluster& cluster) override;

    /**
     * @brief Get the scheduler name.
     * @return "FirstFit"
     */
    std::string GetName() const override;

  protected:
    void DoDispose() override;

  private:
    std::map<std::string, uint32_t> m_nextIndexByType; //!< Per-type round-robin indices
};

} // namespace ns3

#endif // FIRST_FIT_SCHEDULER_H
