# Supported Kernels

## Requirements

| Requirement | Minimum Version | Notes |
|---|---|---|
| Linux kernel | 6.6 | `topology_cluster_id()` stable since 5.16 |
| `CONFIG_CONFIGFS_FS` | — | Must be `=y` (not `=m`) |
| Architecture | arm64 | Qualcomm-specific EL1 registers |

## Tested Baselines

| Kernel | Status |
|---|---|
| QLI Day1 (Linux 7.3-rc2) | Tested |
| Linux 6.6 LTS | Compatible |

## Compatibility Notes

The module depends only on:

- `configfs` kernel subsystem API (stable across versions)
- `topology_cluster_id()` from `include/linux/arch_topology.h` (stable since 5.16)
- Standard ARM64 `read_sysreg_s()` / `write_sysreg_s()` macros

**Per kernel release:** verify `topology_cluster_id()` and `configfs_register_subsystem()`
API signatures are unchanged, then rebuild. Expected maintenance cost: near zero.
