/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef OFFLOAD_CLIENT_HELPER_H
#define OFFLOAD_CLIENT_HELPER_H

#include "ns3/address.h"
#include "ns3/admission-retry-policy.h"
#include "ns3/application-helper.h"
#include "ns3/client-submission-policy.h"
#include "ns3/dag-task.h"
#include "ns3/nstime.h"
#include "ns3/offload-client.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Helper to create and configure OffloadClient applications.
 *
 * This helper simplifies the creation and configuration of OffloadClient
 * applications that offload tasks to an Orchestrator, in either SessionMode
 * (default: admission established once, workloads implicitly admitted
 * thereafter) or WorkloadMode (every workload individually admitted via a
 * combined request+data message).
 */
class OffloadClientHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default OffloadClient type.
     */
    OffloadClientHelper();

    /**
     * @brief Construct helper with target orchestrator address.
     * @param orchestratorAddress Address of the Orchestrator.
     */
    explicit OffloadClientHelper(const Address& orchestratorAddress);

    /**
     * @brief Set the remote orchestrator address.
     * @param addr Address of the Orchestrator.
     */
    void SetOrchestratorAddress(const Address& addr);

    /**
     * @brief Set a constant task rate.
     *
     * Convenience wrapper for SetInterArrival with a ConstantRandomVariable
     * configured to 1/rate seconds.
     *
     * @param rate Tasks per second.
     */
    void SetTaskRate(double rate);

    /**
     * @brief Set the inter-arrival distribution between tasks.
     *
     * Pass any RandomVariableStream returning seconds (e.g.
     * ExponentialRandomVariable for Poisson arrivals,
     * EmpiricalRandomVariable for trace-driven arrivals).
     *
     * @param rv Inter-arrival random variable.
     */
    void SetInterArrival(Ptr<RandomVariableStream> rv);

    /**
     * @brief Set the end-to-end delay budget per task.
     *
     * When zero (the default), the budget is derived from the inter-arrival
     * period sampled at startup.
     *
     * @param budget Delay budget.
     */
    void SetDeadlineBudget(Time budget);

    /**
     * @brief Set the estimated round-trip communication delay.
     *
     * This is subtracted from the deadline budget to compute the
     * compute-only deadline set on each task.
     *
     * @param budget Communication budget (T_ul + T_dl estimate).
     */
    void SetCommunicationBudget(Time budget);

    /**
     * @brief Set the client submission policy.
     *
     * When unset, the client installs a DropWhilePendingSubmissionPolicy at
     * startup, matching the historical drop-if-pending behaviour.
     *
     * @param policy The submission policy to install on the client.
     */
    void SetSubmissionPolicy(Ptr<ClientSubmissionPolicy> policy);

    /**
     * @brief Set whether workloads are admitted once per session or individually.
     * @param mode SessionMode (default) or WorkloadMode.
     */
    void SetAdmissionMode(OffloadClient::AdmissionMode mode);

    /**
     * @brief Set the SessionMode retry-after-rejection policy.
     *
     * Only consulted in SessionMode, when the one-time session-establishing
     * request is rejected. When unset, the client installs a
     * NeverRetryAdmissionPolicy at startup (give up permanently on the first
     * rejection). Ignored in WorkloadMode.
     *
     * @param policy The admission retry policy to install on the client.
     */
    void SetAdmissionRetryPolicy(Ptr<AdmissionRetryPolicy> policy);

    /**
     * @brief Set the template DAG (structure and per-task distributions)
     * that each generated workload is instantiated from.
     *
     * When unset, the client installs a single-SimpleTask template at
     * startup (InputSize=OutputSize=1, ComputeDemand=1.0, no edges),
     * matching historical single-task-per-DAG behaviour.
     *
     * @param dagTemplate The template DAG to install on the client.
     */
    void SetDagTemplate(Ptr<DagTask> dagTemplate);

    /**
     * @brief Build and set a linear-chain template DAG of numTasks SimpleTasks.
     *
     * Task 0 draws its input size from inputSizeDist; tasks 1..numTasks-1
     * are left at InputSize=0 with no input distribution, since their real
     * input is entirely relayed from the preceding task via a data
     * dependency (AddDataDependency accumulates upstream output into
     * downstream input at completion time -- giving a downstream task its
     * own independent input distribution would double-count). Every task
     * draws its compute demand and output size from computeDemandDist and
     * outputSizeDist respectively. numTasks=1 yields the single-task case
     * with no edges.
     *
     * @param numTasks Number of tasks in the chain (>= 1).
     * @param inputSizeDist Input size distribution for task 0, in bytes.
     * @param computeDemandDist Compute demand distribution for every task, in FLOPS.
     * @param outputSizeDist Output size distribution for every task, in bytes.
     */
    void SetLinearChainDagTemplate(uint32_t numTasks,
                                   Ptr<RandomVariableStream> inputSizeDist,
                                   Ptr<RandomVariableStream> computeDemandDist,
                                   Ptr<RandomVariableStream> outputSizeDist);

    /**
     * @brief Set the drain window on stop.
     *
     * On stop, generation halts immediately regardless of this value; a
     * positive GracePeriod keeps the connection open (closing early if the
     * last pending workload resolves first) so an in-flight response still
     * has a chance to arrive, instead of being dropped by an immediate
     * close. Default 0 closes the connection immediately, matching
     * historical behaviour.
     *
     * @param gracePeriod How long to keep draining after stop.
     */
    void SetGracePeriod(Time gracePeriod);
};

} // namespace ns3

#endif // OFFLOAD_CLIENT_HELPER_H
