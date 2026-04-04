/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef SCALING_POLICY_H
#define SCALING_POLICY_H

#include "cluster-state.h"

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"

#include <cstdint>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Backend telemetry reported from accelerator to orchestrator.
 *
 * Contains only metrics the orchestrator cannot infer from its own
 * bookkeeping.
 */
class DeviceMetrics : public SimpleRefCount<DeviceMetrics>
{
  public:
    virtual ~DeviceMetrics() = default;

    double utilization{0};   //!< Rolling compute utilization [0.0, 1.0]
    double currentPower{0};  //!< Current total power draw in Watts
};

/**
 * @ingroup distributed
 * @brief Scaling command produced by a ScalingPolicy.
 *
 * Carries the target performance state index for a backend accelerator.
 * Serialized into a ScalingCommandHeader by DeviceProtocol and sent
 * to the backend.
 */
class ScalingDecision : public SimpleRefCount<ScalingDecision>
{
  public:
    virtual ~ScalingDecision() = default;

    uint32_t targetStateIdx{0}; //!< Target performance state index
};

/**
 * @ingroup distributed
 * @brief Abstract base class for scaling policies.
 *
 * ScalingPolicy decides the target performance state for a backend
 * accelerator based on per-backend state.
 */
class ScalingPolicy : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ~ScalingPolicy() override;

    /**
     * @brief Decide the target performance state for a backend.
     * @param backendIdx The backend index in the cluster.
     * @param backend The backend's current state.
     * @return A scaling decision, or nullptr if no change is needed.
     */
    virtual Ptr<ScalingDecision> Decide(uint32_t backendIdx,
                                        const ClusterState::BackendState& backend) = 0;

    /**
     * @brief Get the policy name for logging.
     * @return A string identifying this policy type.
     */
    virtual std::string GetName() const = 0;
};

} // namespace ns3

#endif // SCALING_POLICY_H
