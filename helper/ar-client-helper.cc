/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ar-client-helper.h"

#include "ns3/address.h"
#include "ns3/double.h"
#include "ns3/string.h"

#include <sstream>

namespace ns3
{

ArClientHelper::ArClientHelper()
    : ApplicationHelper("ns3::ArClient")
{
}

ArClientHelper::ArClientHelper(const Address& orchestratorAddress)
    : ApplicationHelper("ns3::ArClient")
{
    m_factory.Set("Remote", AddressValue(orchestratorAddress));
}

void
ArClientHelper::SetOrchestratorAddress(const Address& addr)
{
    m_factory.Set("Remote", AddressValue(addr));
}

void
ArClientHelper::SetFrameRate(double fps)
{
    m_factory.Set("FrameRate", DoubleValue(fps));
}

void
ArClientHelper::SetMeanFrameSize(double mean, double stddev)
{
    std::ostringstream oss;
    if (stddev > 0)
    {
        oss << "ns3::NormalRandomVariable[Mean=" << mean << "|Variance=" << (stddev * stddev)
            << "|Bound=" << (3.0 * stddev) << "]";
    }
    else
    {
        oss << "ns3::ConstantRandomVariable[Constant=" << mean << "]";
    }
    m_factory.Set("FrameSize", StringValue(oss.str()));
}

void
ArClientHelper::SetComputeDemand(double demand)
{
    std::ostringstream oss;
    oss << "ns3::ConstantRandomVariable[Constant=" << demand << "]";
    m_factory.Set("ComputeDemand", StringValue(oss.str()));
}

void
ArClientHelper::SetOutputSize(double size)
{
    std::ostringstream oss;
    oss << "ns3::ConstantRandomVariable[Constant=" << size << "]";
    m_factory.Set("OutputSize", StringValue(oss.str()));
}

} // namespace ns3
