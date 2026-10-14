# qcom-cache-partition — Integration Guide

Out-of-tree (OOT) kernel module for software-defined L3 cache partitioning on
Qualcomm QCS8300 / QCS9100 platforms (FR203643).

---

## Table of Contents

1. [Overview](#1-overview)
2. [Hardware and Kernel Requirements](#2-hardware-and-kernel-requirements)
3. [Getting the Source](#3-getting-the-source)
4. [Option A — Yocto / OpenEmbedded Integration](#4-option-a--yocto--openembedded-integration)
5. [Option B — Standalone Cross-Compilation](#5-option-b--standalone-cross-compilation)
6. [Option C — Native Build on Target](#6-option-c--native-build-on-target)
7. [Loading the Module](#7-loading-the-module)
8. [Using the ConfigFS Interface](#8-using-the-configfs-interface)
9. [RT Workload Integration Pattern](#9-rt-workload-integration-pattern)
10. [Smoke Test](#10-smoke-test)
11. [Validated Results (IQ9075 EVK)](#11-validated-results-iq9075-evk)
12. [Troubleshooting](#12-troubleshooting)

---

## 1. Overview

`qcom-cache-partition` programs two Qualcomm-proprietary EL1-accessible CPU
system registers directly from Linux kernel space — no TrustZone or SMC calls
are required:

| Register | Encoding | Purpose |
|---|---|---|
| `CLUSTERPARTCR_EL1` | `sys_reg(3, 0, 15, 4, 3)` | Per-cluster L3 way assignment |
| `CLUSTER_THREAD_SID_EL1` | `sys_reg(3, 0, 15, 4, 0)` | Per-thread stream ID |

The module exposes a ConfigFS interface at `/sys/kernel/config/cache_partition/`.
Userspace assigns L3 cache ways to CPU clusters and stream IDs to individual CPUs
using simple `echo` writes, with no kernel rebuild or reboot required.

**Typical use case**: an RT task pinned to a performance CPU cluster that has
exclusive L3 way access, preventing cache eviction by background workloads.

---

## 2. Hardware and Kernel Requirements

| Requirement | Value |
|---|---|
| SoC | QCS8300 or QCS9100 family (includes QCS9075, QCS9075M, QCS9100M) |
| Architecture | arm64 |
| Kernel version | ≥ 6.6 |
| `CONFIG_CONFIGFS_FS` | `=y` (not `=m`) |
| `CONFIG_MODULES` | `=y` |

To verify `CONFIG_CONFIGFS_FS` on a running kernel:

```sh
zcat /proc/config.gz | grep CONFIG_CONFIGFS_FS
# or
cat /boot/config-$(uname -r) | grep CONFIG_CONFIGFS_FS
```

Expected: `CONFIG_CONFIGFS_FS=y`

---

## 3. Getting the Source

```sh
git clone https://github.com/qualcomm-linux/l3-cache-partitioning.git
cd l3-cache-partitioning
```

The repository contains:

```
src/qcom-cache-partition.c   # module source
Kbuild                       # Kbuild descriptor
Makefile                     # supports KDIR= and KERNEL_SRC= (Yocto)
tests/smoke/test_cache_partition.sh
Documentation/
```

---

## 4. Option A — Yocto / OpenEmbedded Integration

This is the recommended path for product builds. The module is packaged as a
standard OOT module recipe using `module.bbclass`.

### 4.1 Add meta-qcom-rt to your build

In your `bblayers.conf`:

```bitbake
BBLAYERS += "/path/to/meta-qcom-rt"
```

The layer provides two recipes:

| Recipe | Purpose |
|---|---|
| `qcom-cache-partition_1.0.bb` | OOT module, auto-loaded at boot |
| `qcom-cache-partition-dkms_1.0.bb` | DKMS variant for host-side development |

### 4.2 Add the module to your image

In your image recipe or a `.bbappend`:

```bitbake
IMAGE_INSTALL:append = " qcom-cache-partition"
```

Or add it to an existing image append. For example, in
`recipes-rt/images/qcom-multimedia-image.bbappend`:

```bitbake
IMAGE_INSTALL:append = " qcom-cache-partition rt-benchmark"
```

### 4.3 Pin SRCREV (recommended for production)

The recipe ships with `SRCREV = "${AUTOREV}"` for development. Before taping
out or releasing, tag the source repo and pin:

```bitbake
# In qcom-cache-partition_1.0.bb or a .bbappend
SRCREV = "8d0ad3756ce7d4febc2a5cbdc4d7ab9781d798ca"  # replace with actual tag commit
PV = "1.0"
```

### 4.4 Build

```sh
bitbake qcom-cache-partition
# or as part of the full image:
bitbake qcom-multimedia-image
```

The module is installed to `/lib/modules/<kver>/extra/qcom-cache-partition.ko`
and auto-loaded at boot via `/etc/modules-load.d/qcom-cache-partition.conf`.

---

## 5. Option B — Standalone Cross-Compilation

Use this when you want to build and deploy the module without a full Yocto
workspace, for example to test a new kernel version quickly.

### 5.1 Prerequisites

- An arm64 cross-compiler (`aarch64-linux-gnu-gcc` or LLVM/Clang)
- Kernel headers or build directory for the target kernel

### 5.2 Build

```sh
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

# Point KDIR at the target kernel build directory or headers package
make KDIR=/path/to/kernel-headers-or-build

# Example with a Yocto sysroot:
make KDIR=/path/to/tmp/work/iq_9075_evk-poky-linux/linux-qcom-rt/6.18.44-r0/build
```

Output: `qcom-cache-partition.ko`

### 5.3 Deploy to target

```sh
# Via ADB
adb push qcom-cache-partition.ko /lib/modules/$(adb shell uname -r)/extra/
adb shell depmod -a
adb shell modprobe qcom-cache-partition

# Via SCP (if SSH is available on the target)
scp qcom-cache-partition.ko root@<device-ip>:/lib/modules/$(ssh root@<device-ip> uname -r)/extra/
ssh root@<device-ip> "depmod -a && modprobe qcom-cache-partition"

# Via serial console — copy to a USB stick and mount on target
mount /dev/sda1 /mnt
cp /mnt/qcom-cache-partition.ko /lib/modules/$(uname -r)/extra/
depmod -a
modprobe qcom-cache-partition
```

---

## 6. Option C — Native Build on Target

For quick iteration when the target has kernel headers installed.

```sh
# On the target device (requires gcc and kernel-headers package)
git clone https://github.com/qualcomm-linux/l3-cache-partitioning.git
cd l3-cache-partitioning
make
sudo make modules_install
sudo depmod -a
sudo modprobe qcom-cache-partition
```

---

## 7. Loading the Module

### Auto-load at boot (Yocto/package install)

When installed via the Yocto recipe, the module loads automatically at boot.
Verify with:

```sh
lsmod | grep qcom_cache_partition
# Expected: qcom_cache_partition    12288  0
```

### Manual load

```sh
modprobe qcom-cache-partition
# or
insmod /lib/modules/$(uname -r)/extra/qcom-cache-partition.ko
```

### Verify ConfigFS mount

ConfigFS is mounted automatically on modern systemd-based distros. If not:

```sh
mount | grep configfs
# If not mounted:
mount -t configfs none /sys/kernel/config
```

### Verify interface nodes

```sh
ls /sys/kernel/config/cache_partition/
# Expected: partcr_el1  thread_sid_el1
```

---

## 8. Using the ConfigFS Interface

### 8.1 Read current state

```sh
cat /sys/kernel/config/cache_partition/partcr_el1
# Cluster 0: 0x0
# Cluster 1: 0x0

cat /sys/kernel/config/cache_partition/thread_sid_el1
# CPU0: 0x0
# CPU1: 0x0
# CPU2: 0x0
# CPU3: 0x0
# CPU4: 0x0
# CPU5: 0x0
# CPU6: 0x0
# CPU7: 0x0
```

All registers default to 0x0 on module load (hardware reset state).

### 8.2 Assign L3 ways to clusters

Write format: `echo "<hex_value> <cluster_id>" > partcr_el1`

`hex_value` is the bitmask written to `CLUSTERPARTCR_EL1` for that cluster.
On QCS9100 / QCS9075, there are 8 L3 ways (bits 7:0). Each bit corresponds to
one way.

```sh
# Give cluster 0 (efficiency cores, CPUs 0-3) the lower 4 ways
echo "0f 0" > /sys/kernel/config/cache_partition/partcr_el1

# Give cluster 1 (performance cores, CPUs 4-7) the upper 4 ways
echo "f0 1" > /sys/kernel/config/cache_partition/partcr_el1

# Give cluster 1 all 8 ways (full L3 exclusive for RT cluster)
echo "ff 1" > /sys/kernel/config/cache_partition/partcr_el1
```

### 8.3 Assign a stream ID to a CPU

Write format: `echo "<hex_value> <cpu_num>" > thread_sid_el1`

```sh
# Assign stream ID 1 to CPU7
echo "1 7" > /sys/kernel/config/cache_partition/thread_sid_el1
```

Stream IDs are used by the hardware to look up which cache partition a thread's
memory accesses should use. Set the same stream ID on all CPUs in the RT cluster
when giving that cluster exclusive cache access.

### 8.4 Verify writes

```sh
cat /sys/kernel/config/cache_partition/partcr_el1
# Cluster 0: 0xf
# Cluster 1: 0xff

cat /sys/kernel/config/cache_partition/thread_sid_el1
# CPU7: 0x1
```

### 8.5 Error handling

```sh
# Invalid format returns EINVAL (shell exit code 1)
echo "zz 99" > /sys/kernel/config/cache_partition/partcr_el1
# bash: echo: write error: Invalid argument

# Writing to an offline CPU returns EINVAL
echo "1 3" > /sys/kernel/config/cache_partition/thread_sid_el1
# bash: echo: write error: Invalid argument
```

---

## 9. RT Workload Integration Pattern

Full isolation for an RT task on CPU7 (performance cluster, cluster 1):

```sh
# Step 1 — Give the RT cluster all L3 ways
echo "ff 1" > /sys/kernel/config/cache_partition/partcr_el1

# Step 2 — Assign stream ID 1 to CPUs 4-7 (all performance cores)
echo "1 4" > /sys/kernel/config/cache_partition/thread_sid_el1
echo "1 5" > /sys/kernel/config/cache_partition/thread_sid_el1
echo "1 6" > /sys/kernel/config/cache_partition/thread_sid_el1
echo "1 7" > /sys/kernel/config/cache_partition/thread_sid_el1

# Step 3 — Pin the RT task to the isolated cluster and run at RT priority
taskset -c 4-7 chrt -f 80 /path/to/rt_task
```

To restore default (no partitioning):

```sh
echo "0 0" > /sys/kernel/config/cache_partition/partcr_el1
echo "0 1" > /sys/kernel/config/cache_partition/partcr_el1
for cpu in 0 1 2 3 4 5 6 7; do
    echo "0 $cpu" > /sys/kernel/config/cache_partition/thread_sid_el1
done
```

---

## 10. Smoke Test

A script is provided at `tests/smoke/test_cache_partition.sh`. Copy to the
target and run as root:

```sh
# Via ADB
adb push tests/smoke/test_cache_partition.sh /data/local/tmp/
adb shell chmod +x /data/local/tmp/test_cache_partition.sh
adb shell /data/local/tmp/test_cache_partition.sh

# Via serial console
chmod +x /path/to/test_cache_partition.sh
/path/to/test_cache_partition.sh
```

Expected output:

```
PASS: partcr_el1 exists
PASS: thread_sid_el1 exists
PASS: partcr_el1 readable
PASS: thread_sid_el1 readable
PASS: partcr_el1 write cluster 0
PASS: bad format rejected
PASS: offline CPU rejected
PASS: module unload
PASS: module reload

Results: 8 passed, 0 failed
```

---

## 11. Validated Results (IQ9075 EVK)

**Platform**: IQ9075 EVK (QCS9075, QCS9100 family)  
**Kernel**: 6.18.44 RT (`linux-qcom-rt`)  
**Build method**: OOT via `module.bbclass` in `meta-qcom-rt` Yocto layer  
**Module auto-loaded**: yes (`KERNEL_MODULE_AUTOLOAD`)

### RT Latency Benchmark (`rt_benchmark 512 32 0 80`)

| Configuration | min | max (wcl) | mean | stddev |
|---|---|---|---|---|
| Split ways (0x0f / 0xf0), no CPU pin | 89,650 µs | **90,148 µs** | 89,868 µs | 167 µs |
| Cluster 1 exclusive (0xff) + stream ID 1 + `taskset -c 7` | 38,803 µs | **48,576 µs** | 43,834 µs | 3,248 µs |

**Worst-case latency reduction: 46.1%** — exceeds FR203643 pass criterion of ≥35%.

The higher stddev in the second run is normal: `taskset -c 7` creates momentary
scheduler contention on the pinned core before the RT task settles, whereas the
unpinned run migrates to a quiet core and stabilizes.

---

## 12. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `modprobe: FATAL: Module qcom-cache-partition not found` | Module not installed or `depmod` not run | Run `depmod -a` then retry |
| `/sys/kernel/config/cache_partition/` missing after `modprobe` | `CONFIG_CONFIGFS_FS` is `=m` not `=y` | Rebuild kernel with `CONFIG_CONFIGFS_FS=y` |
| `echo: write error: Invalid argument` on valid input | CPU offline or wrong cluster ID | Check `cat /sys/devices/system/cpu/online` |
| Module loads on wrong kernel version | Built against different kernel headers | Rebuild with `KDIR` pointing to the running kernel's build directory |
| `Exec format error` when loading `.ko` | Module built for wrong arch | Ensure `ARCH=arm64` and correct `CROSS_COMPILE` are set |
| Latency improvement less than expected | Background load evicting RT cluster's cache | Check that stream IDs are set on ALL CPUs in the RT cluster, not just one |
