/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-client-helper.h"

#include "ns3/address.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include <sstream>

namespace ns3
{

OffloadClientHelper::OffloadClientHelper()
    : ApplicationHelper("ns3::OffloadClient")
{
}

OffloadClientHelper::OffloadClientHelper(const Address& serverAddress)
    : ApplicationHelper("ns3::OffloadClient")
{
    m_factory.Set("Remote", AddressValue(serverAddress));
}

void
OffloadClientHelper::SetServerAddress(const Address& serverAddress)
{
    m_factory.Set("Remote", AddressValue(serverAddress));
}

void
OffloadClientHelper::SetMeanInterArrival(double mean)
{
    std::ostringstream oss;
    oss << "ns3::ExponentialRandomVariable[Mean=" << mean << "]";
    m_factory.Set("InterArrivalTime", StringValue(oss.str()));
}

void
OffloadClientHelper::SetMeanComputeDemand(double mean)
{
    std::ostringstream oss;
    oss << "ns3::ExponentialRandomVariable[Mean=" << mean << "]";
    m_factory.Set("ComputeDemand", StringValue(oss.str()));
}

void
OffloadClientHelper::SetMeanInputSize(double mean)
{
    std::ostringstream oss;
    oss << "ns3::ExponentialRandomVariable[Mean=" << mean << "]";
    m_factory.Set("InputSize", StringValue(oss.str()));
}

void
OffloadClientHelper::SetMeanOutputSize(double mean)
{
    std::ostringstream oss;
    oss << "ns3::ExponentialRandomVariable[Mean=" << mean << "]";
    m_factory.Set("OutputSize", StringValue(oss.str()));
}

void
OffloadClientHelper::SetMaxTasks(uint64_t maxTasks)
{
    m_factory.Set("MaxTasks", UintegerValue(maxTasks));
}

} // namespace ns3
