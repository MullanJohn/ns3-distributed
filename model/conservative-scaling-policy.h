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
 * @ingroup continuum
 * @brief Work-proportional DVFS policy with conservative one-OPP stepping.
 *
 * Computes a target OPP proportional to the estimated drain time of pending
 * compute work (remaining FLOPS / nominal compute rate), then steps one OPP
 * at a time toward that target. The TargetDrainTime attribute sets the drain
 * time that maps to the highest OPP. Handles heterogeneous workloads
 * naturally since heavy tasks contribute more FLOPS than lightweight ones.
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

    Ptr<ScalingDecision> Decide(uint32_t backendIdx,
                                const ClusterState::BackendState& backend) override;
    std::string GetName() const override;

  private:
    double m_targetDrainTime{1.0}; //!< Drain time at nominal rate that maps to highest OPP (seconds)
};

} // namespace ns3

#endif // CONSERVATIVE_SCALING_POLICY_H
