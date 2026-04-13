/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DROP_WHILE_PENDING_SUBMISSION_POLICY_H
#define DROP_WHILE_PENDING_SUBMISSION_POLICY_H

#include "client-submission-policy.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Submission policy that skips ticks while any prior submission is in flight.
 *
 * Reproduces the historical OffloadClient drop-if-pending behaviour and is
 * installed as the default so existing setups keep the same output without
 * explicit configuration.
 */
class DropWhilePendingSubmissionPolicy : public ClientSubmissionPolicy
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    DropWhilePendingSubmissionPolicy();
    ~DropWhilePendingSubmissionPolicy() override;

    /**
     * @brief Submit only when no prior submission is pending.
     * @param state Client-side history snapshot.
     * @return true if pendingCount is zero.
     */
    bool ShouldSubmit(const ClientState& state) override;

    /**
     * @brief Get the policy name.
     * @return "DropWhilePending"
     */
    std::string GetName() const override;
};

} // namespace ns3

#endif // DROP_WHILE_PENDING_SUBMISSION_POLICY_H
