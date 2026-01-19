/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef PROCESSING_MODEL_H
#define PROCESSING_MODEL_H

#include "task.h"

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

namespace ns3
{

class Accelerator;  // Forward declaration

/**
 * @ingroup distributed
 * @brief Abstract base class for compute processing models.
 *
 * ProcessingModel defines the interface for calculating task processing
 * characteristics such as execution time and output size. This decouples
 * the compute model from the hardware accelerator, allowing different
 * workload types (e.g., fixed-ratio compute, LLM inference, image recognition)
 * to be modeled independently.
 *
 * Example usage:
 * @code
 * Ptr<FixedRatioProcessingModel> model = CreateObject<FixedRatioProcessingModel>();
 *
 * Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
 * gpu->SetAttribute("ComputeRate", DoubleValue(1e12));
 * gpu->SetAttribute("MemoryBandwidth", DoubleValue(900e9));
 * gpu->SetAttribute("ProcessingModel", PointerValue(model));
 *
 * Ptr<ComputeTask> task = CreateObject<ComputeTask>();
 * task->SetComputeDemand(1e9);
 * task->SetInputSize(1e6);
 * task->SetOutputSize(1e6);
 *
 * ProcessingModel::Result result = model->Process(task, gpu);
 * if (result.success)
 * {
 *     Simulator::Schedule(result.processingTime, &OnComplete, result.outputSize);
 * }
 * @endcode
 */
class ProcessingModel : public Object
{
  public:
    /**
     * @brief Result of processing a task.
     *
     * Contains the calculated processing time, output size, utilization,
     * and success status.
     */
    struct Result
    {
        Time processingTime;  //!< Total time to process the task
        uint64_t outputSize;  //!< Output data size in bytes
        double utilization;   //!< Device utilization during processing [0.0, 1.0]
        bool success;         //!< True if processing calculation succeeded

        /**
         * @brief Default constructor creates a failed result.
         */
        Result()
            : processingTime(Seconds(0)),
              outputSize(0),
              utilization(0.0),
              success(false)
        {
        }

        /**
         * @brief Constructor for successful result.
         * @param time Processing time
         * @param output Output size in bytes
         * @param util Device utilization [0.0, 1.0], defaults to 1.0
         */
        Result(Time time, uint64_t output, double util = 1.0)
            : processingTime(time),
              outputSize(output),
              utilization(util),
              success(true)
        {
        }
    };

    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Default constructor.
     */
    ProcessingModel();

    /**
     * @brief Destructor.
     */
    ~ProcessingModel() override;

    /**
     * @brief Calculate processing characteristics for a task.
     *
     * Implementations should examine the task properties and return
     * the calculated processing time and output size. If the task
     * type is not supported, return a Result with success=false.
     *
     * @param task The task to process (const to prevent modification).
     * @param accelerator The accelerator providing hardware characteristics.
     * @return Result containing processing time, output size, and success status.
     */
    virtual Result Process(Ptr<const Task> task, Ptr<const Accelerator> accelerator) const = 0;

    /**
     * @brief Get the name of this processing model.
     *
     * @return A string identifying the model (e.g., "FixedRatio", "LLM").
     */
    virtual std::string GetName() const = 0;

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // PROCESSING_MODEL_H
