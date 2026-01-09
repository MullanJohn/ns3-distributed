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
 * @brief Processing model with fixed compute intensity and output ratio.
 *
 * FixedRatioProcessingModel calculates processing time using a three-phase
 * model (input transfer, compute, output transfer) based on task properties
 * and accelerator hardware characteristics.
 *
 * Processing time calculation for ComputeTask:
 * - Input transfer: inputSize / accelerator.MemoryBandwidth
 * - Compute: computeDemand / accelerator.ComputeRate
 * - Output transfer: outputSize / accelerator.MemoryBandwidth
 * - Total: sum of all three phases
 *
 * The FlopsPerByte and OutputRatio attributes can be used for future
 * task types that don't specify explicit compute demand and I/O sizes.
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

    /**
     * @brief Get FLOPS per byte ratio.
     * @return The compute intensity ratio.
     */
    double GetFlopsPerByte() const;

    /**
     * @brief Get output to input ratio.
     * @return The output ratio.
     */
    double GetOutputRatio() const;

  protected:
    void DoDispose() override;

  private:
    double m_flopsPerByte;     //!< Compute intensity (FLOPS per byte of input)
    double m_outputRatio;      //!< Output size / input size ratio
};

} // namespace ns3

#endif // FIXED_RATIO_PROCESSING_MODEL_H
