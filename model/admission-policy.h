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

#include "ns3/nstime.h"
#include "ns3/object.h"

#include <cstdint>
#include <set>
#include <string>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Abstract base class for admission control policies.
 *
 * AdmissionPolicy determines whether a workload should be accepted for execution.
 * It is designed to be decoupled from the orchestrator to allow future extraction
 * to a separate CloudController service.
 *
 * Implementations are stateless - the orchestrator tracks active workloads
 * and passes the count to ShouldAdmit(). This follows real-world patterns
 * from Kubernetes and Spark where admission controllers are stateless.
 *
 * Example usage:
 * @code
 * Ptr<AdmissionPolicy> policy = CreateObject<CapacityAdmissionPolicy>();
 * policy->SetAttribute("MaxConcurrent", UintegerValue(10));
 *
 * AdmissionPolicy::WorkloadMetrics metrics;
 * metrics.taskCount = 5;
 * metrics.totalComputeDemand = 1e12;
 * metrics.requiredAccelerators.insert("GPU");
 *
 * uint32_t activeCount = m_activeWorkloads.size();
 * if (policy->ShouldAdmit(metrics, cluster, activeCount))
 * {
 *     // Accept workload...
 * }
 * else
 * {
 *     // Reject workload...
 * }
 * @endcode
 */
class AdmissionPolicy : public Object
{
  public:
    /**
     * @brief Metrics describing a workload for admission control decisions.
     *
     * WorkloadMetrics contains lightweight metadata about a workload (task or DAG)
     * without the actual data payload. This enables efficient admission checks
     * before transferring large input data.
     */
    struct WorkloadMetrics
    {
        uint32_t taskCount{1};           //!< 1 for single task, N for DAG
        double totalComputeDemand{0.0};  //!< Sum of compute demand across all tasks (FLOPS)
        uint64_t totalInputSize{0};      //!< Sum of input sizes across all tasks (bytes)
        Time earliestDeadline{Time(-1)}; //!< Tightest deadline (-1 = no deadline)
        std::set<std::string>
            requiredAccelerators; //!< Required accelerator types (e.g., {"GPU", "TPU"})
    };

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
     * This method evaluates the workload metrics against the current cluster
     * state and policy rules to decide admission. The policy is stateless -
     * the orchestrator tracks active workloads and passes the count here.
     *
     * @param metrics Workload metadata (no actual data/payload)
     * @param cluster Current cluster state with available backends
     * @param activeWorkloadCount Number of workloads currently executing
     * @return true if the workload should be admitted, false if rejected
     */
    virtual bool ShouldAdmit(const WorkloadMetrics& metrics,
                             const Cluster& cluster,
                             uint32_t activeWorkloadCount) = 0;

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
