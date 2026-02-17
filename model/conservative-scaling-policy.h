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
 * This policy adjusts frequency by a fixed step size per scaling decision.
 * It also computes target voltage via linear V-F mapping.
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

    Ptr<ScalingDecision> Decide(const ClusterState::BackendState& backend) override;

  private:
    double m_minFrequency;  //!< Lower frequency bound in Hz
    double m_maxFrequency;  //!< Upper frequency bound in Hz
    double m_minVoltage;    //!< Lower voltage bound in V
    double m_maxVoltage;    //!< Upper voltage bound in V
    double m_frequencyStep; //!< Frequency step size per decision in Hz
};

} // namespace ns3

#endif // CONSERVATIVE_SCALING_POLICY_H
