/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef CLIENT_SUBMISSION_POLICY_H
#define CLIENT_SUBMISSION_POLICY_H

#include "ns3/nstime.h"
#include "ns3/object.h"

#include <cstdint>
#include <string>

namespace ns3
{

/**
 * @ingroup continuum
 * @brief Abstract base class for client submission policies.
 *
 * ClientSubmissionPolicy decides whether a client should emit the next
 * submission tick or skip it. The client owns the history; the policy
 * reads a snapshot and returns a verdict. Naming is deliberately
 * client-agnostic ("submission") so the abstraction is reusable by
 * non-OffloadClient clients.
 */
class ClientSubmissionPolicy : public Object
{
  public:
    /**
     * @brief Snapshot of client-side submission history passed to the policy.
     */
    struct ClientState
    {
        uint32_t pendingCount{0};       //!< Submissions currently in flight
        uint64_t submissionCount{0};    //!< Total submission ticks (sent + dropped)
        uint64_t submissionsSent{0};    //!< Submissions successfully offered for admission
        uint64_t submissionsDropped{0}; //!< Submissions skipped at the client
        uint64_t submissionsViolated{0};//!< Completed submissions that exceeded the deadline budget
        uint64_t submissionsRejected{0};//!< Submissions rejected by the orchestrator
        uint64_t lastViolationIndex{0}; //!< Index of the most recent violation (0 = never)
        Time lastViolationTime{Seconds(0)}; //!< Wall-clock of the most recent violation
        Time lastRejectionTime{Seconds(0)}; //!< Wall-clock of the most recent rejection
        Time now{Seconds(0)};               //!< Current simulation time
        double submissionRate{0.0};         //!< Client's submission cadence in Hz
    };

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    ~ClientSubmissionPolicy() override = default;

    /**
     * @brief Decide whether the client should submit the next tick.
     * @param state Client-side history snapshot.
     * @return true to submit, false to skip.
     */
    virtual bool ShouldSubmit(const ClientState& state) = 0;

    /**
     * @brief Get the policy name for logging.
     * @return A string identifying this policy type.
     */
    virtual std::string GetName() const = 0;

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // CLIENT_SUBMISSION_POLICY_H
