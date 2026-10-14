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

See [Documentation/supported-kernels.md](Documentation/supported-kernels.md) for
kernel version compatibility.

## Build Requirements

- Linux kernel headers ≥ 6.6 (arm64)
- `CONFIG_CONFIGFS_FS=y` in the target kernel
- GCC or Clang cross-compiler for arm64

## Quick Start

```sh
# Set KDIR to your kernel build directory or installed headers
make KDIR=/lib/modules/$(uname -r)/build

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
# Cluster 0: 0x0
# Cluster 1: 0x0

# Assign ways 0xFF to cluster 1 (RT cluster)
echo "ff 1" > /sys/kernel/config/cache_partition/partcr_el1

# Assign stream ID 1 to CPU 7
echo "1 7" > /sys/kernel/config/cache_partition/thread_sid_el1
```

See [Documentation/integration-guide.md](Documentation/integration-guide.md) for
the full integration guide covering Yocto, standalone cross-compilation, and
native build workflows.

See [Documentation/configuration.md](Documentation/configuration.md) for full
interface documentation and [Documentation/testing.md](Documentation/testing.md)
for validation instructions.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

GPL-2.0-only. See [LICENSE.txt](LICENSE.txt).
