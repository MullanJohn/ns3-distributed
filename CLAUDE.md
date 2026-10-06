# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`continuum` is an ns-3 (ns-3.44) contrib module implementing a "computing continuum" simulation:
offloading computational tasks (e.g. AI/LLM workloads) from client devices to backend servers with
hardware accelerators over a simulated network. It lives at `contrib/continuum` inside a checkout of
`ns-3-dev`, and all commands below are run from the ns-3 root (one level up), not from this directory.

## Build & test commands

Run from the ns-3 root (parent of `contrib/continuum`):

```bash
./ns3 configure --enable-examples --enable-tests   # Configure (only needed once / after changes)
./ns3 build                                        # Build
./test.py -s continuum                             # Run all continuum test suites
./test.py -s continuum -v                          # Verbose test output
./ns3 run continuum-example                        # Run the basic example
./ns3 run continuum-multi-client-example           # Run the multi-client example
```

There is a single test suite entry point registered in `test/continuum-test-suite.cc`; individual test
files under `test/` are added as `TEST_SOURCES` in `CMakeLists.txt` and each corresponds 1:1 to a
`model/*.cc` file (e.g. `model/orchestrator.cc` ↔ `test/orchestrator-test.cc`). When adding a new
model class with non-trivial logic, add a matching test file and register it in `CMakeLists.txt`.

Formatting is enforced by CI (`.github/workflows/auto-format.yml`), which runs `clang-format-16`
against the ns-3.44 `.clang-format` style on every PR and auto-commits fixes — don't hand-tune
whitespace/brace style, just let it pass through the ns-3 formatter conventions already visible in
the codebase (4-space indent, Allman braces, `m_` member prefix).

### Dev container

A `Dockerfile.dev` clones `ns-3-dev` at tag `ns-3.44` and expects this repo to be bind-mounted at
`/workspace/ns-3-dev/contrib/continuum`:

```bash
docker build -f Dockerfile.dev -t continuum-dev .
docker run -it -v "$(pwd)":/workspace/ns-3-dev/contrib/continuum continuum-dev
```

## Architecture

### Pipeline

`OffloadClient` (application on a client node) → `Orchestrator` (application on a controller node,
admission + scheduling) → `BackendServer` (application on each backend node, runs an `Accelerator`).
The set of backends is described by a `Cluster` (parallel to ns-3's `NodeContainer`, but pairs each
node with its listen `Address` and an accelerator-type tag).

### Wire protocol

Task submission is split into an admission phase and a data-upload phase, distinguished by peeking
the first byte of a buffer:
1. **Admission** (`OrchestratorHeader`, message types `ADMISSION_REQUEST`/`ADMISSION_RESPONSE`, types
   2–3): client sends DAG *metadata only* (headers, no payload) via `DagTask::SerializeMetadata()`.
   The orchestrator runs the `AdmissionPolicy` and reserves capacity before any payload bytes are
   transferred.
2. **Upload** (`DATA_UPLOAD`, type 4): once admitted, the client sends the full DAG
   (`DagTask::SerializeFullData()`), and the orchestrator dispatches ready tasks to backends using
   normal `TaskHeader` framing (message types `TASK_REQUEST`/`TASK_RESPONSE`, types 0–1).
3. **Workload response** (`WORKLOAD_RESPONSE`, type 7): when the DAG finishes, the orchestrator
   returns the **entire executed DAG** to the client in one message
   (`DagTask::SerializeFullData(isResponse=true)`), not one packet per sink task. The client's
   `WorkloadProcessed`/`WorkloadViolated` traces therefore fire exactly once per workload no matter
   how many sink tasks the DAG has, and receiving this message *is* the completion signal (the
   pending entry is cleared unconditionally — the client never marks non-sink tasks itself).

The returned DAG is the authoritative record of a workload: every task carries its `backendTime`
(queue + compute) and `computeTime` (accelerator execution only), both serialized in `TaskHeader`,
so `backendTime - computeTime` is queueing delay and `latency - Σ backendTime` is communication
time — derive metrics from the DAG rather than instrumenting backends separately.

Note that `DagTask` prefixes each serialized task with a 1-byte task-type tag, so any deserializer
callback passed to `DagTask::Deserialize*` must strip that byte first (see
`OffloadClient::DeserializeResponseTask` or `Orchestrator::DispatchDeserialize`) — passing
`SimpleTask::Deserialize` directly silently yields garbage fields.

A separate lower-priority channel carries device scaling: `DeviceMetricsHeader` (type-4-in-the-device-
protocol; backends → orchestrator) and `ScalingCommandHeader` (orchestrator → backends), handled by
`DeviceManager`/`DeviceProtocol` rather than the task path.

Because all these headers share a byte-peeking dispatch convention, message-type numeric values are
coordinated across `TaskHeader`, `OrchestratorHeader`, and the device protocol headers — check for
collisions before adding a new message type.

### Task model

