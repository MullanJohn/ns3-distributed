/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/test.h"

namespace ns3
{

// Factories implemented in the other .cc files
TestCase* CreateGpuAcceleratorTestCase();
TestCase* CreateGpuAcceleratorQueueTestCase();
TestCase* CreateTaskGeneratorTestCase();

class DistributedTestSuite : public TestSuite
{
  public:
    DistributedTestSuite();
};

DistributedTestSuite::DistributedTestSuite()
    : TestSuite("distributed", Type::UNIT)
{
    AddTestCase(CreateGpuAcceleratorTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateGpuAcceleratorQueueTestCase(), TestCase::Duration::QUICK);
    AddTestCase(CreateTaskGeneratorTestCase(), TestCase::Duration::QUICK);
}

static DistributedTestSuite sDistributedTestSuite;

} // namespace ns3
