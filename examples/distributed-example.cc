/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#include "ns3/core-module.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/network-module.h"
#include "ns3/task-generator-helper.h"
#include "ns3/task-generator.h"
#include "ns3/task.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistributedExample");

// ===========================================================================
//
//  Distributed Computing Simulation Example
//
//  This example demonstrates a task generator sending computational tasks
//  to a GPU accelerator. Tasks arrive following a Poisson process and are
//  processed sequentially by the GPU.
//
//         TaskGenerator                    GpuAccelerator
//       +---------------+                +------------------+
//       |   Generates   |   SubmitTask   |   Processes      |
//       |    tasks      | -------------> |   tasks via:     |
//       |   (Poisson    |                |   1. Input xfer  |
//       |    arrivals)  |                |   2. Compute     |
//       +---------------+                |   3. Output xfer |
//                                        +------------------+
//
//  Task parameters (compute demand, input/output sizes) and GPU parameters
//  (compute rate, memory bandwidth) can be configured via command line.
//
// ===========================================================================

/**
 * Callback for when a task is generated
 *
 * @param task The generated task
 */
static void
TaskGenerated(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                  << "s: Task " << task->GetTaskId() << " generated"
                  << " | compute=" << task->GetComputeDemand() / 1e9 << " GFLOP"
                  << " | input=" << task->GetInputSize() / 1024.0 << " KB"
                  << " | output=" << task->GetOutputSize() / 1024.0 << " KB");
}

/**
 * Callback for when a task starts processing
 *
 * @param task The task that started
 */
static void
TaskStarted(Ptr<const Task> task)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                  << "s: Task " << task->GetTaskId() << " started processing");
}

/**
 * Callback for when a task completes
 *
 * @param task The completed task
 * @param duration Processing duration
 */
static void
TaskCompleted(Ptr<const Task> task, Time duration)
{
    Time responseTime = Simulator::Now() - task->GetArrivalTime();
    NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                  << "s: Task " << task->GetTaskId() << " completed"
                  << " | processing=" << duration.GetMilliSeconds() << "ms"
                  << " | response=" << responseTime.GetMilliSeconds() << "ms");
}

/**
 * Callback for queue length changes
 *
 * @param oldValue Previous queue length
 * @param newValue New queue length
 */
static void
QueueLengthChanged(uint32_t oldValue, uint32_t newValue)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                  << "s: Queue length changed: " << oldValue << " -> " << newValue);
}

int
main(int argc, char* argv[])
{
    // Default parameters
    double simTime = 0.5;             // Simulation time in seconds
    uint64_t numTasks = 20;           // Number of tasks to generate
    double meanInterArrival = 0.02;   // Mean inter-arrival time (20ms)
    double meanComputeDemand = 5e9;   // Mean compute demand (5 GFLOP)
    double meanInputSize = 10e6;      // Mean input size (10 MB)
    double meanOutputSize = 1e6;      // Mean output size (1 MB)
    double computeRate = 1e12;        // GPU compute rate (1 TFLOPS)
    double memoryBandwidth = 900e9;   // GPU memory bandwidth (900 GB/s)
    bool traceQueue = false;          // Trace queue length changes

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("numTasks", "Number of tasks to generate (0=unlimited)", numTasks);
    cmd.AddValue("meanInterArrival", "Mean task inter-arrival time in seconds", meanInterArrival);
    cmd.AddValue("meanComputeDemand", "Mean compute demand in FLOPS", meanComputeDemand);
    cmd.AddValue("meanInputSize", "Mean input data size in bytes", meanInputSize);
    cmd.AddValue("meanOutputSize", "Mean output data size in bytes", meanOutputSize);
    cmd.AddValue("computeRate", "GPU compute rate in FLOPS", computeRate);
    cmd.AddValue("memoryBandwidth", "GPU memory bandwidth in bytes/sec", memoryBandwidth);
    cmd.AddValue("traceQueue", "Enable queue length tracing", traceQueue);
    cmd.Parse(argc, argv);

    // Create a node to host the application
    Ptr<Node> node = CreateObject<Node>();

    // Create GPU accelerator with specified parameters
    Ptr<GpuAccelerator> gpu = CreateObject<GpuAccelerator>();
    gpu->SetAttribute("ComputeRate", DoubleValue(computeRate));
    gpu->SetAttribute("MemoryBandwidth", DoubleValue(memoryBandwidth));

    // Connect GPU trace sources
    gpu->TraceConnectWithoutContext("TaskStarted", MakeCallback(&TaskStarted));
    gpu->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&TaskCompleted));
    if (traceQueue)
    {
        gpu->TraceConnectWithoutContext("QueueLength", MakeCallback(&QueueLengthChanged));
    }

    // Create task generator using helper
    TaskGeneratorHelper helper(gpu);
    helper.SetMeanInterArrival(meanInterArrival);
    helper.SetMeanComputeDemand(meanComputeDemand);
    helper.SetMeanInputSize(meanInputSize);
    helper.SetMeanOutputSize(meanOutputSize);
    helper.SetMaxTasks(numTasks);

    // Install application on node
    ApplicationContainer apps = helper.Install(node);

    // Connect task generator trace
    Ptr<TaskGenerator> generator = DynamicCast<TaskGenerator>(apps.Get(0));
    generator->TraceConnectWithoutContext("TaskGenerated", MakeCallback(&TaskGenerated));

    // Set application timing
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(simTime));

    // Run simulation
    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