`Task` is an abstract base (compute demand, I/O sizes, deadline, priority, required accelerator type,
lifecycle `TaskState`). `SimpleTask` is the reference concrete implementation. `DagTask` is a
*container* of `Task`s plus dependency edges (not a `Task` itself) — it tracks in-degree/ready-set
incrementally and supports both ordering-only and data-flow dependencies (`AddDependency` vs.
`AddDataDependency`, the latter accumulating upstream output size into downstream input size).

New task types are added by subclassing `Task`, implementing `Serialize`/`GetTaskType`, providing
static `Deserialize`/`DeserializeHeader` factories, and calling
`Orchestrator::RegisterTaskType(taskType, fullDeserializer, metadataDeserializer)` before
`StartApplication()` — this is how the orchestrator's DAG deserialization dispatches to the right
task type for mixed-type DAGs (e.g. an image task feeding an LLM task).

### Pluggable strategy interfaces

Nearly every cross-cutting concern is an abstract `ns3::Object` interface with one or more concrete
implementations selected via ns-3 Attributes (`PointerValue`), following the standard ns-3
Attribute/TypeId pattern throughout:

| Interface | Implementations | Concern |
|---|---|---|
| `ConnectionManager` | `TcpConnectionManager` (pooled), `UdpConnectionManager` | transport, used identically by client/orchestrator/backend |
| `ClusterScheduler` | `RoundRobinScheduler`, `LeastLoadedScheduler`, `NetworkAwareScheduler`, `TieredDelayScheduler` | which backend gets a dispatched task |
| `AdmissionPolicy` | `MaxActiveTasksPolicy` | whether the orchestrator admits a workload |
| `ClientSubmissionPolicy` | `AlwaysSubmitPolicy`, `DropWhilePendingSubmissionPolicy` | whether a client submits on a given generation tick |
| `Accelerator` | `GpuAccelerator` | hardware execution, owns a `ProcessingModel` + `EnergyModel` |
| `ProcessingModel` | `FixedRatioProcessingModel` | compute-demand → duration |
| `EnergyModel` | `DvfsEnergyModel` | energy accounting, DVFS-aware |
| `QueueScheduler` | `FifoQueueScheduler` | task queue ordering on an accelerator |
| `ScalingPolicy` | `UtilizationScalingPolicy`, `ConservativeScalingPolicy` | DVFS performance-state transitions, driven by `DeviceManager` |

When implementing a new strategy, look at the sibling implementation(s) in the same file group first —
they define the exact virtual interface contract and constructor/Attribute conventions to match.

`NetworkAwareScheduler` and `TieredDelayScheduler` factor communication cost (edge vs. cloud
placement) into scheduling. `Cluster::Backend` carries static `commDelay`/`commCapacity`
(topology-configured); `ClusterState::BackendState::Network` carries an optional live-measured
delay, populated only when `Orchestrator`'s `EnableNetworkDelayTracking` attribute is on (default
off) — schedulers should prefer the live value when `network.delayValid` and fall back to the
static one, matching the existing pattern in `NetworkAwareScheduler`.

### Component ownership

- `OffloadClient`/`BackendServer`/`Orchestrator` are ns-3 `Application`s (installed on nodes,
  `StartApplication`/`StopApplication` lifecycle).
- `Orchestrator` owns two `ConnectionManager`s (one listening for clients, one outgoing to backends),
  a `Cluster`, a `ClusterState` (per-backend load/metrics), and optionally a `DeviceManager` for DVFS
  scaling. Per-address receive buffers (`std::map<Address, Ptr<Packet>>`) handle TCP stream
  reassembly; message boundaries are determined by each header's own `GetSerializedHeaderSize()` /
  payload-length field, not by relying on Packet boundaries.
- Every application/model class exposes `TracedCallback`s for its key lifecycle events (task sent,
  admitted, rejected, dispatched, completed, dropped, state changed, etc.) — this is the primary
  instrumentation surface for building simulation output/statistics; wire new observability through
  traces rather than ad hoc logging.
- Helpers (`helper/*Helper`) are thin `Install()`-style wrappers over the model classes, matching
  standard ns-3 helper conventions.

### File conventions

- One class per `.h`/`.cc` pair in `model/` or `helper/`; both are always registered together in
  `CMakeLists.txt` under `SOURCE_FILES`/`HEADER_FILES`.
- `model/continuum.h` is the umbrella include aggregating every public header, grouped by subsystem
  (Task, Accelerator, Queue management, Connection manager, Orchestration, Device management, Client
  submission policies, Applications, Helpers) — add new headers to the matching group there.
- Copyright/license header on every file: `Copyright (c) 2025 UCC`, `SPDX-License-Identifier:
  GPL-2.0-only`, author line. Match this on new files.
- Doxygen is used for public API docs (`@ingroup continuum`, `@brief`, `@param`/`@return`), but keep it
  proportionate — a prior change explicitly trimmed "excessive Doxygen" from `TcpConnectionManager`;
  don't add verbose comment blocks that just restate the signature.
