# Configuration Interface

## Mount ConfigFS

```sh
mount -t configfs none /sys/kernel/config
```

## partcr_el1

Controls `CLUSTERPARTCR_EL1` — per-cluster L3 way assignment.

**Read:**
```sh
cat /sys/kernel/config/cache_partition/partcr_el1
# Cluster 0: 0x0
# Cluster 1: 0x0
```

**Write:** `echo "<hex_value> <cluster_id>" > partcr_el1`

| Field | Type | Description |
|---|---|---|
| `hex_value` | u32 hex | Value to write to CLUSTERPARTCR_EL1 |
| `cluster_id` | int | Target cluster (0-based, from `topology_cluster_id()`) |

Example — reserve ways for cluster 1:
```sh
echo "ff 1" > /sys/kernel/config/cache_partition/partcr_el1
```

**Errors:**
- `EINVAL`: wrong format, or cluster has no online CPU
- `ENOMEM`: temporary allocation failure during read

## thread_sid_el1

Controls `CLUSTER_THREAD_SID_EL1` — per-thread stream ID.

**Read:**
```sh
cat /sys/kernel/config/cache_partition/thread_sid_el1
# CPU0: 0x0
# CPU1: 0x0
# ...
# CPU7: 0x0 [offline]
```

**Write:** `echo "<hex_value> <cpu_num>" > thread_sid_el1`

| Field | Type | Description |
|---|---|---|
| `hex_value` | u32 hex | Stream ID to assign |
| `cpu_num` | int | Target CPU number (must be online) |

Example — assign stream ID 1 to CPU 7:
```sh
echo "1 7" > /sys/kernel/config/cache_partition/thread_sid_el1
```

**Errors:**
- `EINVAL`: wrong format, CPU out of range, or CPU offline
- `ENOMEM`: temporary allocation failure during read
