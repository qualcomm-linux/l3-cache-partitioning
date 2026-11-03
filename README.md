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

Both registers are reset by hardware on cluster power-gate. The module registers
a `cpu_pm` notifier that automatically restores both registers on `CPU_PM_EXIT`,
so partition assignments survive CPU idle transitions.

## Supported Platforms

| SoC | Status |
|---|---|
| QCS8300 | Supported |
| QCS9100 | Supported |

## Prerequisites

Two conditions must be met for the module to function correctly.

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

### 2. EL2 / VHE host (bare-metal KVM)

`CLUSTERPARTCR_EL1` and `CLUSTER_THREAD_SID_EL1` are trapped by both the
Gunyah hypervisor and by the KVM EL2 host when accessed from a VM guest. The
module must run on bare-metal Linux at EL2 in VHE mode (i.e. as the KVM host,
not as a guest).

**Standard QCS8300 / QCS9100 builds satisfy this automatically**: the
`xbl_config.elf` shipped by `qcomflash` is the KVM variant, which initializes
the CPU in VHE mode with Linux running at EL2.

If you are running Linux inside a Gunyah or KVM guest VM, register accesses
will trap and the module will not function.

## Build Requirements

- Linux kernel headers ≥ 6.6 (arm64)
- `CONFIG_CONFIGFS_FS=y` in the target kernel (see Prerequisites §1)
- Linux running at EL2 as VHE host — satisfied by default on QCS8300/QCS9100 (see Prerequisites §2)
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

SRC_URI = "git://github.com/qualcomm-linux/l3-cache-partitioning.git;protocol=https;branch=main"
SRCREV = "1eb5f683c15db85d92c19c96edcc7664b084f4d3"

PV = "1.0+git"

COMPATIBLE_MACHINE = "^$"
COMPATIBLE_MACHINE:aarch64 = "(.*)"

KERNEL_MODULE_AUTOLOAD += "qcom-cache-partition"
RPROVIDES:${PN} += "kernel-module-qcom-cache-partition"
PACKAGE_ARCH = "${MACHINE_ARCH}"
```

Then follow Steps 1–2 above.

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
