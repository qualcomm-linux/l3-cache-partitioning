// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Qualcomm Cache Partition Control via ConfigFS (FR203643)
 *
 * Programs CLUSTERPARTCR_EL1 (per-cluster way assignment) and
 * CLUSTER_THREAD_SID_EL1 (per-thread stream ID) directly at EL1 —
 * no TrustZone / SMC involved.
 *
 * Interface: /sys/kernel/config/cache_partition/
 *   partcr_el1      read : Cluster <N>: 0x<value>
 *                   write: <hex_value> <cluster_id>
 *   thread_sid_el1  read : CPU<N>: 0x<value>
 *                   write: <hex_value> <cpu_num>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/configfs.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/topology.h>
#include <linux/cpumask.h>
#include <asm/sysreg.h>
#include <asm/cputype.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("Qualcomm Cache Partition Control for RT workloads (FR203643)");
MODULE_VERSION("1.0");

/* Qualcomm vendor-specific EL1 registers — accessible directly from Linux */
#define CLUSTER_THREAD_SID_EL1  sys_reg(3, 0, 15, 4, 0)
#define CLUSTERPARTCR_EL1       sys_reg(3, 0, 15, 4, 3)

#define BITS_PER_SID            4u
#define L3_WAYS_SID_MASK(sid)   GENMASK(((sid) * BITS_PER_SID) + 3, (sid) * BITS_PER_SID)

/*
 * Upper bound for cluster-ID bitmap deduplication.
 * Not a hard CPU limit — topology_cluster_id() values outside [0, limit)
 * are skipped gracefully.
 */
#define MAX_CLUSTERS_HINT       8

/* Serialize concurrent cluster writes so two callers don't race on the same cluster */
static DEFINE_SPINLOCK(partcr_lock);

struct reg_read_context {
	u64 *values;	/* caller-allocated, nr_cpu_ids elements, indexed by cpu */
	int  reg_id;	/* 1 = CLUSTERPARTCR_EL1, 2 = CLUSTER_THREAD_SID_EL1 */
};

static void cache_partition_reg_write(u64 regval, int reg)
{
	switch (reg) {
	case 1:
		write_sysreg_s(regval, CLUSTERPARTCR_EL1);
		break;
	case 2:
		write_sysreg_s(regval, CLUSTER_THREAD_SID_EL1);
		break;
	default:
		pr_err("cache_partition: invalid register id %d\n", reg);
		return;
	}
	isb();
}

static void generic_reg_read(void *info)
{
	struct reg_read_context *ctx = info;
	int cpu = smp_processor_id();
	u64 val = 0;

	switch (ctx->reg_id) {
	case 1:
		val = read_sysreg_s(CLUSTERPARTCR_EL1);
		break;
	case 2:
		val = read_sysreg_s(CLUSTER_THREAD_SID_EL1);
		break;
	default:
		pr_err("cache_partition: CPU%d invalid reg id %d\n", cpu, ctx->reg_id);
		return;
	}

	ctx->values[cpu] = val;
}

/* ---------- CLUSTERPARTCR_EL1 (per cluster) ---------- */

static ssize_t partcr_el1_show(struct config_item *item, char *page)
{
	struct reg_read_context ctx;
	DECLARE_BITMAP(seen, MAX_CLUSTERS_HINT);
	int len = 0, cpu, clid;

	ctx.values = kcalloc(nr_cpu_ids, sizeof(u64), GFP_KERNEL);
	if (!ctx.values)
		return -ENOMEM;
	ctx.reg_id = 1;

	bitmap_zero(seen, MAX_CLUSTERS_HINT);

	/* Read from the first online CPU of each cluster */
	for_each_online_cpu(cpu) {
		clid = topology_cluster_id(cpu);
		if (clid < 0 || clid >= MAX_CLUSTERS_HINT)
			continue;
		if (!test_and_set_bit(clid, seen))
			smp_call_function_single(cpu, generic_reg_read, &ctx, true);
	}

	/* Display one line per cluster */
	bitmap_zero(seen, MAX_CLUSTERS_HINT);
	for_each_online_cpu(cpu) {
		clid = topology_cluster_id(cpu);
		if (clid < 0 || clid >= MAX_CLUSTERS_HINT)
			continue;
		if (!test_and_set_bit(clid, seen))
			len += scnprintf(page + len, PAGE_SIZE - len,
					 "Cluster %d: 0x%llx\n", clid, ctx.values[cpu]);
	}

	len += scnprintf(page + len, PAGE_SIZE - len,
			 "\nUsage: echo <hex_value> <cluster_id> > partcr_el1\n");

	kfree(ctx.values);
	return len;
}

