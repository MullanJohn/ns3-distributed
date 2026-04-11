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
#include <map>
#include <string>
#include <vector>

namespace ns3
{

class DeviceMetrics;

/**
 * @ingroup continuum
 * @brief Centralized view of per-backend load and device metrics for decision-makers.
 *
 * ClusterState is a plain data container owned by Orchestrator. It aggregates
 * orchestrator-tracked dispatch/completion counts, derived backend capability,
 * and device-reported metrics into a single object that is passed to
 * ScalingPolicy, ClusterScheduler, and AdmissionPolicy on each call.
 */
class ClusterState
{
  public:
    /**
     * @brief Per-backend state split by write authority.
     *
     * Device fields are written exclusively by DeviceManager.
     * Orchestration fields are written exclusively by Orchestrator.
     */
    struct BackendState
    {
        struct Device
        {
            double nominalComputeRate{0.0};     //!< Compute rate at base state (index 0) in FLOP/s
            uint32_t numPerformanceStates{0};   //!< Available performance states (0 = no scaling)
            double maxComputeRate{0.0};         //!< Compute rate at highest state in FLOP/s
            uint32_t currentStateIdx{0};        //!< Current performance state index
            double effectiveComputeRate{0.0};   //!< Current compute rate in FLOP/s
            Ptr<DeviceMetrics> deviceMetrics;   //!< Latest device-reported metrics (nullable)
        };

        struct Orchestration
        {
            uint32_t activeTasks{0};                 //!< Dispatched but not yet completed
            double estimatedRemainingComputeWork{0}; //!< Estimated queued compute work in FLOPs
        };

        Device device;
        Orchestration orchestration;
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
     * @param computeDemand Estimated compute work added to the backend in FLOPs.
     */
    void NotifyTaskDispatched(uint32_t backendIdx, double computeDemand = 0.0);

    /**
     * @brief Roll back a dispatch notification (e.g., on send failure).
     * @param backendIdx The backend index.
     * @param computeDemand Estimated compute work removed from the backend in FLOPs.
     */
    void UndoTaskDispatched(uint32_t backendIdx, double computeDemand = 0.0);

    /**
     * @brief Record that a task completed on a backend.
     * @param backendIdx The backend index.
     * @param computeDemand Estimated compute work removed from the backend in FLOPs.
     */
    void NotifyTaskCompleted(uint32_t backendIdx, double computeDemand = 0.0);

    /**
     * @brief Initialize backend capability information used by schedulers.
     * @param backendIdx The backend index.
     * @param nominalComputeRate Compute rate at base state (index 0) in FLOP/s.
     */
    void SetBackendCapability(uint32_t backendIdx, double nominalComputeRate);

    /**
     * @brief Store device metrics for a backend.
     * @param backendIdx The backend index.
     * @param metrics The device metrics.
     */
    void SetDeviceMetrics(uint32_t backendIdx, Ptr<DeviceMetrics> metrics);

    /**
     * @brief Set the current performance state for a backend.
     * @param backendIdx The backend index.
     * @param stateIdx Performance state index.
     * @param effectiveComputeRate Compute rate at this state in FLOP/s.
     */
    void SetCurrentState(uint32_t backendIdx, uint32_t stateIdx, double effectiveComputeRate);

    /**
     * @brief Set the performance state range for a backend.
     * @param backendIdx The backend index.
     * @param numStates Number of available performance states.
     * @param maxComputeRate Compute rate at the highest state in FLOP/s.
     */
    void SetPerformanceStates(uint32_t backendIdx, uint32_t numStates, double maxComputeRate);

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
     * @brief Reserve capacity for tasks pending admission (not yet dispatched).
     * @param count Number of tasks to reserve.
     * @param acceleratorType Required accelerator type for the reservation. Empty = any backend.
     */
    void ReserveTasks(uint32_t count, const std::string& acceleratorType = "");

    /**
     * @brief Release previously reserved task capacity.
     * @param count Number of tasks to release.
     * @param acceleratorType Required accelerator type for the reservation. Empty = any backend.
     */
    void ReleaseTasks(uint32_t count, const std::string& acceleratorType = "");

    /**
     * @brief Get the total number of tasks reserved by pending admissions.
     * @return Total pending admission task count across all accelerator types.
     */
    uint32_t GetPendingAdmissionTasks() const;

    /**
     * @brief Get the number of tasks reserved for a specific accelerator type.
     * @param acceleratorType Required accelerator type. Empty = any backend.
     * @return Pending admission task count for the given type.
     */
    uint32_t GetPendingAdmissionTasksForType(const std::string& acceleratorType) const;

    /**
     * @brief Clear all state.
     */
    void Clear();

  private:
    std::vector<BackendState> m_backends; //!< Per-backend state
    uint32_t m_activeWorkloads{0};        //!< Number of active workloads
    uint32_t m_pendingAdmissionTasks{0};  //!< Total tasks admitted but not yet dispatched
    std::map<std::string, uint32_t>
        m_pendingAdmissionTasksByType; //!< Pending admissions grouped by required accelerator type
};

} // namespace ns3

#endif // CLUSTER_STATE_H
