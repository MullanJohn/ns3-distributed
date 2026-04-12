/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ALWAYS_SUBMIT_POLICY_H
#define ALWAYS_SUBMIT_POLICY_H

#include "client-submission-policy.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Baseline submission policy that never skips a tick.
 *
 * Models the no-adaptation case: the client offers every scheduled
 * submission regardless of in-flight work, letting the orchestrator and
 * transport absorb the load.
 */
class AlwaysSubmitPolicy : public ClientSubmissionPolicy
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    AlwaysSubmitPolicy();
    ~AlwaysSubmitPolicy() override;

    /**
     * @brief Always submit.
     * @return true.
     */
    bool ShouldSubmit(const ClientState& state) override;

    /**
     * @brief Get the policy name.
     * @return "AlwaysSubmit"
     */
    std::string GetName() const override;
};

} // namespace ns3

#endif // ALWAYS_SUBMIT_POLICY_H
