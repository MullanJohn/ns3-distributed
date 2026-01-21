/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ROUND_ROBIN_SCHEDULER_H
#define ROUND_ROBIN_SCHEDULER_H

#include "node-scheduler.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Round-robin node scheduling policy.
 *
 * RoundRobinScheduler distributes tasks evenly across backends by cycling
 * through them in order, regardless of task characteristics or backend load.
 * This is a simple, stateless scheduling algorithm that provides fair
 * distribution when all backends have similar capacity.
 *
 * The scheduler maintains an index that increments with each selection,
 * wrapping around to 0 when it reaches the number of backends.
 *
 * Example behavior with 3 backends:
 * - Task 1: Backend 0
 * - Task 2: Backend 1
 * - Task 3: Backend 2
 * - Task 4: Backend 0 (wraps around)
 * - Task 5: Backend 1
 * - ...
 */
class RoundRobinScheduler : public NodeScheduler
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
    RoundRobinScheduler();

    /**
     * @brief Copy constructor.
     * @param other The RoundRobinScheduler to copy.
     */
    RoundRobinScheduler(const RoundRobinScheduler& other);

    /**
     * @brief Destructor.
     */
    ~RoundRobinScheduler() override;

    // Inherited from NodeScheduler
    std::string GetName() const override;
    void Initialize(const Cluster& cluster) override;
    int32_t SelectBackend(const TaskHeader& header, const Cluster& cluster) override;
    Ptr<NodeScheduler> Fork() override;

  protected:
    void DoDispose() override;

  private:
    uint32_t m_nextIndex; //!< Index of the next backend to select
};

} // namespace ns3

#endif // ROUND_ROBIN_SCHEDULER_H
