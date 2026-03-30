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
 * @brief Type-aware admission policy that rejects workloads when compatible backends are at
 * capacity.
 *
 * MaxActiveTasksPolicy checks whether backends compatible with the workload's
 * required accelerator types have fewer active tasks than the configured
 * threshold. For each required type, at least one matching backend must have
 * capacity. Tasks with no required type are matched against any backend.
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
     * @brief Admit if compatible backends have capacity for each required task type.
     *
     * @param dag The workload DAG (inspected for required accelerator types).
     * @param cluster The cluster (used for type-based backend lookup).
     * @param state Per-backend load and device metrics.
     * @return true if capacity exists for all required types, false otherwise.
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
