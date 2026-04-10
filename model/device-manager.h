/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "cluster-state.h"
#include "cluster.h"
#include "connection-manager.h"
#include "device-protocol.h"
#include "scaling-policy.h"

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <vector>

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Manages performance scaling for backend accelerators in the orchestrator.
 *
 * DeviceManager is a concrete component of EdgeOrchestrator. It evaluates a
 * pluggable ScalingPolicy using per-backend state from ClusterState, and sends
 * ScalingCommandHeader packets to backends via the orchestrator's worker
 * ConnectionManager.
 *
 * Metrics arrive via HandleMetrics() (called by EdgeOrchestrator when a type-4
 * packet arrives). Scaling is evaluated via EvaluateScaling() (called by
 * EdgeOrchestrator on task events).
 */
class DeviceManager : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    DeviceManager();
    ~DeviceManager() override;

    /**
     * @brief Initialize the device manager with a cluster and connection manager.
     *
     * Must be called before HandleMetrics() or EvaluateScaling().
     *
     * @param cluster The backend cluster.
     * @param backendCm The backend connection manager for sending commands.
     * @param state The cluster state to initialize performance states and
     * backend compute capabilities.
     */
    void Start(const Cluster& cluster, Ptr<ConnectionManager> backendCm, ClusterState& state);

    /**
     * @brief Store metrics received from a backend.
     *
     * Called by EdgeOrchestrator when a type-4 packet arrives.
     *
     * @param packet The metrics packet (DeviceMetricsHeader).
     * @param backendIdx The backend index in the cluster.
     * @param state The cluster state to update with parsed metrics.
     */
    void HandleMetrics(Ptr<Packet> packet, uint32_t backendIdx, ClusterState& state);

    /**
     * @brief Try to consume a device metrics message from a receive buffer.
     *
     * Peeks at the first byte of the buffer. If it is a metrics message and
     * enough data is available, the message is consumed (removed from the
     * buffer), parsed, and stored in ClusterState.
     *
     * @param buffer The receive buffer (modified in-place if consumed).
     * @param from The backend address (used to resolve backend index).
     * @param state The cluster state to update.
     * @return true if a metrics message was consumed, false otherwise.
     */
    bool TryConsumeMetrics(Ptr<Packet> buffer, const Address& from, ClusterState& state);

    /**
     * @brief Evaluate scaling decisions for all backends.
     *
     * Called by EdgeOrchestrator on task events. For each backend,
     * runs ScalingPolicy::Decide() and sends command packets
     * if the performance state changed.
     *
     * @param state The cluster state with per-backend load and metrics.
     */
    void EvaluateScaling(ClusterState& state);

    /**
     * @brief TracedCallback signature for performance state change events.
     * @param backendIdx The backend index.
     * @param oldStateIdx The previous performance state index.
     * @param newStateIdx The new performance state index.
     */
    typedef void (*StateChangedTracedCallback)(uint32_t backendIdx,
                                               uint32_t oldStateIdx,
                                               uint32_t newStateIdx);

  protected:
    void DoDispose() override;

  private:
    Ptr<ScalingPolicy> m_scalingPolicy;   //!< Pluggable scaling strategy
    Ptr<DeviceProtocol> m_deviceProtocol; //!< Protocol for metrics serialization

    Ptr<ConnectionManager> m_backendConnMgr; //!< Backend connection for sending commands
    Cluster m_cluster;                       //!< Backend cluster reference
    std::vector<std::vector<double>>
        m_stateComputeRates; //!< Per-backend compute rate at each performance state

    TracedCallback<uint32_t, uint32_t, uint32_t> m_stateChangedTrace; //!< State change trace
};

} // namespace ns3

#endif // DEVICE_MANAGER_H
