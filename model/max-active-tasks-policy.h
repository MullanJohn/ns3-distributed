/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef MAX_ACTIVE_TASKS_POLICY_H
#define MAX_ACTIVE_TASKS_POLICY_H

#include "admission-policy.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Type-aware admission policy that rejects workloads when compatible backends lack
 * capacity.
 *
 * MaxActiveTasksPolicy counts the incoming workload's tasks per required
 * accelerator type and checks whether compatible backends have enough
 * remaining capacity after accounting for active tasks and pending admission
 * reservations. Tasks with no required type are matched against any backend.
 */
class MaxActiveTasksPolicy : public AdmissionPolicy
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    MaxActiveTasksPolicy();
    ~MaxActiveTasksPolicy() override;

    /**
     * @brief Admit if compatible backends have enough capacity for the incoming workload.
     *
     * @param dag The workload DAG (inspected for per-type task counts).
     * @param cluster The cluster (used for type-based backend lookup).
     * @param state Per-backend load, metrics, and pending admission reservations.
     * @return true if capacity exists for all required types, false otherwise.
     */
    bool ShouldAdmit(Ptr<DagTask> dag, const Cluster& cluster, const ClusterState& state) override;

    /**
     * @brief Get type-scoped reservation counts for the incoming workload.
     * @param dag The workload DAG.
     * @return Required task counts grouped by accelerator type.
     */
    std::map<std::string, uint32_t> GetReservationTaskCounts(Ptr<DagTask> dag) const override;

    /**
     * @brief Get the policy name.
     * @return "MaxActiveTasks"
     */
    std::string GetName() const override;

  private:
    uint32_t m_maxActiveTasks; //!< Per-backend active task threshold
};

} // namespace ns3

#endif // MAX_ACTIVE_TASKS_POLICY_H
