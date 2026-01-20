/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/cluster.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/node.h"
#include "ns3/simple-task-header.h"
#include "ns3/round-robin-scheduler.h"
#include "ns3/test.h"

using namespace ns3;

/**
 * @ingroup distributed-tests
 * @brief Test RoundRobinScheduler basic cycling behavior
 */
class RoundRobinSchedulerTestCase : public TestCase
{
  public:
    RoundRobinSchedulerTestCase();
    void DoRun() override;
};

RoundRobinSchedulerTestCase::RoundRobinSchedulerTestCase()
    : TestCase("Test RoundRobinScheduler cycling behavior")
{
}

void
RoundRobinSchedulerTestCase::DoRun()
{
    // Create a cluster with 3 backends
    Cluster cluster;
    for (uint32_t i = 0; i < 3; i++)
    {
        Ptr<Node> node = CreateObject<Node>();
        Address addr = InetSocketAddress(Ipv4Address(("10.1." + std::to_string(i) + ".1").c_str()), 9000);
        cluster.AddBackend(node, addr);
    }

    // Create scheduler
    Ptr<RoundRobinScheduler> scheduler = CreateObject<RoundRobinScheduler>();
    NS_TEST_ASSERT_MSG_EQ(scheduler->GetName(), "RoundRobin", "Scheduler name should be RoundRobin");

    scheduler->Initialize(cluster);

    // Create a dummy header for testing
    SimpleTaskHeader header;
    header.SetMessageType(SimpleTaskHeader::TASK_REQUEST);
    header.SetTaskId(1);
    header.SetComputeDemand(1e9);
    header.SetInputSize(1024);
    header.SetOutputSize(512);

    // Test round-robin cycling
    int32_t selected;

    selected = scheduler->SelectBackend(header, cluster);
    NS_TEST_ASSERT_MSG_EQ(selected, 0, "First selection should be backend 0");

    header.SetTaskId(2);
    selected = scheduler->SelectBackend(header, cluster);
    NS_TEST_ASSERT_MSG_EQ(selected, 1, "Second selection should be backend 1");

    header.SetTaskId(3);
    selected = scheduler->SelectBackend(header, cluster);
    NS_TEST_ASSERT_MSG_EQ(selected, 2, "Third selection should be backend 2");

    header.SetTaskId(4);
    selected = scheduler->SelectBackend(header, cluster);
    NS_TEST_ASSERT_MSG_EQ(selected, 0, "Fourth selection should wrap to backend 0");

    header.SetTaskId(5);
    selected = scheduler->SelectBackend(header, cluster);
    NS_TEST_ASSERT_MSG_EQ(selected, 1, "Fifth selection should be backend 1");
}

/**
 * @ingroup distributed-tests
 * @brief Test RoundRobinScheduler with empty cluster
 */
class RoundRobinEmptyClusterTestCase : public TestCase
{
  public:
    RoundRobinEmptyClusterTestCase();
    void DoRun() override;
};

RoundRobinEmptyClusterTestCase::RoundRobinEmptyClusterTestCase()
    : TestCase("Test RoundRobinScheduler with empty cluster")
{
}

void
RoundRobinEmptyClusterTestCase::DoRun()
{
    Cluster emptyCluster;
    Ptr<RoundRobinScheduler> scheduler = CreateObject<RoundRobinScheduler>();
    scheduler->Initialize(emptyCluster);

    SimpleTaskHeader header;
    header.SetMessageType(SimpleTaskHeader::TASK_REQUEST);
    header.SetTaskId(1);

    int32_t selected = scheduler->SelectBackend(header, emptyCluster);
    NS_TEST_ASSERT_MSG_EQ(selected, -1, "Should return -1 for empty cluster");
}

/**
 * @ingroup distributed-tests
 * @brief Test RoundRobinScheduler with single backend
 */
class RoundRobinSingleBackendTestCase : public TestCase
{
  public:
    RoundRobinSingleBackendTestCase();
    void DoRun() override;
};

