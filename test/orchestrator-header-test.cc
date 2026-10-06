/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/orchestrator-header.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

namespace ns3
{
namespace
{

/**
 * @ingroup continuum-tests
 * @brief Test OrchestratorHeader serialization round-trip for WORKLOAD_REQUEST.
 */
class OrchestratorHeaderRequestTestCase : public TestCase
{
  public:
    OrchestratorHeaderRequestTestCase()
        : TestCase("Test OrchestratorHeader WORKLOAD_REQUEST roundtrip")
    {
    }

  private:
    void DoRun() override
    {
        OrchestratorHeader original;
        original.SetMessageType(OrchestratorHeader::WORKLOAD_REQUEST);
        original.SetDagId(42);
        original.SetPayloadSize(1024);

        NS_TEST_ASSERT_MSG_EQ(original.GetSerializedSize(),
                              OrchestratorHeader::SERIALIZED_SIZE,
                              "Serialized size should be 18 bytes");

        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        OrchestratorHeader deserialized;
        packet->RemoveHeader(deserialized);

        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              OrchestratorHeader::WORKLOAD_REQUEST,
                              "Message type should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetDagId(), 42, "DAG ID should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetPayloadSize(), 1024, "Payload size should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.IsWorkloadRequest(),
                              true,
                              "IsWorkloadRequest should be true");
        NS_TEST_ASSERT_MSG_EQ(deserialized.IsResponse(), false, "IsResponse should be false");
    }
};

/**
 * @ingroup continuum-tests
 * @brief Test OrchestratorHeader serialization round-trip for ADMISSION_RESPONSE.
 */
class OrchestratorHeaderResponseTestCase : public TestCase
{
  public:
    OrchestratorHeaderResponseTestCase()
        : TestCase("Test OrchestratorHeader ADMISSION_RESPONSE roundtrip")
    {
    }

  private:
    void DoRun() override
    {
        OrchestratorHeader original;
        original.SetMessageType(OrchestratorHeader::ADMISSION_RESPONSE);
        original.SetDagId(123);
        original.SetAdmitted(true);
        original.SetPayloadSize(0);

        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(original);

        OrchestratorHeader deserialized;
        packet->RemoveHeader(deserialized);

        NS_TEST_ASSERT_MSG_EQ(deserialized.GetMessageType(),
                              OrchestratorHeader::ADMISSION_RESPONSE,
                              "Message type should be ADMISSION_RESPONSE");
        NS_TEST_ASSERT_MSG_EQ(deserialized.IsAdmitted(), true, "Should be admitted");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetDagId(), 123, "DAG ID should match");
        NS_TEST_ASSERT_MSG_EQ(deserialized.GetPayloadSize(), 0, "Payload size should be 0");
        NS_TEST_ASSERT_MSG_EQ(deserialized.IsWorkloadRequest(),
                              false,
                              "IsWorkloadRequest should be false");
        NS_TEST_ASSERT_MSG_EQ(deserialized.IsResponse(), true, "IsResponse should be true");
    }
};

} // namespace

TestCase*
CreateOrchestratorHeaderRequestTestCase()
{
    return new OrchestratorHeaderRequestTestCase;
}

TestCase*
CreateOrchestratorHeaderResponseTestCase()
{
    return new OrchestratorHeaderResponseTestCase;
}

} // namespace ns3
