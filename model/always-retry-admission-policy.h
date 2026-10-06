/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ALWAYS_RETRY_ADMISSION_POLICY_H
#define ALWAYS_RETRY_ADMISSION_POLICY_H

#include "admission-retry-policy.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Retry policy that keeps attempting session establishment indefinitely.
 *
 * Retries need no separate timer: OffloadClient's normal generation cadence
 * already re-attempts session establishment on every tick while unadmitted,
 * so this policy only needs to say "keep trying."
 */
class AlwaysRetryAdmissionPolicy : public AdmissionRetryPolicy
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    AlwaysRetryAdmissionPolicy();
    ~AlwaysRetryAdmissionPolicy() override;

    /**
     * @brief Always retry.
     * @return true.
     */
    bool ShouldRetry(uint32_t rejectionCount) const override;

    /**
     * @brief Get the policy name.
     * @return "AlwaysRetry"
     */
    std::string GetName() const override;
};

} // namespace ns3

#endif // ALWAYS_RETRY_ADMISSION_POLICY_H
