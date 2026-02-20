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
 * @ingroup distributed
 * @brief Admission policy that rejects workloads when all backends are at capacity.
 *
 * MaxActiveTasksPolicy checks whether any backend has fewer active tasks
 * than the configured threshold. If at least one backend has capacity,
 * the workload is admitted; if all backends are at or above the threshold,
 * it is rejected.
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
     * @brief Admit if any backend has fewer active tasks than the threshold.
     *
     * @param dag The workload DAG (ignored).
     * @param cluster Current cluster state (ignored).
     * @param state Per-backend load and device metrics.
     * @return true if any backend has capacity, false if all are at/above threshold.
     */
    bool ShouldAdmit(Ptr<DagTask> dag, const Cluster& cluster, const ClusterState& state) override;

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
