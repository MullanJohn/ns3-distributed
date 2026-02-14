/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ALWAYS_ADMIT_POLICY_H
#define ALWAYS_ADMIT_POLICY_H

#include "admission-policy.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Admission policy that always admits workloads.
 *
 * AlwaysAdmitPolicy is a baseline policy that accepts all workloads
 * regardless of cluster state or workload metrics.
 */
class AlwaysAdmitPolicy : public AdmissionPolicy
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    AlwaysAdmitPolicy();
    ~AlwaysAdmitPolicy() override;

    /**
     * @brief Always admits the workload.
     *
     * @param dag The workload DAG (ignored)
     * @param cluster Current cluster state (ignored)
     * @param state Per-backend load and device metrics (ignored)
     * @return Always returns true
     */
    bool ShouldAdmit(Ptr<DagTask> dag, const Cluster& cluster, const ClusterState& state) override;

    /**
     * @brief Get the policy name.
     * @return "AlwaysAdmit"
     */
    std::string GetName() const override;
};

} // namespace ns3

#endif // ALWAYS_ADMIT_POLICY_H
