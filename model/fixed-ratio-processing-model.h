/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef FIXED_RATIO_PROCESSING_MODEL_H
#define FIXED_RATIO_PROCESSING_MODEL_H

#include "processing-model.h"

namespace ns3
{

/**
 * @ingroup distributed
 * @brief Processing model using three-phase timing for ComputeTask on GpuAccelerator.
 *
 * FixedRatioProcessingModel calculates processing time using a three-phase
 * model (input transfer, compute, output transfer) based on ComputeTask
 * properties and GpuAccelerator hardware characteristics.
 *
 * Processing time calculation:
 * - Input transfer: inputSize / GpuAccelerator.MemoryBandwidth
 * - Compute: computeDemand / GpuAccelerator.ComputeRate
 * - Output transfer: outputSize / GpuAccelerator.MemoryBandwidth
 * - Total: sum of all three phases
 *
 * This model requires:
 * - Task must be a ComputeTask (returns failure otherwise)
 * - Accelerator must be a GpuAccelerator (returns failure otherwise)
 *
 * Example usage:
 * @code
 * Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
 *
 * Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
 * gpu->SetAttribute("ComputeRate", DoubleValue(1e12));      // 1 TFLOPS
 * gpu->SetAttribute("MemoryBandwidth", DoubleValue(900e9)); // 900 GB/s
 * gpu->SetAttribute("ProcessingModel", PointerValue(model));
 * @endcode
 */
class FixedRatioProcessingModel : public ProcessingModel
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Default constructor.
     */
    FixedRatioProcessingModel();

    /**
     * @brief Destructor.
     */
    ~FixedRatioProcessingModel() override;

    // ProcessingModel interface
    Result Process(Ptr<const Task> task, Ptr<const Accelerator> accelerator) const override;
    std::string GetName() const override;

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // FIXED_RATIO_PROCESSING_MODEL_H
