// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Qualcomm Cache Partition Control via ConfigFS
 *
 * Programs CLUSTERPARTCR_EL1 (per-cluster way assignment) and
 * CLUSTER_THREAD_SID_EL1 (per-thread stream ID) directly at EL1 —
 * no TrustZone / SMC involved.
 *
 * Both registers are reset by hardware on cluster power-gate.  The driver
 * mirrors every write into shadow arrays (partcr_state / thread_sid_state)
 * and restores them via a cpu_pm notifier on CPU_PM_EXIT, so register state
 * survives idle transitions without requiring cpuidle.off=1.
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
#include <linux/cpu_pm.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/topology.h>
#include <linux/cpumask.h>
#include <asm/sysreg.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qualcomm Technologies, Inc.");
MODULE_DESCRIPTION("Qualcomm L3 cache partition control for RT workloads");
MODULE_VERSION("1.0");

/* Qualcomm vendor-specific EL1 registers — accessible directly from Linux */
#define CLUSTER_THREAD_SID_EL1  sys_reg(3, 0, 15, 4, 0)
#define CLUSTERPARTCR_EL1       sys_reg(3, 0, 15, 4, 3)

/*
 * Upper bound for cluster-ID bitmap deduplication.
 * Not a hard CPU limit — show and restore paths skip cluster IDs outside
 * [0, limit) gracefully; store rejects them with -EINVAL.
 */
#define MAX_CLUSTERS_HINT       8

enum cache_reg {
	REG_PARTCR,     /* CLUSTERPARTCR_EL1      — per-cluster way assignment */
	REG_THREAD_SID, /* CLUSTER_THREAD_SID_EL1 — per-thread stream ID       */
};

/*
 * Shadow state: mirrors the last value written to each register.
 * Restored by the cpu_pm notifier after a cluster power-gate resets
 * both CLUSTERPARTCR_EL1 and CLUSTER_THREAD_SID_EL1.
 */
static u64 partcr_state[MAX_CLUSTERS_HINT];
static u64 thread_sid_state[NR_CPUS];

struct reg_read_context {
	u64            *values; /* caller-allocated, nr_cpu_ids elements, indexed by cpu */
	enum cache_reg  reg;
};

struct reg_write_ctx {
	u64            value;
	enum cache_reg reg;
};

static void cache_partition_reg_write(u64 regval, enum cache_reg reg)
{
	switch (reg) {
	case REG_PARTCR:
		write_sysreg_s(regval, CLUSTERPARTCR_EL1);
		break;
	case REG_THREAD_SID:
		write_sysreg_s(regval, CLUSTER_THREAD_SID_EL1);
		break;
	default:
		return;
	}
	isb();
}

/* ---------- cpu_pm notifier: restore registers on cluster wake ---------- */

/*
 * Called on the waking CPU with interrupts disabled.  Restores both
 * registers from shadow state so partitioning survives cluster power-gate.
 */
static void restore_regs_this_cpu(void)
{
	int cpu = smp_processor_id();
	int clid = topology_cluster_id(cpu);

	if (clid >= 0 && clid < MAX_CLUSTERS_HINT)
		cache_partition_reg_write(READ_ONCE(partcr_state[clid]), REG_PARTCR);
	cache_partition_reg_write(READ_ONCE(thread_sid_state[cpu]), REG_THREAD_SID);
}

static int cache_partition_cpu_pm_notify(struct notifier_block *nb,
					  unsigned long action, void *unused)
{
	if (action == CPU_PM_EXIT || action == CPU_PM_ENTER_FAILED)
		restore_regs_this_cpu();
	return NOTIFY_OK;
}

static struct notifier_block cache_partition_cpu_pm_nb = {
	.notifier_call = cache_partition_cpu_pm_notify,
};

static void generic_reg_read(void *info)
{
	struct reg_read_context *ctx = info;
	int cpu = smp_processor_id();
	u64 val = 0;

	switch (ctx->reg) {
	case REG_PARTCR:
		val = read_sysreg_s(CLUSTERPARTCR_EL1);
		break;
	case REG_THREAD_SID:
		val = read_sysreg_s(CLUSTER_THREAD_SID_EL1);
		break;
	}

	ctx->values[cpu] = val;
}

static void do_reg_write(void *data)
{
	const struct reg_write_ctx *w = data;

	cache_partition_reg_write(w->value, w->reg);
}

