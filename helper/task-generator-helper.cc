/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "task-generator-helper.h"

#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include <sstream>

namespace ns3
{

TaskGeneratorHelper::TaskGeneratorHelper()
    : ApplicationHelper("ns3::TaskGenerator")
{
}

TaskGeneratorHelper::TaskGeneratorHelper(Ptr<GpuAccelerator> accelerator)
    : ApplicationHelper("ns3::TaskGenerator")
{
    m_factory.Set("Accelerator", PointerValue(accelerator));
}

void
TaskGeneratorHelper::SetAccelerator(Ptr<GpuAccelerator> accelerator)
{
    m_factory.Set("Accelerator", PointerValue(accelerator));
}

void
TaskGeneratorHelper::SetMeanInterArrival(double mean)
{
    std::ostringstream oss;
    oss << "ns3::ExponentialRandomVariable[Mean=" << mean << "]";
    m_factory.Set("InterArrivalTime", StringValue(oss.str()));
}

void
TaskGeneratorHelper::SetMeanComputeDemand(double mean)
{
    std::ostringstream oss;
    oss << "ns3::ExponentialRandomVariable[Mean=" << mean << "]";
    m_factory.Set("ComputeDemand", StringValue(oss.str()));
}

void
TaskGeneratorHelper::SetMeanInputSize(double mean)
{
    std::ostringstream oss;
    oss << "ns3::ExponentialRandomVariable[Mean=" << mean << "]";
    m_factory.Set("InputSize", StringValue(oss.str()));
}

void
TaskGeneratorHelper::SetMeanOutputSize(double mean)
{
    std::ostringstream oss;
    oss << "ns3::ExponentialRandomVariable[Mean=" << mean << "]";
    m_factory.Set("OutputSize", StringValue(oss.str()));
}

void
TaskGeneratorHelper::SetMaxTasks(uint64_t maxTasks)
{
    m_factory.Set("MaxTasks", UintegerValue(maxTasks));
}

} // namespace ns3