static void do_partcr_write(void *data)
{
	cache_partition_reg_write((unsigned long)data, 1);
}

static ssize_t partcr_el1_store(struct config_item *item,
				const char *page, size_t count)
{
	u32 value;
	int cluster_id, cpu, rep_cpu = -1;

	if (sscanf(page, "%x %d", &value, &cluster_id) != 2) {
		pr_err("cache_partition: expected: <hex_value> <cluster_id>\n");
		return -EINVAL;
	}

	/* Find an online representative CPU for the requested cluster */
	for_each_online_cpu(cpu) {
		if (topology_cluster_id(cpu) == cluster_id) {
			rep_cpu = cpu;
			break;
		}
	}

	if (rep_cpu < 0) {
		pr_err("cache_partition: cluster %d has no online CPU\n", cluster_id);
		return -EINVAL;
	}

	spin_lock(&partcr_lock);
	smp_call_function_single(rep_cpu, do_partcr_write,
				 (void *)(unsigned long)value, true);
	spin_unlock(&partcr_lock);

	pr_info("cache_partition: CLUSTERPARTCR_EL1=0x%x cluster %d (CPU%d)\n",
		value, cluster_id, rep_cpu);
	return count;
}

/* ---------- CLUSTER_THREAD_SID_EL1 (per CPU thread) ---------- */

static ssize_t thread_sid_el1_show(struct config_item *item, char *page)
{
	struct reg_read_context ctx;
	int len = 0, cpu;

	ctx.values = kcalloc(nr_cpu_ids, sizeof(u64), GFP_KERNEL);
	if (!ctx.values)
		return -ENOMEM;
	ctx.reg_id = 2;

	for_each_online_cpu(cpu)
		smp_call_function_single(cpu, generic_reg_read, &ctx, true);

	for_each_possible_cpu(cpu)
		len += scnprintf(page + len, PAGE_SIZE - len,
				 "CPU%d: 0x%llx%s\n", cpu,
				 ctx.values[cpu],
				 cpu_online(cpu) ? "" : " [offline]");

	len += scnprintf(page + len, PAGE_SIZE - len,
			 "\nUsage: echo <hex_value> <cpu_num> > thread_sid_el1\n");

	kfree(ctx.values);
	return len;
}

static void do_thread_sid_write(void *data)
{
	cache_partition_reg_write((unsigned long)data, 2);
}

static ssize_t thread_sid_el1_store(struct config_item *item,
				    const char *page, size_t count)
{
	u32 value;
	int cpu_num;

	if (sscanf(page, "%x %d", &value, &cpu_num) != 2) {
		pr_err("cache_partition: expected: <hex_value> <cpu_num>\n");
		return -EINVAL;
	}

	if (cpu_num < 0 || cpu_num >= nr_cpu_ids) {
		pr_err("cache_partition: CPU%d out of range\n", cpu_num);
		return -EINVAL;
	}

	if (!cpu_online(cpu_num)) {
		pr_err("cache_partition: CPU%d is offline\n", cpu_num);
		return -EINVAL;
	}

	smp_call_function_single(cpu_num, do_thread_sid_write,
				 (void *)(unsigned long)value, true);

	pr_info("cache_partition: CLUSTER_THREAD_SID_EL1=0x%x CPU%d\n",
		value, cpu_num);
	return count;
}

/* ---------- ConfigFS subsystem ---------- */

CONFIGFS_ATTR(, partcr_el1);
CONFIGFS_ATTR(, thread_sid_el1);

static struct configfs_attribute *cache_partition_attrs[] = {
	&attr_partcr_el1,
	&attr_thread_sid_el1,
	NULL,
};

static struct config_item_type cache_partition_type = {
	.ct_attrs = cache_partition_attrs,
	.ct_owner = THIS_MODULE,
};

static struct configfs_subsystem cache_partition_subsys = {
	.su_group = {
		.cg_item = {
			.ci_namebuf = "cache_partition",
			.ci_type    = &cache_partition_type,
		},
	},
};

static int __init cache_partition_init(void)
{
	int ret;

	config_group_init(&cache_partition_subsys.su_group);
	mutex_init(&cache_partition_subsys.su_mutex);

	ret = configfs_register_subsystem(&cache_partition_subsys);
	if (ret) {
		pr_err("cache_partition: failed to register configfs subsystem: %d\n", ret);
		return ret;
	}

	pr_info("cache_partition: loaded (%d CPUs online)\n", num_online_cpus());
	return 0;
}

static void __exit cache_partition_exit(void)
{
	configfs_unregister_subsystem(&cache_partition_subsys);
	pr_info("cache_partition: unloaded\n");
}

module_init(cache_partition_init);
module_exit(cache_partition_exit);
