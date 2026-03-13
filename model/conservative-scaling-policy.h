/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef CONSERVATIVE_SCALING_POLICY_H
#define CONSERVATIVE_SCALING_POLICY_H

#include "scaling-policy.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Conservative DVFS scaling policy inspired by Linux conservative governor.
 *
 * This policy steps frequency up or down by one operating point per decision.
 * Frequency and voltage values come from the accelerator's OPP table.
 */
class ConservativeScalingPolicy : public ScalingPolicy
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ConservativeScalingPolicy();
    ~ConservativeScalingPolicy() override;

    Ptr<ScalingDecision> Decide(const ClusterState::BackendState& backend,
                                const std::vector<OperatingPoint>& opps) override;
};

} // namespace ns3

#endif // CONSERVATIVE_SCALING_POLICY_H