/* ---------- CLUSTERPARTCR_EL1 (per cluster) ---------- */

static ssize_t partcr_el1_show(struct config_item *item, char *page)
{
	u64 cluster_vals[MAX_CLUSTERS_HINT] = {};
	struct reg_read_context ctx;
	DECLARE_BITMAP(seen, MAX_CLUSTERS_HINT);
	int len = 0, cpu, clid;

	ctx.values = kcalloc(nr_cpu_ids, sizeof(u64), GFP_KERNEL);
	if (!ctx.values)
		return -ENOMEM;
	ctx.reg = REG_PARTCR;

	bitmap_zero(seen, MAX_CLUSTERS_HINT);

	/* Single pass: read from the first online CPU of each cluster */
	for_each_online_cpu(cpu) {
		clid = topology_cluster_id(cpu);
		if (clid < 0 || clid >= MAX_CLUSTERS_HINT)
			continue;
		if (!test_and_set_bit(clid, seen)) {
			if (!smp_call_function_single(cpu, generic_reg_read, &ctx, true))
				cluster_vals[clid] = ctx.values[cpu];
		}
	}

	kfree(ctx.values);

	for_each_set_bit(clid, seen, MAX_CLUSTERS_HINT)
		len += scnprintf(page + len, PAGE_SIZE - len,
				 "Cluster %d: 0x%llx\n", clid, cluster_vals[clid]);

	len += scnprintf(page + len, PAGE_SIZE - len,
			 "\nUsage: echo <hex_value> <cluster_id> > partcr_el1\n");

	return len;
}

static ssize_t partcr_el1_store(struct config_item *item,
				const char *page, size_t count)
{
	struct reg_write_ctx wctx = { .reg = REG_PARTCR };
	u64 value;
	int cluster_id, cpu, rep_cpu = -1, ret;

	if (sscanf(page, "%llx %d", &value, &cluster_id) != 2) {
		pr_err("cache_partition: expected: <hex_value> <cluster_id>\n");
		return -EINVAL;
	}

	if (cluster_id < 0 || cluster_id >= MAX_CLUSTERS_HINT) {
		pr_err("cache_partition: cluster_id %d out of range [0, %d)\n",
		       cluster_id, MAX_CLUSTERS_HINT);
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

	/*
	 * Save shadow state before the hardware write.  The cpu_pm notifier
	 * reads this to restore the register after cluster power-gate.
	 */
	WRITE_ONCE(partcr_state[cluster_id], value);
	wctx.value = value;

	ret = smp_call_function_single(rep_cpu, do_reg_write, &wctx, true);

	if (ret)
		pr_warn("cache_partition: cluster %d CPU%d went offline; state saved, will restore on wake\n",
			cluster_id, rep_cpu);
	else
		pr_info("cache_partition: CLUSTERPARTCR_EL1=0x%llx cluster %d (CPU%d)\n",
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
	ctx.reg = REG_THREAD_SID;

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

static ssize_t thread_sid_el1_store(struct config_item *item,
				    const char *page, size_t count)
{
	struct reg_write_ctx wctx = { .reg = REG_THREAD_SID };
	u64 value;
	int cpu_num, ret;

	if (sscanf(page, "%llx %d", &value, &cpu_num) != 2) {
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

	/* Save shadow state — cpu_pm notifier uses this to restore on wake */
	WRITE_ONCE(thread_sid_state[cpu_num], value);
	wctx.value = value;

	ret = smp_call_function_single(cpu_num, do_reg_write, &wctx, true);
	if (ret)
		pr_warn("cache_partition: CPU%d went offline; state saved, will restore on wake\n",
			cpu_num);
	else
		pr_info("cache_partition: CLUSTER_THREAD_SID_EL1=0x%llx CPU%d\n",
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

	cpu_pm_register_notifier(&cache_partition_cpu_pm_nb);

	pr_info("cache_partition: loaded (%d CPUs online)\n", num_online_cpus());
	return 0;
}

static void __exit cache_partition_exit(void)
{
	cpu_pm_unregister_notifier(&cache_partition_cpu_pm_nb);
	configfs_unregister_subsystem(&cache_partition_subsys);
	pr_info("cache_partition: unloaded\n");
}

module_init(cache_partition_init);
module_exit(cache_partition_exit);