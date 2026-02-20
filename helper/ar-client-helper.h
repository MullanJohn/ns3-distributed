/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef AR_CLIENT_HELPER_H
#define AR_CLIENT_HELPER_H

#include "ns3/address.h"
#include "ns3/application-helper.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Helper to create and configure ArClient applications.
 *
 * This helper simplifies the creation and configuration of ArClient
 * applications that offload AR frames to an EdgeOrchestrator via the
 * two-phase admission protocol.
 */
class ArClientHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default ArClient type.
     */
    ArClientHelper();

    /**
     * @brief Construct helper with target orchestrator address.
     * @param orchestratorAddress Address of the EdgeOrchestrator.
     */
    explicit ArClientHelper(const Address& orchestratorAddress);

    /**
     * @brief Set the remote orchestrator address.
     * @param addr Address of the EdgeOrchestrator.
     */
    void SetOrchestratorAddress(const Address& addr);

    /**
     * @brief Set the frame rate.
     * @param fps Frames per second.
     */
    void SetFrameRate(double fps);

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
};

} // namespace ns3

#endif // AR_CLIENT_HELPER_H
