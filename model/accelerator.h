/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef ACCELERATOR_H
#define ACCELERATOR_H

#include "task.h"

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class Node;
class EnergyModel;

/**
 * @ingroup distributed
 * @brief Abstract base class for computational accelerators.
 *
 * Accelerator defines the interface for hardware that processes computational
 * tasks. Concrete implementations include GpuAccelerator, and could include
 * ASIC, FPGA, or other specialized hardware accelerators.
 *
 * Example usage:
 * @code
 * // Create a concrete accelerator (e.g., GPU)
 * Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
 * gpu->SetAttribute("ProcessingModel", PointerValue(processingModel));
 * node->AggregateObject(gpu);
 *
 * // Connect to trace sources
 * gpu->TraceConnectWithoutContext("TaskStarted", MakeCallback(&OnStart));
 * gpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&OnComplete));
 *
 * // Submit a task
 * Ptr<ComputeTask> task = CreateObject<ComputeTask>();
 * task->SetComputeDemand(1e9);
 * task->SetInputSize(1e6);
 * task->SetOutputSize(1e6);
 * gpu->SubmitTask(task);
 * @endcode
 */
class Accelerator : public Object
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
    Accelerator();

    /**
     * @brief Destructor.
     */
    ~Accelerator() override;

    /**
     * @brief Submit a task for execution.
     *
     * This is the core method that all accelerators must implement.
     * The implementation decides how to process the task (queueing,
     * scheduling, execution model, etc.).
     *
     * @param task The task to execute.
     */
    virtual void SubmitTask(Ptr<Task> task) = 0;

    /**
     * @brief Get the name of this accelerator type.
     *
     * @return A string identifying the accelerator (e.g., "GPU", "FPGA", "ASIC").
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Get the current queue length.
     *
     * Default implementation returns 0. Override in subclasses that
     * maintain a task queue.
     *
     * @return Number of tasks in queue (including currently executing).
     */
    virtual uint32_t GetQueueLength() const;

    /**
     * @brief Check if accelerator is currently busy.
     *
     * Default implementation returns false. Override in subclasses that
     * track execution state.
     *
     * @return True if executing a task.
     */
    virtual bool IsBusy() const;

    /**
     * @brief Get the current operating voltage.
     *
     * Default implementation returns 1.0V. Override in subclasses that
     * support DVFS or have configurable voltage.
     *
     * @return Operating voltage in Volts.
     */
    virtual double GetVoltage() const;

    /**
     * @brief Get the current operating frequency.
     *
     * Default implementation returns 1.0Hz. Override in subclasses that
     * support DVFS or have configurable frequency.
     *
     * @return Operating frequency in Hz.
     */
    virtual double GetFrequency() const;

    /**
     * @brief Get the node this accelerator is aggregated to.
     * @return Pointer to the node, or nullptr if not aggregated.
     */
    Ptr<Node> GetNode() const;

    /**
     * @brief Get the current power consumption.
     *
     * @return Current power in Watts, or 0 if no EnergyModel is configured.
     */
    double GetCurrentPower() const;

    /**
     * @brief Get the total energy consumed.
     *
     * @return Total energy consumed in Joules, or 0 if no EnergyModel is configured.
     */
    double GetTotalEnergy() const;

    /**
     * @brief TracedCallback signature for task events.
     * @param task The task.
     */
    typedef void (*TaskTracedCallback)(Ptr<const Task> task);

    /**
     * @brief TracedCallback signature for task completion.
     * @param task The completed task.
     * @param duration The total processing duration.
     */
    typedef void (*TaskCompletedTracedCallback)(Ptr<const Task> task, Time duration);

    /**
     * @brief TracedCallback signature for task failure.
     * @param task The failed task.
     * @param reason Description of why the task failed.
     */
    typedef void (*TaskFailedTracedCallback)(Ptr<const Task> task, std::string reason);

    /**
     * @brief TracedCallback signature for power state changes.
     * @param power Current power consumption in Watts.
     */
    typedef void (*PowerTracedCallback)(double power);

    /**
     * @brief TracedCallback signature for energy accumulation.
     * @param energy Total energy consumed in Joules.
     */
    typedef void (*EnergyTracedCallback)(double energy);

    /**
     * @brief TracedCallback signature for per-task energy.
     * @param task The completed task.
     * @param energy Energy consumed by this task in Joules.
     */
    typedef void (*TaskEnergyTracedCallback)(Ptr<const Task> task, double energy);

  protected:
    void DoDispose() override;
    void NotifyNewAggregate() override;

    /**
     * @brief Update the energy state based on current activity.
     *
     * This method should be called by subclasses when the accelerator's
     * activity state changes (e.g., starting or completing a task).
     * It accumulates energy from the previous state and calculates the
     * new power consumption.
     *
     * @param active Whether the accelerator is currently active.
     * @param utilization Current utilization level [0.0, 1.0].
     */
    void UpdateEnergyState(bool active, double utilization);

    /**
     * @brief Record the current energy as baseline for task energy tracking.
     *
     * Call this when starting a task to track per-task energy consumption.
     */
    void RecordTaskStartEnergy();

    /**
     * @brief Get energy consumed since RecordTaskStartEnergy was called.
     *
     * @return Energy consumed by the current task in Joules.
     */
    double GetTaskEnergy() const;

    Ptr<Node> m_node; //!< Node this accelerator is aggregated to

    // Trace sources for subclasses to fire
    TracedCallback<Ptr<const Task>> m_taskStartedTrace;             //!< Task started
    TracedCallback<Ptr<const Task>, Time> m_taskCompletedTrace;     //!< Task completed
    TracedCallback<Ptr<const Task>, std::string> m_taskFailedTrace; //!< Task failed
    TracedCallback<double> m_powerTrace;                            //!< Power state changes
    TracedCallback<double> m_energyTrace;                           //!< Total energy accumulation
    TracedCallback<Ptr<const Task>, double> m_taskEnergyTrace;      //!< Per-task energy

  private:
    Ptr<EnergyModel> m_energyModel; //!< Energy model for power calculation
    Time m_lastEnergyUpdateTime;    //!< Time of last energy state update
    double m_totalEnergy;           //!< Total energy consumed in Joules
    double m_currentPower;          //!< Current power consumption in Watts
    double m_taskStartEnergy;       //!< Energy at task start for per-task tracking
};

} // namespace ns3

#endif // ACCELERATOR_H
