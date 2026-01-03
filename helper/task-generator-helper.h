/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef TASK_GENERATOR_HELPER_H
#define TASK_GENERATOR_HELPER_H

#include "ns3/application-helper.h"
#include "ns3/gpu-accelerator.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Helper to create and configure TaskGenerator applications.
 */
class TaskGeneratorHelper : public ApplicationHelper
{
  public:
    /**
     * @brief Construct helper with default TaskGenerator type.
     */
    TaskGeneratorHelper();

    /**
     * @brief Construct helper with target accelerator.
     * @param accelerator Target GPU accelerator for generated tasks.
     */
    explicit TaskGeneratorHelper(Ptr<GpuAccelerator> accelerator);

    /**
     * @brief Set the target accelerator for generated tasks.
     * @param accelerator Pointer to the target GpuAccelerator.
     */
    void SetAccelerator(Ptr<GpuAccelerator> accelerator);

    /**
     * @brief Set the mean inter-arrival time for Poisson arrivals.
     * @param mean Mean inter-arrival time in seconds.
     */
    void SetMeanInterArrival(double mean);

    /**
     * @brief Set the mean compute demand.
     * @param mean Mean compute demand in FLOPS.
     */
    void SetMeanComputeDemand(double mean);

    /**
     * @brief Set the mean input size.
     * @param mean Mean input size in bytes.
     */
    void SetMeanInputSize(double mean);

    /**
     * @brief Set the mean output size.
     * @param mean Mean output size in bytes.
     */
    void SetMeanOutputSize(double mean);

    /**
     * @brief Set maximum number of tasks to generate.
     * @param maxTasks Maximum task count (0 = unlimited).
     */
    void SetMaxTasks(uint64_t maxTasks);
};

} // namespace ns3

#endif // TASK_GENERATOR_HELPER_H
