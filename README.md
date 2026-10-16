# l3-cache-partitioning

Out-of-tree Linux kernel module for software-defined L3 cache partitioning on
Qualcomm IoT platforms (QCS8300 / QCS9100). Implements FR203643.

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

## Build Requirements

- Linux kernel headers ≥ 6.6 (arm64)
- `CONFIG_CONFIGFS_FS=y` in the target kernel
- GCC or Clang cross-compiler for arm64

## Yocto Integration

The Yocto recipe lives in
[qualcomm-linux/meta-qcom-realtime](https://github.com/qualcomm-linux/meta-qcom-realtime),
which targets the **scarthgap** Yocto release.

> **Note:** `meta-qcom-realtime` does more than provide this module — it also
> sets RT kernel providers, kernel command-line parameters, and appends several
> RT packages to the image. If you only want the cache partition module without
> those changes, follow the standalone recipe path below.

### Using meta-qcom-realtime (scarthgap builds)

**Step 1 — Add the layer to `bblayers.conf`:**

```
BBLAYERS += "/path/to/meta-qcom-realtime"
```

**Step 2 — Enable ConfigFS in your kernel config fragment.**

Create a `.cfg` file in your layer (e.g. `recipes-kernel/linux/files/qcom-cache-partition.cfg`):

```
CONFIG_CONFIGFS_FS=y
```

Then append it to your kernel recipe via a bbappend
(`recipes-kernel/linux/linux-qcom-rt_%.bbappend`):

```bitbake
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append = " file://qcom-cache-partition.cfg"
```

**Step 3 — Add the module to your image recipe or bbappend:**

```bitbake
IMAGE_INSTALL:append = " qcom-cache-partition"
```

The module loads automatically at boot via `systemd-modules-load.service`.

### Standalone recipe (other Yocto releases)

If you are not on scarthgap or do not want to pull in the full
`meta-qcom-realtime` layer, copy the recipe directly into your own layer.

Create `recipes-kernel/qcom-cache-partition/qcom-cache-partition_1.0.bb`:

```bitbake
# SPDX-License-Identifier: GPL-2.0-only
DESCRIPTION = "Qualcomm L3 Cache Partitioning OOT kernel module (FR203643)"
HOMEPAGE = "https://github.com/rrevdeka/l3-cache-partitioning"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://LICENSE.txt;md5=a362c03f8df4e23f8a155e1cb2c45b2a"

SRC_URI = "git://github.com/rrevdeka/l3-cache-partitioning.git;protocol=https;branch=main"
SRCREV = "b2041aa0361c1cb848ebbc43555cf63e674b283d"

inherit module

KERNEL_MODULE_AUTOLOAD += "qcom-cache-partition"
RPROVIDES:${PN} += "kernel-module-qcom-cache-partition"

COMPATIBLE_MACHINE = "^$"
COMPATIBLE_MACHINE:aarch64 = "(.*)"

PACKAGE_ARCH = "${MACHINE_ARCH}"
```

Then follow Steps 2 and 3 above (kernel config fragment + `IMAGE_INSTALL`).

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
