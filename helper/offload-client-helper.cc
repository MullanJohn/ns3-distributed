/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "offload-client-helper.h"

#include "ns3/address.h"
#include "ns3/double.h"
#include "ns3/nstime.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/string.h"

#include <sstream>

namespace ns3
{

OffloadClientHelper::OffloadClientHelper()
    : ApplicationHelper("ns3::OffloadClient")
{
}

OffloadClientHelper::OffloadClientHelper(const Address& orchestratorAddress)
    : ApplicationHelper("ns3::OffloadClient")
{
    m_factory.Set("Remote", AddressValue(orchestratorAddress));
}

void
OffloadClientHelper::SetOrchestratorAddress(const Address& addr)
{
    m_factory.Set("Remote", AddressValue(addr));
}

void
OffloadClientHelper::SetFrameRate(double fps)
{
    auto rv = CreateObject<ConstantRandomVariable>();
    rv->SetAttribute("Constant", DoubleValue(1.0 / fps));
    m_factory.Set("InterArrival", PointerValue(rv));
}

void
OffloadClientHelper::SetInterArrival(Ptr<RandomVariableStream> rv)
{
    m_factory.Set("InterArrival", PointerValue(rv));
}

void
OffloadClientHelper::SetMeanFrameSize(double mean, double stddev)
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
OffloadClientHelper::SetComputeDemand(double demand)
{
    std::ostringstream oss;
    oss << "ns3::ConstantRandomVariable[Constant=" << demand << "]";
    m_factory.Set("ComputeDemand", StringValue(oss.str()));
}

void
OffloadClientHelper::SetOutputSize(double size)
{
    std::ostringstream oss;
    oss << "ns3::ConstantRandomVariable[Constant=" << size << "]";
    m_factory.Set("OutputSize", StringValue(oss.str()));
}

void
OffloadClientHelper::SetDeadlineBudget(Time budget)
{
    m_factory.Set("DeadlineBudget", TimeValue(budget));
}

void
OffloadClientHelper::SetCommunicationBudget(Time budget)
{
    m_factory.Set("CommunicationBudget", TimeValue(budget));
}

void
OffloadClientHelper::SetSubmissionPolicy(Ptr<ClientSubmissionPolicy> policy)
{
    m_factory.Set("SubmissionPolicy", PointerValue(policy));
}

} // namespace ns3
