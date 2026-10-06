/*
 * Copyright (c) 2025 UCC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: John Mullan <122331816@umail.ucc.ie>
 */

#ifndef CONTINUUM_H
#define CONTINUUM_H

/**
 * @defgroup continuum Computing Continuum Module
 *
 * This module provides classes for simulating computing continuum workloads
 * in ns-3. It models task offloading from clients to servers with hardware
 * accelerators.
 *
 * Key components:
 * - Task/TaskHeader: Abstract interfaces for tasks and protocol headers
 * - Accelerator: Abstract interface for hardware accelerators (GPU, FPGA, ASIC)
 * - ProcessingModel: Abstract interface for compute timing models
 * - QueueScheduler: Abstract interface for task queue management
 * - ConnectionManager: Abstract interface for transport layer (TCP, UDP)
 * - ClusterScheduler: Abstract interface for backend selection policies
 * - OffloadClient/BackendServer: Configurable client-server pair for task offloading
 */

// Task
#include "ns3/dag-task.h"
#include "ns3/simple-task.h"
#include "ns3/task-header.h"
#include "ns3/task.h"

// Accelerator
#include "ns3/accelerator.h"
#include "ns3/dvfs-energy-model.h"
#include "ns3/energy-model.h"
#include "ns3/fixed-ratio-processing-model.h"
#include "ns3/gpu-accelerator.h"
#include "ns3/processing-model.h"

// Queue management
#include "ns3/fifo-queue-scheduler.h"
#include "ns3/queue-scheduler.h"

// Connection manager
#include "ns3/connection-manager.h"
#include "ns3/tcp-connection-manager.h"
#include "ns3/udp-connection-manager.h"

// Orchestration
#include "ns3/admission-policy.h"
#include "ns3/cluster-scheduler.h"
#include "ns3/cluster-state.h"
#include "ns3/cluster.h"
#include "ns3/conservative-scaling-policy.h"
#include "ns3/least-loaded-scheduler.h"
#include "ns3/max-active-tasks-policy.h"
#include "ns3/network-aware-scheduler.h"
#include "ns3/orchestrator-header.h"
#include "ns3/orchestrator.h"
#include "ns3/round-robin-scheduler.h"
#include "ns3/tiered-delay-scheduler.h"

// Device management
#include "ns3/device-manager.h"
#include "ns3/device-metrics-header.h"
#include "ns3/device-protocol.h"
#include "ns3/gpu-device-protocol.h"
#include "ns3/scaling-command-header.h"
#include "ns3/scaling-policy.h"
#include "ns3/utilization-scaling-policy.h"

// Client submission and admission policies
#include "ns3/admission-retry-policy.h"
#include "ns3/always-retry-admission-policy.h"
#include "ns3/always-submit-policy.h"
#include "ns3/client-submission-policy.h"
#include "ns3/drop-while-pending-submission-policy.h"
#include "ns3/never-retry-admission-policy.h"

// Applications
#include "ns3/backend-server.h"
#include "ns3/offload-client.h"

// Helpers
#include "ns3/continuum-helper.h"

#endif // CONTINUUM_H