RoundRobinSingleBackendTestCase::RoundRobinSingleBackendTestCase()
    : TestCase("Test RoundRobinScheduler with single backend")
{
}

void
RoundRobinSingleBackendTestCase::DoRun()
{
    Cluster cluster;
    Ptr<Node> node = CreateObject<Node>();
    cluster.AddBackend(node, InetSocketAddress(Ipv4Address("10.1.1.1"), 9000));

    Ptr<RoundRobinScheduler> scheduler = CreateObject<RoundRobinScheduler>();
    scheduler->Initialize(cluster);

    SimpleTaskHeader header;
    header.SetMessageType(SimpleTaskHeader::TASK_REQUEST);

    // With single backend, should always return 0
    for (uint32_t i = 0; i < 5; i++)
    {
        header.SetTaskId(i + 1);
        int32_t selected = scheduler->SelectBackend(header, cluster);
        NS_TEST_ASSERT_MSG_EQ(selected, 0, "Single backend should always return 0");
    }
}

/**
 * @ingroup distributed-tests
 * @brief Test RoundRobinScheduler Fork functionality
 */
class RoundRobinForkTestCase : public TestCase
{
  public:
    RoundRobinForkTestCase();
    void DoRun() override;
};

RoundRobinForkTestCase::RoundRobinForkTestCase()
    : TestCase("Test RoundRobinScheduler Fork functionality")
{
}

void
RoundRobinForkTestCase::DoRun()
{
    Cluster cluster;
    for (uint32_t i = 0; i < 3; i++)
    {
        Ptr<Node> node = CreateObject<Node>();
        cluster.AddBackend(node, InetSocketAddress(Ipv4Address(("10.1." + std::to_string(i) + ".1").c_str()), 9000));
    }

    Ptr<RoundRobinScheduler> scheduler = CreateObject<RoundRobinScheduler>();
    scheduler->Initialize(cluster);

    SimpleTaskHeader header;
    header.SetMessageType(SimpleTaskHeader::TASK_REQUEST);
    header.SetTaskId(1);

    // Advance the original scheduler
    scheduler->SelectBackend(header, cluster); // Now at index 1

    // Fork the scheduler
    Ptr<NodeScheduler> forked = scheduler->Fork();
    NS_TEST_ASSERT_MSG_NE(forked, nullptr, "Fork should return a valid scheduler");
    NS_TEST_ASSERT_MSG_EQ(forked->GetName(), "RoundRobin", "Forked scheduler should be RoundRobin");

    // Original should continue from where it was
    header.SetTaskId(2);
    int32_t originalNext = scheduler->SelectBackend(header, cluster);
    NS_TEST_ASSERT_MSG_EQ(originalNext, 1, "Original should continue at backend 1");

    // Forked should also continue from where original was at fork time
    header.SetTaskId(3);
    int32_t forkedNext = forked->SelectBackend(header, cluster);
    NS_TEST_ASSERT_MSG_EQ(forkedNext, 1, "Forked should also be at backend 1");
}

namespace ns3
{

/**
 * @brief Factory function for RoundRobinSchedulerTestCase
 */
TestCase*
CreateRoundRobinSchedulerTestCase()
{
    return new RoundRobinSchedulerTestCase();
}

/**
 * @brief Factory function for RoundRobinEmptyClusterTestCase
 */
TestCase*
CreateRoundRobinEmptyClusterTestCase()
{
    return new RoundRobinEmptyClusterTestCase();
}

/**
 * @brief Factory function for RoundRobinSingleBackendTestCase
 */
TestCase*
CreateRoundRobinSingleBackendTestCase()
{
    return new RoundRobinSingleBackendTestCase();
}

/**
 * @brief Factory function for RoundRobinForkTestCase
 */
TestCase*
CreateRoundRobinForkTestCase()
{
    return new RoundRobinForkTestCase();
}

} // namespace ns3
