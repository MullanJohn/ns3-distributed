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
 * @ingroup distributed
 * @brief Manages DVFS scaling for backend accelerators in the orchestrator.
 *
 * DeviceManager is a concrete component of EdgeOrchestrator. It stores the
 * latest DeviceMetrics per backend, evaluates a pluggable ScalingPolicy, and
 * sends ScalingCommandHeader packets back to backends via the orchestrator's
 * worker ConnectionManager.
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
     * @param workerCm The worker connection manager for sending commands.
     */
    void Start(const Cluster& cluster, Ptr<ConnectionManager> workerCm);

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
     * if frequency or voltage changed.
     *
     * @param state The cluster state with per-backend load and metrics.
     */
    void EvaluateScaling(const ClusterState& state);

    /**
     * @brief TracedCallback signature for frequency change events.
     * @param backendIdx The backend index.
     * @param oldFreq The previous frequency in Hz.
     * @param newFreq The new frequency in Hz.
     */
    typedef void (*FrequencyChangedTracedCallback)(uint32_t backendIdx,
                                                   double oldFreq,
                                                   double newFreq);

  protected:
    void DoDispose() override;

  private:
    Ptr<ScalingPolicy> m_scalingPolicy;   //!< Pluggable scaling strategy
    Ptr<DeviceProtocol> m_deviceProtocol; //!< Protocol for metrics/command serialization

    Ptr<ConnectionManager> m_workerConnMgr; //!< Worker connection for sending commands
    Cluster m_cluster;                      //!< Backend cluster reference
    std::vector<double>
        m_commandedFrequency; //!< Expected frequency per backend (reported or commanded)

    TracedCallback<uint32_t, double, double> m_frequencyChangedTrace; //!< Frequency change trace
};

} // namespace ns3

#endif // DEVICE_MANAGER_H
