# l3-cache-partitioning

Out-of-tree Linux kernel module for software-defined L3 cache partitioning on
Qualcomm IoT platforms (QCS8300 / QCS9100).

## Overview

The module programs two Qualcomm-proprietary EL1-accessible CPU system registers
directly from Linux kernel space — no TrustZone or SMC calls required:

| Register | Encoding | Purpose |
|---|---|---|
| `CLUSTERPARTCR_EL1` | `sys_reg(3, 0, 15, 4, 3)` | Per-cluster L3 way assignment |
| `CLUSTER_THREAD_SID_EL1` | `sys_reg(3, 0, 15, 4, 0)` | Per-thread stream ID |

A ConfigFS subsystem at `/sys/kernel/config/cache_partition/` exposes two
attributes for reading and writing these registers.

## Supported Platforms

| SoC | Status |
|---|---|
| QCS8300 | Supported |
| QCS9100 | Supported |

## Prerequisites

Three conditions must be met for the module to function correctly.

### 1. `CONFIG_CONFIGFS_FS=y`

The module's userspace interface is built on ConfigFS. If your kernel does not
have it enabled, `insmod` will succeed but the `/sys/kernel/config/cache_partition/`
directory will not appear.

**How to satisfy:**

Check whether your running kernel already has it:

```sh
zcat /proc/config.gz | grep CONFIG_CONFIGFS_FS
```

If not, add a kernel config fragment to your BSP layer:

```
# recipes-kernel/linux/files/qcom-cache-partition.cfg
CONFIG_CONFIGFS_FS=y
```

Then append it via a bbappend:

```bitbake
# recipes-kernel/linux/<kernel>_%.bbappend
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append = " file://qcom-cache-partition.cfg"
```

### 2. `cpuidle.off=1`

`CLUSTERPARTCR_EL1` is a cluster-level register that is re-initialized by the
hardware on cluster power-down. If the CPU idle driver is allowed to power-gate
the cluster, register values written by the module are silently lost on the next
idle exit — cache partitioning appears to work but has no effect after the first
idle transition.

**How to satisfy (choose one):**

- **Yocto — `meta-qcom-realtime` (public, scarthgap):** The layer's
  `conf/layer.conf` sets `cpuidle.off=1` in `KERNEL_CMDLINE_EXTRA` for
  `qcs8300`, `qcs9100`, and `qcm6490` machines automatically.

- **Yocto — manual:** Add to your machine config or `local.conf`:
  ```bitbake
  KERNEL_CMDLINE_EXTRA:append = " cpuidle.off=1"
  ```

- **Non-Yocto:** Add `cpuidle.off=1` to your bootloader kernel command line
  (GRUB, EFI, or U-Boot `bootargs`).

### 3. Host KVM (EL2 initialisation)

`CLUSTERPARTCR_EL1` is a Qualcomm vendor register that is only accessible from
EL1 after the CPU has been initialized at EL2. If the system boots without KVM
host support, EL2 is not initialized, and any `mrs`/`msr` to this register will
fault — the module will load successfully but the first write to ConfigFS will
trigger a kernel exception.

**How to satisfy:**

- **Yocto — QCS8300/QCS9100 EVKs:** The `iq-9075-evk.conf` / equivalent machine
  config in `meta-qcom` already includes the EL2 DTB variant
  (e.g. `lemans-evk-el2.dtb`) in `KERNEL_DEVICETREE`. The bootloader selects it
  automatically from the multi-DTB image.

- **Yocto — custom board:** Add the EL2 DTB for your board to
  `KERNEL_DEVICETREE` in your machine config:
  ```bitbake
  KERNEL_DEVICETREE:append = " qcom/<board>-el2.dtb"
  ```
  and ensure `CONFIG_KVM=y` is set in your kernel config.

- **Non-Yocto:** Boot with your board's EL2 DTB selected in the bootloader.

## Build Requirements

- Linux kernel headers ≥ 6.6 (arm64)
- `CONFIG_CONFIGFS_FS=y` in the target kernel (see Prerequisites)
- GCC or Clang cross-compiler for arm64

## Yocto Integration

### Using meta-qcom (primary path)

The Yocto recipe for this module lives in
[qualcomm-linux/meta-qcom](https://github.com/qualcomm-linux/meta-qcom) under
`recipes-kernel/qcom-cache-partition/`. If `meta-qcom` is already in your
`bblayers.conf` (standard Qualcomm Linux builds), no extra layer is needed.

**Step 1 — Enable ConfigFS in your kernel config fragment** (see Prerequisites §1).

**Step 2 — Add the module to your image recipe or bbappend:**

```bitbake
IMAGE_INSTALL:append = " qcom-cache-partition"
```

**Step 3 — Satisfy cpuidle and KVM prerequisites** (see Prerequisites §2 and §3).

The module loads automatically at boot via `systemd-modules-load.service`.

### Standalone recipe (builds without meta-qcom)

If `meta-qcom` is not in your layer stack, copy the recipe directly into your
own layer.

Create `recipes-kernel/qcom-cache-partition/qcom-cache-partition_1.0.bb`:

```bitbake
# SPDX-License-Identifier: GPL-2.0-only
SUMMARY = "Qualcomm L3 cache partition kernel module"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=a362c03f8df4e23f8a155e1cb2c45b2a"

inherit module

SRC_URI = "git://github.com/rrevdeka/l3-cache-partitioning.git;protocol=https;branch=main"
SRCREV = "1eb5f683c15db85d92c19c96edcc7664b084f4d3"

PV = "1.0+git"
S = "${WORKDIR}/git"

COMPATIBLE_MACHINE = "^$"
COMPATIBLE_MACHINE:aarch64 = "(.*)"

KERNEL_MODULE_AUTOLOAD += "qcom-cache-partition"
RPROVIDES:${PN} += "kernel-module-qcom-cache-partition"
PACKAGE_ARCH = "${MACHINE_ARCH}"
```

Then follow Steps 1–3 above.

## Quick Start (native / cross build)

```sh
# Cross-compile
make KDIR=/path/to/kernel ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

# Install
sudo make KDIR=/lib/modules/$(uname -r)/build modules_install
sudo depmod -a

# Load
sudo modprobe qcom-cache-partition

# Mount configfs if not already mounted
mount -t configfs none /sys/kernel/config
```

## Usage

```sh
# Read current state
cat /sys/kernel/config/cache_partition/partcr_el1

# Assign ways 0xFF to cluster 1 (RT cluster)
echo "ff 1" > /sys/kernel/config/cache_partition/partcr_el1

# Assign stream ID 1 to CPU 7
echo "1 7" > /sys/kernel/config/cache_partition/thread_sid_el1
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

GPL-2.0-only. See [LICENSE.txt](LICENSE.txt).
