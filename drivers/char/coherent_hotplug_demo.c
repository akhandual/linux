/*
 * Memory hotplug support for coherent memory nodes in runtime.
 *
 * Copyright (C) 2016, Reza Arbab, IBM Corporation.
 * Copyright (C) 2016, Anshuman Khandual, IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/of.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/memory.h>
#include <linux/sizes.h>
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/migrate.h>
#include <linux/memblock.h>
#include <linux/uaccess.h>

#include <asm/mmu.h>
#include <asm/pgalloc.h>
#include "memory_online_sysfs.h"

#define MAX_HOTADD_NODES 100
phys_addr_t addr[MAX_HOTADD_NODES][2];
int nr_addr;

/*
 * extern int memory_failure(unsigned long pfn, int trapno, int flags);
 * extern int min_free_kbytes;
 * extern int user_min_free_kbytes;
 *
 * extern unsigned long nr_kernel_pages;
 * extern unsigned long nr_all_pages;
 * extern unsigned long dma_reserve;
 */

static void dump_core_vm_tunables(void)
{
/*
 *	printk(":::::::: VM TUNABLES :::::::\n");
 *	printk("[min_free_kbytes]	%d\n", min_free_kbytes);
 *	printk("[user_min_free_kbytes]	%d\n", user_min_free_kbytes);
 *	printk("[nr_kernel_pages]	%ld\n", nr_kernel_pages);
 *	printk("[nr_all_pages]		%ld\n", nr_all_pages);
 *	printk("[dma_reserve]		%ld\n", dma_reserve);
 */
}



static int online_coherent_memory(void)
{
	struct device_node *memory;

	nr_addr = 0;
	disable_auto_online();
	dump_core_vm_tunables();
	for_each_compatible_node(memory, NULL, "ibm,memory-device") {
		struct device_node *mem;
		const __be64 *reg;
		unsigned int len, ret;
		phys_addr_t start, size;

		mem = of_parse_phandle(memory, "memory-region", 0);
		if (!mem) {
			pr_info("memory-region property not found\n");
			return -1;
		}

		reg = of_get_property(mem, "reg", &len);
		if (!reg || len <= 0) {
			pr_info("memory-region property not found\n");
			return -1;
		}
		start = be64_to_cpu(*reg);
		size = be64_to_cpu(*(reg + 1));
		pr_info("Coherent memory start %llx size %llx\n", start, size);
		ret = memory_probe_store(start, size);
		if (ret)
			pr_info("probe failed\n");

		ret = store_mem_state(start, size, "online_movable");
		if (ret)
			pr_info("online_movable failed\n");

		addr[nr_addr][0] = start;
		addr[nr_addr][1] = size;
		nr_addr++;
	}
	dump_core_vm_tunables();
	enable_auto_online();
	return 0;
}

static int offline_coherent_memory(void)
{
	int i;

	for (i = 0; i < nr_addr; i++)
		store_mem_state(addr[i][0], addr[i][1], "offline");
	return 0;
}

static void __exit coherent_hotplug_exit(void)
{
	pr_info("%s\n", __func__);
	offline_coherent_memory();
}

static int __init coherent_hotplug_init(void)
{
	pr_info("%s\n", __func__);
	return online_coherent_memory();
}
module_init(coherent_hotplug_init);
module_exit(coherent_hotplug_exit);
MODULE_LICENSE("GPL");
