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
 * @ingroup distributed
 * @brief Utilization-based DVFS scaling policy inspired by Linux ondemand governor.
 *
 * Simple binary policy:
 * - Busy or queued tasks → maxFrequency (aggressive scale-up for latency)
 * - Idle → minFrequency (energy savings)
 * - Voltage is passed through unchanged.
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

    Ptr<ScalingDecision> Decide(const ClusterState::BackendState& backend,
                                double minFrequency,
                                double maxFrequency) override;
    std::string GetName() const override;
};

} // namespace ns3

#endif // UTILIZATION_SCALING_POLICY_H
