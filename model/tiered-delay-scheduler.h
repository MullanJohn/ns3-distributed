/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef TIERED_DELAY_SCHEDULER_H
#define TIERED_DELAY_SCHEDULER_H

#include "cluster-scheduler.h"

#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"

#include <map>
#include <string>

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Scheduler that groups backends into commDelay tiers with queue-delay overflow.
 *
 * TieredDelayScheduler groups candidate backends into tiers by their static
 * Cluster::Backend::commDelay (tier 0 = lowest delay, e.g. "edge"; later tiers
 * = higher delay, e.g. "cloud"). It prefers the lowest-delay tier while that
 * tier has spare capacity, and overflows to the next tier once the
 * least-loaded backend in the current tier is saturated. The last tier is
 * always used as a catch-all regardless of its load, so this scheduler never
 * refuses a task that has a structurally compatible backend.
 *
 * Load is read directly from ClusterState::BackendState::Orchestration
 * ::estimatedRemainingComputeWork (the same live, centrally-maintained backlog
 * figure LeastLoadedScheduler uses), converted to a "drain time" -- how long
 * the backend's current queue would take to work through at its effective
 * compute rate. This is a continuous, event-driven signal with no windowing,
 * so it cannot itself oscillate the way a periodic accumulator would. A
 * per-backend hysteresis latch is applied on top: a backend only starts
 * overflowing once its drain time reaches OverflowQueueDelay, and only stops
 * overflowing once it drops back to RecoverQueueDelay, preventing rapid
 * flapping between tiers when load hovers near the threshold. Within a tier,
 * the backend with the lowest drain time is preferred; exact ties are broken
 * by uniform random choice among the tied candidates.
 */
class TieredDelayScheduler : public ClusterScheduler
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    TieredDelayScheduler();
    ~TieredDelayScheduler() override;

    /**
     * @brief Select a backend using tiered commDelay overflow with a utilization threshold.
     *
     * @param task The task to schedule.
     * @param cluster The cluster of backends.
     * @param state Per-backend load and device state.
     * @return Backend index, or -1 if no suitable backend.
     */
    int32_t ScheduleTask(Ptr<Task> task,
                         const Cluster& cluster,
                         const ClusterState& state) override;

    /**
     * @brief Get the scheduler name.
     * @return "TieredDelay"
     */
    std::string GetName() const override;

  protected:
    void DoDispose() override;

  private:
    /**
     * @brief Get a backend's current queue drain time (queued compute / effective rate).
     * @param idx Backend index.
     * @param state Per-backend load and device state.
     * @return Estimated time to work through the backend's currently queued compute.
     */
    Time GetDrainTime(uint32_t idx, const ClusterState& state) const;

    std::map<uint32_t, bool> m_overflowing; //!< backendIdx -> currently latched into overflow

    Ptr<UniformRandomVariable> m_rand; //!< Tie-break among equally-loaded candidates

    Time m_overflowQueueDelay; //!< Drain time at/above which a tier starts overflowing
    Time m_recoverQueueDelay;  //!< Drain time at/below which an overflowing backend recovers
};

} // namespace ns3

#endif // TIERED_DELAY_SCHEDULER_H
