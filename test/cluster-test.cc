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
#include "ns3/test.h"

using namespace ns3;

/**
 * @ingroup continuum-tests
 * @brief Test basic Cluster operations.
 */
class ClusterBasicTestCase : public TestCase
{
  public:
    ClusterBasicTestCase();
    void DoRun() override;
};

ClusterBasicTestCase::ClusterBasicTestCase()
    : TestCase("Test basic Cluster operations")
{
}

void
ClusterBasicTestCase::DoRun()
{
    Cluster cluster;
    NS_TEST_ASSERT_MSG_EQ(cluster.GetN(), 0, "Empty cluster should have 0 backends");
    NS_TEST_ASSERT_MSG_EQ(cluster.IsEmpty(), true, "Empty cluster should report IsEmpty()");
    NS_TEST_ASSERT_MSG_EQ((cluster.Begin() == cluster.End()),
                          true,
                          "Empty cluster Begin() should equal End()");

    Ptr<Node> node1 = CreateObject<Node>();
    Ptr<Node> node2 = CreateObject<Node>();
    Ptr<Node> node3 = CreateObject<Node>();

    Address addr1 = InetSocketAddress(Ipv4Address("10.1.1.1"), 9000);
    Address addr2 = InetSocketAddress(Ipv4Address("10.1.2.1"), 9000);
    Address addr3 = InetSocketAddress(Ipv4Address("10.1.3.1"), 9001);

    cluster.AddBackend(node1, addr1);
    NS_TEST_ASSERT_MSG_EQ(cluster.GetN(), 1, "Cluster should have 1 backend after first add");
    NS_TEST_ASSERT_MSG_EQ(cluster.IsEmpty(), false, "Cluster should not be empty after add");

    cluster.AddBackend(node2, addr2);
    cluster.AddBackend(node3, addr3);
    NS_TEST_ASSERT_MSG_EQ(cluster.GetN(), 3, "Cluster should have 3 backends");

    const Cluster::Backend& backend0 = cluster.Get(0);
    NS_TEST_ASSERT_MSG_EQ(backend0.node, node1, "Backend 0 should have node1");

    const Cluster::Backend& backend1 = cluster.Get(1);
    NS_TEST_ASSERT_MSG_EQ(backend1.node, node2, "Backend 1 should have node2");

    const Cluster::Backend& backend2 = cluster.Get(2);
    NS_TEST_ASSERT_MSG_EQ(backend2.node, node3, "Backend 2 should have node3");

    InetSocketAddress inetAddr0 = InetSocketAddress::ConvertFrom(backend0.address);
    NS_TEST_ASSERT_MSG_EQ(inetAddr0.GetPort(), 9000, "Backend 0 should have port 9000");

    InetSocketAddress inetAddr2 = InetSocketAddress::ConvertFrom(backend2.address);
    NS_TEST_ASSERT_MSG_EQ(inetAddr2.GetPort(), 9001, "Backend 2 should have port 9001");

    cluster.Clear();
    NS_TEST_ASSERT_MSG_EQ(cluster.GetN(), 0, "Cluster should be empty after Clear()");
    NS_TEST_ASSERT_MSG_EQ(cluster.IsEmpty(), true, "Cluster should report IsEmpty() after Clear()");
}

/**
 * @ingroup continuum-tests
 * @brief Test Cluster iteration.
 */
class ClusterIterationTestCase : public TestCase
{
  public:
    ClusterIterationTestCase();
    void DoRun() override;
};

ClusterIterationTestCase::ClusterIterationTestCase()
    : TestCase("Test Cluster iteration")
{
}

void
ClusterIterationTestCase::DoRun()
{
    Cluster cluster;

    std::vector<Ptr<Node>> nodes;
    for (uint32_t i = 0; i < 5; i++)
    {
        Ptr<Node> node = CreateObject<Node>();
        nodes.push_back(node);
        Address addr =
            InetSocketAddress(Ipv4Address(("10.1." + std::to_string(i) + ".1").c_str()), 9000 + i);
        cluster.AddBackend(node, addr);
    }

    uint32_t count = 0;
    for (auto it = cluster.Begin(); it != cluster.End(); ++it)
    {
        NS_TEST_ASSERT_MSG_EQ(it->node, nodes[count], "Iterator should return backends in order");
        count++;
    }
    NS_TEST_ASSERT_MSG_EQ(count, 5, "Should iterate over all 5 backends");

    count = 0;
    for (const auto& backend : cluster)
    {
        NS_TEST_ASSERT_MSG_EQ(backend.node,
                              nodes[count],
                              "Range-based for should return backends in order");
        count++;
    }
    NS_TEST_ASSERT_MSG_EQ(count, 5, "Range-based for should iterate over all 5 backends");
}

/**
 * @ingroup continuum-tests
 * @brief Test Cluster's static commDelay/commCapacity backend properties.
 */
class ClusterCommPropertiesTestCase : public TestCase
{
  public:
    ClusterCommPropertiesTestCase();
    void DoRun() override;
};

ClusterCommPropertiesTestCase::ClusterCommPropertiesTestCase()
    : TestCase("Test Cluster commDelay and commCapacity backend properties")
{
}

void
ClusterCommPropertiesTestCase::DoRun()
{
    Cluster cluster;

    Ptr<Node> edgeNode = CreateObject<Node>();
    Ptr<Node> cloudNode = CreateObject<Node>();
    Address edgeAddr = InetSocketAddress(Ipv4Address("10.1.1.1"), 9000);
    Address cloudAddr = InetSocketAddress(Ipv4Address("10.1.2.1"), 9000);

    // No comm properties supplied: defaults are "unset" (0).
    cluster.AddBackend(edgeNode, edgeAddr);
    NS_TEST_ASSERT_MSG_EQ(cluster.Get(0).commDelay,
                          Seconds(0),
                          "commDelay defaults to 0 when unset");
    NS_TEST_ASSERT_MSG_EQ_TOL(cluster.Get(0).commCapacity,
                              0.0,
                              1e-9,
                              "commCapacity defaults to 0 when unset");

    // Explicit comm properties round-trip.
    cluster.AddBackend(cloudNode, cloudAddr, "GPU", MilliSeconds(40), 1.0e9);
    NS_TEST_ASSERT_MSG_EQ(cluster.Get(1).commDelay,
                          MilliSeconds(40),
                          "commDelay round-trips");
    NS_TEST_ASSERT_MSG_EQ_TOL(cluster.Get(1).commCapacity,
                              1.0e9,
                              1e-3,
                              "commCapacity round-trips");
}

namespace ns3
{

TestCase*
CreateClusterBasicTestCase()
{
    return new ClusterBasicTestCase();
}

TestCase*
CreateClusterIterationTestCase()
{
    return new ClusterIterationTestCase();
}

TestCase*
CreateClusterCommPropertiesTestCase()
{
    return new ClusterCommPropertiesTestCase();
}

} // namespace ns3
