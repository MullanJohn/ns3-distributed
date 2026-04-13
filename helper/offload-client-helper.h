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
#include "ns3/application-helper.h"
#include "ns3/client-submission-policy.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Helper to create and configure OffloadClient applications.
 *
 * This helper simplifies the creation and configuration of OffloadClient
 * applications that offload frames to an Orchestrator via the
 * two-phase admission protocol.
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
     * @brief Set a constant frame rate.
     *
     * Convenience wrapper for SetInterArrival with a ConstantRandomVariable
     * configured to 1/fps seconds.
     *
     * @param fps Frames per second.
     */
    void SetFrameRate(double fps);

    /**
     * @brief Set the inter-arrival distribution between frames.
     *
     * Pass any RandomVariableStream returning seconds (e.g.
     * ExponentialRandomVariable for Poisson arrivals,
     * EmpiricalRandomVariable for trace-driven arrivals).
     *
     * @param rv Inter-arrival random variable.
     */
    void SetInterArrival(Ptr<RandomVariableStream> rv);

    /**
     * @brief Set the mean frame size with optional standard deviation.
     *
     * If stddev > 0, uses a NormalRandomVariable with the given mean and
     * variance (bounded at 3*stddev). Otherwise uses a ConstantRandomVariable.
     *
     * @param mean Mean frame size in bytes.
     * @param stddev Standard deviation in bytes (0 = constant).
     */
    void SetMeanFrameSize(double mean, double stddev = 0);

    /**
     * @brief Set a constant compute demand per frame.
     * @param demand Compute demand in FLOPS.
     */
    void SetComputeDemand(double demand);

    /**
     * @brief Set a constant output size per frame.
     * @param size Output size in bytes.
     */
    void SetOutputSize(double size);

    /**
     * @brief Set the end-to-end delay budget per frame.
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
};

} // namespace ns3

#endif // OFFLOAD_CLIENT_HELPER_H
