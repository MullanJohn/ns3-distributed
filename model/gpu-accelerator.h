/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef GPU_ACCELERATOR_H
#define GPU_ACCELERATOR_H

#include "accelerator.h"
#include "processing-model.h"
#include "queue-scheduler.h"
#include "task.h"

#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/traced-value.h"

#include <deque>

namespace ns3
{

/**
 * @ingroup distributed
 * @brief GPU accelerator for processing computational tasks.
 *
 * GpuAccelerator models a GPU processing unit. Task processing time
 * is determined by the attached ProcessingModel, which must be set
 * before tasks can be submitted.
 */
class GpuAccelerator : public Accelerator
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    GpuAccelerator();
    ~GpuAccelerator() override;

    // Accelerator interface implementation
    void SubmitTask(Ptr<Task> task) override;
    std::string GetName() const override;
    uint32_t GetQueueLength() const override;
    bool IsBusy() const override;
    double GetUtilization() const override;
    double GetVoltage() const override;
    double GetFrequency() const override;
    void SetFrequency(double frequency) override;
    void SetVoltage(double voltage) override;

    /**
     * @brief Get effective compute rate in FLOPS.
     *
     * Returns the product of the raw compute rate (scaled by DVFS)
     * and the ComputeEfficiency attribute.
     *
     * @return The effective compute rate.
     */
    double GetComputeRate() const override;

    /**
     * @brief Get memory bandwidth in bytes/sec.
     * @return The memory bandwidth.
     */
    double GetMemoryBandwidth() const;

    void ApplyState(uint32_t stateIdx) override;
    uint32_t GetNumPerformanceStates() const override;
    double GetComputeRateAtState(uint32_t stateIdx) const override;

  protected:
    void DoInitialize() override;
    void DoDispose() override;

  private:
    /**
     * @brief Phase-aware timing breakdown for a single task or task segment.
     *
     * Used by DVFS rescaling to adjust only the compute phase when
     * frequency changes mid-task.
     */
    struct TaskProfile
    {
        Time inputTransferTime{Seconds(0)};  //!< Host-to-device transfer time
        Time computeTime{Seconds(0)};        //!< Compute execution time
        Time outputTransferTime{Seconds(0)}; //!< Device-to-host transfer time
        Time processingTime{Seconds(0)};     //!< Sum of all three phases
    };

    /**
     * @brief A closed time interval [start, end) used for utilization tracking.
     */
    struct TimeInterval
    {
        Time start{Seconds(0)}; //!< Interval start time
        Time end{Seconds(0)};   //!< Interval end time
    };

    /**
     * @brief Start processing the next task from queue.
     */
    void StartNextTask();

    /**
     * @brief Called when ProcessingModel completes task processing.
     */
    void ProcessingComplete();

    /**
     * @brief Reset current task state to idle.
     */
    void ClearCurrentTask();

    /**
     * @brief Compute the remaining task profile after a mid-task DVFS change.
     *
     * Only the compute phase is rescaled by the frequency ratio; memory
     * transfer phases are frequency-independent.
     *
     * @param elapsed Time elapsed since the current segment started.
     * @param ratio New frequency divided by old frequency.
     * @return Remaining task profile with adjusted compute time.
     */
    TaskProfile GetRemainingTaskProfile(Time elapsed, double ratio) const;

    /**
     * @brief Record the compute interval of the current task segment up to now.
     * @param now Current simulation time.
     */
    void AccumulateCurrentComputeUntil(Time now);

    /**
     * @brief Remove compute intervals that have fallen outside the utilization window.
     * @param now Current simulation time.
     */
    void PruneUtilizationHistory(Time now) const;

    /**
     * @brief Compute rolling compute utilization at a given time.
     * @param now The time at which to evaluate utilization.
     * @return Utilization in [0.0, 1.0].
     */
    double ComputeUtilizationAt(Time now) const;

    /**
     * @brief Compute the duty cycle (compute fraction) of a task profile.
     * @param profile The task profile.
     * @return computeTime / processingTime, or 0.0 if processingTime is zero.
     */
    static double CalculateTaskUtilization(const TaskProfile& profile);

    /**
     * @brief Sum interval durations that overlap a given time window.
     * @param intervals The interval deque to scan.
     * @param windowStart Start of the observation window.
     * @param windowEnd End of the observation window.
     * @return Total overlapping seconds.
     */
    static double SumWindowedIntervalSeconds(const std::deque<TimeInterval>& intervals,
                                             Time windowStart,
                                             Time windowEnd);

    /**
     * @brief Append a time interval to the deque if it has positive duration.
     * @param intervals The interval deque.
     * @param start Interval start.
     * @param end Interval end.
     */
    static void AppendInterval(std::deque<TimeInterval>& intervals, Time start, Time end);

    // GPU-specific attributes
    double m_computeRate;                   //!< Raw compute rate in FLOPS (scaled by DVFS)
    double m_computeEfficiency;             //!< Fraction of peak rate achieved by workload (0,1]
    double m_memoryBandwidth;               //!< Memory bandwidth in bytes/sec
    double m_frequency;                     //!< Operating frequency in Hz
    double m_voltage;                       //!< Operating voltage in Volts
    Time m_utilizationWindow;               //!< Rolling window for compute utilization
    Ptr<ProcessingModel> m_processingModel; //!< Processing model for timing calculation
    Ptr<QueueScheduler> m_queueScheduler;   //!< Queue scheduler for task management

    // Task execution state
    Ptr<Task> m_currentTask;          //!< Currently executing task
    EventId m_currentEvent;           //!< Scheduled completion event
    Time m_taskStartTime;             //!< When current task started
    Time m_currentSegmentStartTime;   //!< Start of current DVFS segment
    TaskProfile m_currentTaskProfile; //!< Phase profile of current segment

    // Utilization tracking
    mutable std::deque<TimeInterval> m_computeIntervals; //!< Historical compute intervals

    // Traced values
    TracedValue<uint32_t> m_queueLength; //!< Current queue length
};

} // namespace ns3

#endif // GPU_ACCELERATOR_H
