/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef NEVER_RETRY_ADMISSION_POLICY_H
#define NEVER_RETRY_ADMISSION_POLICY_H

#include "admission-retry-policy.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Default retry policy: give up permanently after one rejection.
 *
 * Models a client that treats a rejected session-establishment attempt as
 * final — it stops generating further workloads for the rest of the
 * simulation rather than repeatedly retrying.
 */
class NeverRetryAdmissionPolicy : public AdmissionRetryPolicy
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    NeverRetryAdmissionPolicy();
    ~NeverRetryAdmissionPolicy() override;

    /**
     * @brief Never retry.
     * @return false.
     */
    bool ShouldRetry(uint32_t rejectionCount) const override;

    /**
     * @brief Get the policy name.
     * @return "NeverRetry"
     */
    std::string GetName() const override;
};

} // namespace ns3

#endif // NEVER_RETRY_ADMISSION_POLICY_H
