# Architecture

## Overview

The `qcom-cache-partition` module provides software-defined L3 cache partitioning
for real-time workloads on Qualcomm QCS8300 and QCS9100 SoCs.

## Register Interface

The module programs two Qualcomm-proprietary system registers accessible from
Linux kernel space at EL1 (no SMC/TrustZone involvement):

### CLUSTERPARTCR_EL1 — `sys_reg(3, 0, 15, 4, 3)`

Per-cluster L3 way assignment register. Controls which L3 cache ways are
available to each CPU cluster.

### CLUSTER_THREAD_SID_EL1 — `sys_reg(3, 0, 15, 4, 0)`

Per-thread stream ID register. Controls the stream ID associated with each
CPU thread for cache partition lookup.

## ConfigFS Interface

The module registers a ConfigFS subsystem at:

```
/sys/kernel/config/cache_partition/
├── partcr_el1        # CLUSTERPARTCR_EL1 per-cluster read/write
└── thread_sid_el1    # CLUSTER_THREAD_SID_EL1 per-CPU read/write
```

ConfigFS was chosen over sysfs because it provides per-subsystem locking
and a clean group-based namespace without requiring a platform device.

## CPU Topology

Cluster membership is determined dynamically via `topology_cluster_id(cpu)`
(from `include/linux/arch_topology.h`, available since Linux 5.16). This
avoids hardcoded cluster-to-CPU mappings and works correctly across different
QCS8300/QCS9100 SKUs.

## Concurrency

- A spinlock (`partcr_lock`) serializes concurrent writes to CLUSTERPARTCR_EL1
  across callers on different CPUs.
- Register reads and writes are performed on the target CPU via
  `smp_call_function_single()` to ensure the correct hardware thread executes
  the `msr`/`mrs` instruction.
- `cpu_online()` is checked before dispatching to any CPU.

## Comparison with NXP i.MX93

On NXP i.MX93, cache partition registers (`PARTCR`, `THREADSID`) are EL3-protected
and require `arm_smccc_smc(0x8400ff04/05, ...)` calls through TrustZone firmware.
On Qualcomm QCS8300/QCS9100, the equivalent registers (`CLUSTERPARTCR_EL1`,
`CLUSTER_THREAD_SID_EL1`) carry the `_EL1` suffix and are directly accessible from
Linux kernel space, eliminating the EL3 firmware dependency.
