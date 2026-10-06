/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ADMISSION_RETRY_POLICY_H
#define ADMISSION_RETRY_POLICY_H

#include "ns3/object.h"

#include <cstdint>
#include <string>

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Abstract base class for reacting to a rejected session admission.
 *
 * AdmissionRetryPolicy is consulted only by OffloadClient's SessionMode, when
 * the one-time session-establishing WORKLOAD_REQUEST is rejected. It decides
 * whether the client should keep attempting to establish a session on
 * subsequent generation ticks, or give up for the rest of the simulation.
 * WorkloadMode never consults this policy: each of its workloads is
 * independently admitted, so a single rejection has no session to abandon.
 */
class AdmissionRetryPolicy : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ~AdmissionRetryPolicy() override = default;

    /**
     * @brief Decide whether to retry session admission after a rejection.
     * @param rejectionCount Number of consecutive session-admission rejections so far.
     * @return true to retry on the next generation tick, false to give up permanently.
     */
    virtual bool ShouldRetry(uint32_t rejectionCount) const = 0;

    /**
     * @brief Get the policy name for logging.
     * @return A string identifying this policy type.
     */
    virtual std::string GetName() const = 0;

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // ADMISSION_RETRY_POLICY_H
