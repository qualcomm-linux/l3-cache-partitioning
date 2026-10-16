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

### Option A — via `meta-qcom-realtime` (recommended for RT builds)

The recipe is maintained in the
[qualcomm-linux/meta-qcom-realtime](https://github.com/qualcomm-linux/meta-qcom-realtime)
layer. If your build already includes that layer, add the module to your image:

```bitbake
# In your image recipe or bbappend
IMAGE_INSTALL:append = " qcom-cache-partition"
```

Also ensure the kernel config fragment enables ConfigFS:

```
CONFIG_CONFIGFS_FS=y
```

The module will load automatically at boot via `systemd-modules-load.service`.

### Option B — standalone layer

This repository is itself a valid Yocto layer. Add it to `bblayers.conf`:

```
BBLAYERS += "/path/to/l3-cache-partitioning"
```

Then add to your image:

```bitbake
IMAGE_INSTALL:append = " qcom-cache-partition"
```

The layer depends on `core` and `qcom` (from `meta-qcom`).

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
