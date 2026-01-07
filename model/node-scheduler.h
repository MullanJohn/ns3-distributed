/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef NODE_SCHEDULER_H
#define NODE_SCHEDULER_H

#include "cluster.h"
#include "offload-header.h"

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Abstract base class for node scheduling policies.
 *
 * NodeScheduler defines the interface for selecting which backend server
 * in a cluster should handle an incoming task. Subclasses implement different
 * scheduling policies such as round-robin, least-pending, weighted, or
 * metadata-aware scheduling.
 *
 * Example usage:
 * @code
 * Ptr<NodeScheduler> scheduler = CreateObject<RoundRobinScheduler>();
 * scheduler->Initialize(cluster);
 *
 * // For each incoming task:
 * int32_t backendIndex = scheduler->SelectBackend(header, cluster);
 * if (backendIndex >= 0)
 * {
 *     // Forward task to cluster.Get(backendIndex)
 *     scheduler->NotifyTaskSent(backendIndex, header);
 * }
 *
 * // When response arrives:
 * scheduler->NotifyTaskCompleted(backendIndex, taskId, duration);
 * @endcode
 */
class NodeScheduler : public Object
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Default constructor.
     */
    NodeScheduler();

    /**
     * @brief Copy constructor.
     * @param other The NodeScheduler to copy.
     */
    NodeScheduler(const NodeScheduler& other);

    /**
     * @brief Destructor.
     */
    ~NodeScheduler() override;

    /**
     * @brief Get the name of the scheduling algorithm.
     *
     * @return A string identifying the scheduler (e.g., "RoundRobin").
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Initialize the scheduler with a cluster.
     *
     * Called once when the load balancer starts. Allows the scheduler to
     * cache cluster information and set up internal state. Subclasses
     * should call the base implementation.
     *
     * @param cluster The cluster of backend servers.
     */
    virtual void Initialize(const Cluster& cluster);

    /**
     * @brief Select a backend server for the given task.
     *
     * This is the main scheduling decision point. The scheduler examines
     * the task metadata (compute demand, I/O sizes) and selects an
     * appropriate backend from the cluster.
     *
     * @param header The task request header containing task metadata.
     * @param cluster The cluster to select from.
     * @return Index into the cluster (0 to GetN()-1), or -1 if no backend
     *         is available (e.g., all backends down).
     */
    virtual int32_t SelectBackend(const OffloadHeader& header, const Cluster& cluster) = 0;

    /**
     * @brief Notify the scheduler that a task was sent to a backend.
     *
     * Called after a task is successfully forwarded to a backend. Allows
     * stateful schedulers to update internal state (e.g., increment pending
     * task count for the backend).
     *
     * @param backendIndex The index of the backend that was selected.
     * @param header The task header that was sent.
     */
    virtual void NotifyTaskSent(uint32_t backendIndex, const OffloadHeader& header);

    /**
     * @brief Notify the scheduler that a task completed on a backend.
     *
     * Called when a response is received from a backend. Allows stateful
     * schedulers to update internal state (e.g., decrement pending task
     * count, track latency statistics).
     *
     * @param backendIndex The index of the backend that processed the task.
     * @param taskId The ID of the completed task.
     * @param duration The total processing time on the backend.
     */
    virtual void NotifyTaskCompleted(uint32_t backendIndex, uint64_t taskId, Time duration);

    /**
     * @brief Create a copy of this scheduler.
     *
     * Required for ns-3's Object cloning pattern. Implementations should
     * use CopyObject<DerivedClass>(this).
     *
     * @return Pointer to a new NodeScheduler with the same configuration.
     */
    virtual Ptr<NodeScheduler> Fork() = 0;

  protected:
    void DoDispose() override;

    uint32_t m_numBackends; //!< Cached number of backends from Initialize()
};

} // namespace ns3

#endif // NODE_SCHEDULER_H
