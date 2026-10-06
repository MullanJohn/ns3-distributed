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
#include "ns3/enum.h"
#include "ns3/nstime.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simple-task.h"
#include "ns3/string.h"

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
OffloadClientHelper::SetTaskRate(double rate)
{
    auto rv = CreateObject<ConstantRandomVariable>();
    rv->SetAttribute("Constant", DoubleValue(1.0 / rate));
    m_factory.Set("InterArrival", PointerValue(rv));
}

void
OffloadClientHelper::SetInterArrival(Ptr<RandomVariableStream> rv)
{
    m_factory.Set("InterArrival", PointerValue(rv));
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

void
OffloadClientHelper::SetAdmissionMode(OffloadClient::AdmissionMode mode)
{
    m_factory.Set("AdmissionMode", EnumValue(mode));
}

void
OffloadClientHelper::SetAdmissionRetryPolicy(Ptr<AdmissionRetryPolicy> policy)
{
    m_factory.Set("AdmissionRetryPolicy", PointerValue(policy));
}

void
OffloadClientHelper::SetDagTemplate(Ptr<DagTask> dagTemplate)
{
    m_factory.Set("DagTemplate", PointerValue(dagTemplate));
}

void
OffloadClientHelper::SetLinearChainDagTemplate(uint32_t numTasks,
                                               Ptr<RandomVariableStream> inputSizeDist,
                                               Ptr<RandomVariableStream> computeDemandDist,
                                               Ptr<RandomVariableStream> outputSizeDist)
{
    Ptr<DagTask> dagTemplate = CreateObject<DagTask>();

    for (uint32_t i = 0; i < numTasks; i++)
    {
        Ptr<SimpleTask> task = CreateObject<SimpleTask>();
        if (i == 0)
        {
            task->SetAttribute("InputSizeDistribution", PointerValue(inputSizeDist));
        }
        task->SetAttribute("ComputeDemandDistribution", PointerValue(computeDemandDist));
        task->SetAttribute("OutputSizeDistribution", PointerValue(outputSizeDist));
        dagTemplate->AddTask(task);

        if (i > 0)
        {
            dagTemplate->AddDataDependency(i - 1, i);
        }
    }

    SetDagTemplate(dagTemplate);
}

void
OffloadClientHelper::SetGracePeriod(Time gracePeriod)
{
    m_factory.Set("GracePeriod", TimeValue(gracePeriod));
}

} // namespace ns3
