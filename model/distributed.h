/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef DISTRIBUTED_H
#define DISTRIBUTED_H

/**
 * @defgroup distributed Distributed Computing Module
 *
 * This module provides classes for simulating distributed computing workloads
 * in ns-3. It models task offloading from clients to servers with hardware
 * accelerators, including load balancing across server clusters.
 *
 * Key components:
 * - Task: Abstract interface for all task types (GetTaskId, GetName, ArrivalTime)
 * - Accelerator: Abstract interface for hardware accelerators (GPU, FPGA, ASIC)
 * - OffloadClient/OffloadServer: TCP-based client-server for task offloading
 * - LoadBalancer: Distributes tasks across a cluster of servers
 * - NodeScheduler: Abstract interface for scheduling policies
 */

// Model headers
#include "ns3/accelerator.h"
#include "ns3/cluster.h"
#include "ns3/compute-task.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/load-balancer.h"
#include "ns3/node-scheduler.h"
#include "ns3/offload-client.h"
#include "ns3/offload-header.h"
#include "ns3/offload-server.h"
#include "ns3/round-robin-scheduler.h"
#include "ns3/task.h"

#endif // DISTRIBUTED_H
