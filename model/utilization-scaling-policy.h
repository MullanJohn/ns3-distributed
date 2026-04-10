/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef UTILIZATION_SCALING_POLICY_H
#define UTILIZATION_SCALING_POLICY_H

#include "scaling-policy.h"

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Utilization-based DVFS scaling policy inspired by Linux ondemand governor.
 *
 * Simple binary policy using the accelerator's OPP table:
 * - Busy or queued tasks -> highest OPP (aggressive scale-up for latency)
 * - Idle -> lowest OPP (energy savings)
 */
class UtilizationScalingPolicy : public ScalingPolicy
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    UtilizationScalingPolicy();
    ~UtilizationScalingPolicy() override;

    Ptr<ScalingDecision> Decide(uint32_t backendIdx,
                                const ClusterState::BackendState& backend) override;
    std::string GetName() const override;
};

} // namespace ns3

#endif // UTILIZATION_SCALING_POLICY_H
