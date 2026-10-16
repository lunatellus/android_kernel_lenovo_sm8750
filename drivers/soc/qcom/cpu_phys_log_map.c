// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define NUM_CLUSTER_CPUS NR_CPUS

#ifndef topology_cluster_id
#define topology_cluster_id(cpu) topology_physical_package_id(cpu)
#endif

static int cpu_phys_log_map_client_inited = -EPROBE_DEFER;
static int physical_cpus[NR_CPUS];
static int logical_cpus[NR_CPUS];

/*
 * calling site needs to handle error condition
 * return >=0 for valid cpu, -1 for error
 */
int cpu_logical_to_phys(int cpu)
{
	if (cpu >= NUM_CLUSTER_CPUS || cpu < 0)
		return -EINVAL;

	if (likely(cpu_phys_log_map_client_inited > 0 && physical_cpus[cpu] >= 0))
		return physical_cpus[cpu];

	pr_warn("cpu_phys_log_map invalid inited=%d map=%d\n", cpu_phys_log_map_client_inited,
			physical_cpus[cpu]);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(cpu_logical_to_phys);

/*
 * calling site needs to handle error condition
 * return >=0 for valid cpu, -1 for error
 */
int cpu_phys_to_logical(int cpu)
{
	if (cpu >= NUM_CLUSTER_CPUS || cpu < 0)
		return -EINVAL;

	if (likely(cpu_phys_log_map_client_inited > 0 && logical_cpus[cpu] >= 0))
		return logical_cpus[cpu];

	pr_warn("cpu_phys_log_map invalid inited=%d map=%d\n", cpu_phys_log_map_client_inited,
			logical_cpus[cpu]);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(cpu_phys_to_logical);

static void get_phy_cpu_logic(int clusters_len, u32 *cummulative_cluster_cpus)
{
	int i, cpu, cluster_index, got_cpu;

	for (i = 0; i < NUM_CLUSTER_CPUS; i++) {
		physical_cpus[i] = -1;
		logical_cpus[i] = -1;
	}

	for_each_possible_cpu(cpu) {
		cluster_index = topology_cluster_id(cpu);
		got_cpu = cummulative_cluster_cpus[cluster_index] + topology_core_id(cpu);
		physical_cpus[cpu] = got_cpu;
		logical_cpus[got_cpu] = cpu;
	}

}

static int qcom_cpu_logical_map(void)
{
	int i, ret, clusters_len;
	char name[20];
	struct device_node *cn, *map_handle, *child;
	u32 cluster_cpus[NUM_CLUSTER_CPUS] = {0};
	u32 cummulative_cluster_cpus[NUM_CLUSTER_CPUS] = {0};

	ret = 0;
	i = 0;

	cn = of_find_node_by_path("/cpus");
	if (!cn) {
		ret = -ENODEV;
		cpu_phys_log_map_client_inited = -ENODEV;
		pr_err("No CPU information found in DT\n");
		goto out;
	}

	map_handle = of_get_child_by_name(cn, "cpu-map");
	if (!map_handle)
		goto out;

	do {
		snprintf(name, sizeof(name), "cluster%d", i);
		child = of_get_child_by_name(map_handle, name);
		of_node_put(child);
		cluster_cpus[i] = of_get_child_count(child);
		i++;
	} while (child);

	clusters_len = of_get_child_count(map_handle);

	if (clusters_len < 0)
		goto out;

	for (i = 1; i < clusters_len ; i++)
		cummulative_cluster_cpus[i] = cummulative_cluster_cpus[i-1] + cluster_cpus[i-1];

	get_phy_cpu_logic(clusters_len, cummulative_cluster_cpus);

	cpu_phys_log_map_client_inited = 1;
out:
	return ret;
}

module_init(qcom_cpu_logical_map);
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. CPU Logical Map Module");
MODULE_LICENSE("GPL");
