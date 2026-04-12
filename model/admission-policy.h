/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ADMISSION_POLICY_H
#define ADMISSION_POLICY_H

#include "cluster.h"

#include "ns3/object.h"
#include "ns3/ptr.h"

#include <cstdint>
#include <map>
#include <string>

namespace ns3
{

class ClusterState;
class DagTask;

/**
 * @ingroup continuum
 * @brief Abstract base class for admission control policies.
 *
 * AdmissionPolicy determines whether a workload should be accepted for execution.
 *
 * Implementations are stateless - the orchestrator tracks active workloads
 * and passes per-backend state to ShouldAdmit(). This follows real-world
 * patterns from Kubernetes and Spark where admission controllers are stateless.
 *
 * Example usage:
 * @code
 * Ptr<AdmissionPolicy> policy = CreateObject<MaxActiveTasksPolicy>();
 *
 * if (policy->ShouldAdmit(dag, cluster, state))
 * {
 *     // Accept workload...
 * }
 * @endcode
 */
class AdmissionPolicy : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    AdmissionPolicy();
    ~AdmissionPolicy() override;

    /**
     * @brief Check if a workload should be admitted.
     *
     * This method evaluates the workload against the current cluster
     * state and policy rules to decide admission.
     *
     * @param dag The workload DAG (single tasks are wrapped as 1-node DAGs)
     * @param cluster Current cluster state with available backends
     * @param state Per-backend load and device metrics
     * @return true if the workload should be admitted, false if rejected
     */
    virtual bool ShouldAdmit(Ptr<DagTask> dag,
                             const Cluster& cluster,
                             const ClusterState& state) = 0;

    /**
     * @brief Get pending-admission reservation counts for this workload.
     *
     * The orchestrator uses this to reserve admission capacity between Phase 1
     * admission and Phase 2 data upload without needing policy-specific logic.
     * The default implementation reserves the whole workload against the
     * generic/unspecified accelerator type.
     *
     * @param dag The workload DAG.
     * @return Required reservation counts grouped by accelerator type.
     */
    virtual std::map<std::string, uint32_t> GetReservationTaskCounts(Ptr<DagTask> dag) const;

    /**
     * @brief Get the policy name for logging and debugging.
     * @return A string identifying this policy type.
     */
    virtual std::string GetName() const = 0;

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // ADMISSION_POLICY_H
