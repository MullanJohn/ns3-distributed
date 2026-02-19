/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DEADLINE_AWARE_ADMISSION_POLICY_H
#define DEADLINE_AWARE_ADMISSION_POLICY_H

#include "admission-policy.h"
#include "cluster-state.h"
#include "task.h"

#include "ns3/nstime.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Admission policy that rejects workloads with infeasible deadlines.
 *
 * Estimates whether each task in the DAG can complete before its deadline
 * given current backend load and DAG dependency structure. A task cannot
 * start until all its predecessors complete, so the earliest start time
 * accounts for the critical path through the DAG.
 *
 * Tasks without deadlines are always feasible. If any task cannot meet
 * its deadline on any matching backend, the entire workload is rejected.
 */
class DeadlineAwareAdmissionPolicy : public AdmissionPolicy
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    DeadlineAwareAdmissionPolicy();
    ~DeadlineAwareAdmissionPolicy() override;

    /**
     * @brief Admit if all tasks with deadlines can be met on at least one backend.
     *
     * @param dag The workload DAG
     * @param cluster Current cluster state
     * @param state Per-backend load and device metrics
     * @return true if all deadline-bearing tasks are feasible
     */
    bool ShouldAdmit(Ptr<DagTask> dag, const Cluster& cluster, const ClusterState& state) override;

    /**
     * @brief Get the policy name.
     * @return "DeadlineAware"
     */
    std::string GetName() const override;

  private:
    /**
     * @brief Check if a task can meet its deadline on a given backend.
     *
     * @param task The task to check
     * @param backend The backend state
     * @param earliestStart The earliest time this task can begin (after predecessors complete)
     * @return true if estimated completion time is within deadline
     */
    bool CanMeetDeadline(Ptr<Task> task,
                         const ClusterState::BackendState& backend,
                         Time earliestStart) const;

    double m_computeRate; //!< Assumed backend processing rate in FLOPS
};

} // namespace ns3

#endif // DEADLINE_AWARE_ADMISSION_POLICY_H
