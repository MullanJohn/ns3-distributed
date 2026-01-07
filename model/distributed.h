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
 * in ns-3. It models task offloading from clients to servers with GPU
 * accelerators, including load balancing across server clusters.
 *
 * Key components:
 * - Task: Represents a computational task with compute demand and I/O sizes
 * - GpuAccelerator: Models GPU processing with three-phase execution
 * - OffloadClient/OffloadServer: TCP-based client-server for task offloading
 * - LoadBalancer: Distributes tasks across a cluster of servers
 * - NodeScheduler: Abstract interface for scheduling policies
 */

// Model headers
#include "ns3/cluster.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/load-balancer.h"
#include "ns3/node-scheduler.h"
#include "ns3/offload-client.h"
#include "ns3/offload-header.h"
#include "ns3/offload-server.h"
#include "ns3/round-robin-scheduler.h"
#include "ns3/task.h"

#endif // DISTRIBUTED_H
