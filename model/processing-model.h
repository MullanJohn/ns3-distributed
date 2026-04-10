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

class Accelerator; // Forward declaration

/**
 * @ingroup continuum
 * @brief Abstract base class for compute processing models.
 *
 * ProcessingModel defines the interface for calculating task processing
 * characteristics such as execution time and output size.
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
 * @endcode
 */
class ProcessingModel : public Object
{
  public:
    /**
     * @brief Result of processing a task.
     *
     * Contains the calculated phase timings, total processing time,
     * output size, and success status.
     */
    struct Result
    {
        Time processingTime;     //!< Total time to process the task
        Time inputTransferTime;  //!< Time spent transferring input on-device
        Time computeTime;        //!< Time spent executing compute work
        Time outputTransferTime; //!< Time spent transferring output on-device
        uint64_t outputSize;     //!< Output data size in bytes
        bool success;            //!< True if processing calculation succeeded

        /**
         * @brief Default constructor creates a failed result.
         */
        Result()
            : processingTime(Seconds(0)),
              inputTransferTime(Seconds(0)),
              computeTime(Seconds(0)),
              outputTransferTime(Seconds(0)),
              outputSize(0),
              success(false)
        {
        }

        /**
         * @brief Constructor for successful result.
         * @param inputTime Input-transfer time
         * @param compute Input-independent compute-execution time
         * @param outputTime Output-transfer time
         * @param output Output size in bytes
         */
        Result(Time inputTime, Time compute, Time outputTime, uint64_t output)
            : processingTime(inputTime + compute + outputTime),
              inputTransferTime(inputTime),
              computeTime(compute),
              outputTransferTime(outputTime),
              outputSize(output),
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
     * the calculated phase timings, processing time, and output size.
     * If the task type is not supported, return a Result with success=false.
     *
     * @param task The task to process.
     * @param accelerator The accelerator providing hardware characteristics.
     * @return Result containing phase timings, output size, and success status.
     */
    virtual Result Process(Ptr<const Task> task, Ptr<const Accelerator> accelerator) const = 0;

    /**
     * @brief Get the name of this processing model.
     *
     * @return A string identifying the model (e.g., "FixedRatio").
     */
    virtual std::string GetName() const = 0;

  protected:
    void DoDispose() override;
};

} // namespace ns3

#endif // PROCESSING_MODEL_H
