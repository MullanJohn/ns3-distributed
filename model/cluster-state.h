/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef CLUSTER_STATE_H
#define CLUSTER_STATE_H

#include "ns3/ptr.h"

#include <cstdint>
#include <vector>

namespace ns3
{

class DeviceMetrics;

/**
 * @ingroup distributed
 * @brief Centralized view of per-backend load and device metrics for decision-makers.
 *
 * ClusterState is a plain data container owned by
 * EdgeOrchestrator. It aggregates orchestrator-tracked dispatch/completion
 * counts and device-reported metrics into a single object that is passed
 * to ScalingPolicy, ClusterScheduler, and AdmissionPolicy on each call.
 */
class ClusterState
{
  public:
    /**
     * @brief Per-backend state combining orchestrator-tracked load and device metrics.
     */
    struct BackendState
    {
        uint32_t activeTasks{0};          //!< Dispatched but not yet completed
        uint32_t totalDispatched{0};      //!< Lifetime dispatch count
        uint32_t totalCompleted{0};       //!< Lifetime completion count
        Ptr<DeviceMetrics> deviceMetrics; //!< Latest device-reported metrics (nullable)
    };

    /**
     * @brief Resize the backend state vector.
     * @param n Number of backends.
     */
    void Resize(uint32_t n);

    /**
     * @brief Get the number of backends.
     * @return Number of backends.
     */
    uint32_t GetN() const;

    /**
     * @brief Get the state of a specific backend.
     * @param idx Backend index.
     * @return Const reference to the backend state.
     */
    const BackendState& Get(uint32_t idx) const;

    /**
     * @brief Record that a task was dispatched to a backend.
     * @param backendIdx The backend index.
     */
    void NotifyTaskDispatched(uint32_t backendIdx);

    /**
     * @brief Record that a task completed on a backend.
     * @param backendIdx The backend index.
     */
    void NotifyTaskCompleted(uint32_t backendIdx);

    /**
     * @brief Store device metrics for a backend.
     * @param backendIdx The backend index.
     * @param metrics The device metrics.
     */
    void SetDeviceMetrics(uint32_t backendIdx, Ptr<DeviceMetrics> metrics);

    /**
     * @brief Set the active workload count.
     * @param count Number of active workloads.
     */
    void SetActiveWorkloadCount(uint32_t count);

    /**
     * @brief Get the active workload count.
     * @return Number of active workloads.
     */
    uint32_t GetActiveWorkloadCount() const;

    /**
     * @brief Get the total number of active tasks across all backends.
     * @return Sum of activeTasks for all backends.
     */
    uint32_t GetTotalActiveTasks() const;

    /**
     * @brief Clear all state.
     */
    void Clear();

  private:
    std::vector<BackendState> m_backends; //!< Per-backend state
    uint32_t m_activeWorkloads{0};        //!< Number of active workloads
};

} // namespace ns3

#endif // CLUSTER_STATE_H
