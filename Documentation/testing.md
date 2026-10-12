# Testing

## Build Validation

```sh
make KDIR=/lib/modules/$(uname -r)/build W=1
```

Check for zero warnings with `W=1`. Run checkpatch on any new patches:

```sh
git format-patch HEAD~1
scripts/checkpatch.pl --strict 0001-*.patch
```

## Module Load/Unload

```sh
sudo modprobe qcom-cache-partition
lsmod | grep qcom_cache_partition
sudo modprobe -r qcom-cache-partition
```

## Functional Smoke Test

See `tests/smoke/test_cache_partition.sh` for an automated smoke test.

Manual steps:

```sh
mount -t configfs none /sys/kernel/config

# Verify attributes exist
ls /sys/kernel/config/cache_partition/

# Read baseline
cat /sys/kernel/config/cache_partition/partcr_el1
cat /sys/kernel/config/cache_partition/thread_sid_el1

# Write and verify (QCS8300: cluster 0 = efficiency, cluster 1 = performance)
echo "f0 0" > /sys/kernel/config/cache_partition/partcr_el1
echo "0f 1" > /sys/kernel/config/cache_partition/partcr_el1
cat /sys/kernel/config/cache_partition/partcr_el1

# Invalid input should return EINVAL
echo "zz 99" > /sys/kernel/config/cache_partition/partcr_el1 && echo "FAIL: expected error" || echo "PASS: got expected error"
```

## RT Latency Validation

Use `rt_benchmark` (from `meta-qcom-rt/recipes-rt/rt-benchmark/`) to measure
worst-case pointer-chase latency before and after partitioning:

```sh
# Baseline (no partitioning)
rt_benchmark 512 32 0 80

# With partitioning active
rt_benchmark 512 32 0 80
```

Expected: ≥35% reduction in worst-case latency and stddev under memory stress.

## CPU Hotplug

Verify the driver handles CPU hotplug correctly:

```sh
# Take a CPU offline while the module is loaded
echo 0 > /sys/devices/system/cpu/cpu3/online

# Attempt to write to the offlined CPU — should return EINVAL
echo "1 3" > /sys/kernel/config/cache_partition/thread_sid_el1 && echo "FAIL" || echo "PASS"

# Bring CPU back online
echo 1 > /sys/devices/system/cpu/cpu3/online
```
