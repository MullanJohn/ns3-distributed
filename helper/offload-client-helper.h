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

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Helper to create and configure OffloadClient applications.
 *
 * This helper simplifies the creation and configuration of OffloadClient
 * applications for distributed computing simulations.
 */
class OffloadClientHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default OffloadClient type.
     */
    OffloadClientHelper();

    /**
     * @brief Construct helper with target server address.
     * @param serverAddress Address of the offload server.
     */
    explicit OffloadClientHelper(const Address& serverAddress);

    /**
     * @brief Set the remote server address.
     * @param serverAddress Address of the offload server.
     */
    void SetServerAddress(const Address& serverAddress);

    /**
     * @brief Set the mean inter-arrival time between tasks.
     * @param mean Mean inter-arrival time in seconds.
     */
    void SetMeanInterArrival(double mean);

    /**
     * @brief Set the mean compute demand.
     * @param mean Mean compute demand in FLOPS.
     */
    void SetMeanComputeDemand(double mean);

    /**
     * @brief Set the mean input size.
     * @param mean Mean input size in bytes.
     */
    void SetMeanInputSize(double mean);

    /**
     * @brief Set the mean output size.
     * @param mean Mean output size in bytes.
     */
    void SetMeanOutputSize(double mean);

    /**
     * @brief Set maximum number of tasks to send.
     * @param maxTasks Maximum task count (0 = unlimited).
     */
    void SetMaxTasks(uint64_t maxTasks);
};

} // namespace ns3

#endif // OFFLOAD_CLIENT_HELPER_H
