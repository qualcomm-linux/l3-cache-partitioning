# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

## [1.0.0] - 2026-11-24

### Added
- Initial public release of the QCOM L3 cache partitioning kernel module
- ConfigFS interface at `/sys/kernel/config/cache_partition/` for per-cluster L3 cache way assignment
- Per-thread stream ID configuration via `CLUSTER_THREAD_SID_EL1`
- `cpu_pm` notifier to restore register state after cluster power-gate
- Smoke test script for validating ConfigFS interface
- GitHub Actions CI: kernel build (arm64), copyright/license check, DCO enforcement
