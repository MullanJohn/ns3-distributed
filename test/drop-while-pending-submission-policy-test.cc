/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/drop-while-pending-submission-policy.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
 * @brief Test DropWhilePendingSubmissionPolicy admits only when no submission is pending.
 */
class DropWhilePendingSubmissionPolicyTestCase : public TestCase
{
  public:
    DropWhilePendingSubmissionPolicyTestCase()
        : TestCase("DropWhilePendingSubmissionPolicy admits only when no submission is pending")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<DropWhilePendingSubmissionPolicy> policy =
            CreateObject<DropWhilePendingSubmissionPolicy>();

        NS_TEST_ASSERT_MSG_EQ(policy->GetName(),
                              "DropWhilePending",
                              "Name should be DropWhilePending");

        ClientSubmissionPolicy::ClientState state;
        state.pendingCount = 0;
        NS_TEST_ASSERT_MSG_EQ(policy->ShouldSubmit(state),
                              true,
                              "Should submit when no prior submission is pending");

        state.pendingCount = 1;
        NS_TEST_ASSERT_MSG_EQ(policy->ShouldSubmit(state),
                              false,
                              "Should skip while a prior submission is pending");

        state.pendingCount = 5;
        NS_TEST_ASSERT_MSG_EQ(policy->ShouldSubmit(state),
                              false,
                              "Should skip while multiple submissions are pending");
    }
};

} // namespace

TestCase*
CreateDropWhilePendingSubmissionPolicyTestCase()
{
    return new DropWhilePendingSubmissionPolicyTestCase;
}

} // namespace ns3
