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
 * - Task/TaskHeader: Abstract interfaces for tasks and protocol headers
 * - Accelerator: Abstract interface for hardware accelerators (GPU, FPGA, ASIC)
 * - ProcessingModel: Abstract interface for compute timing models
 * - QueueScheduler: Abstract interface for task queue management
 * - ConnectionManager: Abstract interface for transport layer (TCP, UDP)
 * - NodeScheduler: Abstract interface for backend selection policies
 * - OffloadClient/OffloadServer: TCP-based client-server for task offloading
 * - LoadBalancer: Distributes tasks across a cluster of servers
 */

// Task
#include "ns3/dag-task.h"
#include "ns3/simple-task-header.h"
#include "ns3/simple-task.h"
#include "ns3/task-header.h"
#include "ns3/task.h"

// Accelerator
#include "ns3/accelerator.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/processing-model.h"

// Queue management
#include "ns3/batching-queue-scheduler.h"
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/queue-scheduler.h"

// Connection manaager
#include "ns3/connection-manager.h"
#include "ns3/tcp-connection-manager.h"
#include "ns3/udp-connection-manager.h"

// Load balancing
#include "ns3/cluster.h"
#include "ns3/load-balancer.h"
#include "ns3/node-scheduler.h"
#include "ns3/round-robin-scheduler.h"

// Applications
#include "ns3/offload-client.h"
#include "ns3/offload-server.h"

#endif // DISTRIBUTED_H
