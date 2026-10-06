/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef NETWORK_AWARE_SCHEDULER_H
#define NETWORK_AWARE_SCHEDULER_H

#include "cluster-scheduler.h"

#include <string>

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Scheduler that combines predicted compute burden with per-backend network cost.
 *
 * NetworkAwareScheduler extends LeastLoadedScheduler's compute-completion-time
 * score with a network term: an RTT estimate from the backend's communication
 * delay (doubled one-way commDelay, live-measured value preferred over the
 * static Cluster::Backend value when available) plus a transmission-time
 * estimate derived from the backend's static commCapacity and the task's
 * input/output size. This allows placement decisions to prefer low-latency
 * ("edge") backends over higher-latency ("cloud") backends, while still
 * accounting for load.
 *
 * If no backend has commDelay/commCapacity configured and live network-delay
 * tracking is disabled (the defaults), the network term is always zero and
 * this scheduler ranks backends identically to LeastLoadedScheduler.
 */
class NetworkAwareScheduler : public ClusterScheduler
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    NetworkAwareScheduler();
    ~NetworkAwareScheduler() override;

    /**
     * @brief Select the backend with the lowest combined compute+network score.
     *
     * @param task The task to schedule.
     * @param cluster The cluster of backends.
     * @param state Per-backend load and network state.
     * @return Backend index, or -1 if no suitable backend.
     */
    int32_t ScheduleTask(Ptr<Task> task,
                         const Cluster& cluster,
                         const ClusterState& state) override;

    /**
     * @brief Get the scheduler name.
     * @return "NetworkAware"
     */
    std::string GetName() const override;

  protected:
    void DoDispose() override;

  private:
    double m_networkWeight; //!< Weight applied to the network term (seconds), 0 = LeastLoaded
};

} // namespace ns3

#endif // NETWORK_AWARE_SCHEDULER_H
