/*
 * Accessing sysfs interface for memory hotplug operation from
 * inside the kernel.
 *
 * Licensed under GPL V2
 */
#ifndef __SYSFS_H
#define __SYSFS_H

#include <linux/fs.h>
#include <linux/uaccess.h>

#define AUTO_ONLINE_BLOCKS "/sys/devices/system/memory/auto_online_blocks"
#define BLOCK_SIZE_BYTES   "/sys/devices/system/memory/block_size_bytes"
#define MEMORY_PROBE       "/sys/devices/system/memory/probe"

static ssize_t read_buf(char *filename, char *buf, ssize_t count)
{
	mm_segment_t old_fs;
	struct file *filp;
	loff_t pos = 0;

	if (!count)
		return 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		count = PTR_ERR(filp);
		goto err_open;
	}

	count = vfs_read(filp, buf, count - 1, &pos);
	buf[count] = '\0';

	filp_close(filp, NULL);

err_open:
	set_fs(old_fs);

	return count;
}

static unsigned long long read_0x(char *filename)
{
	unsigned long long ret;
	char buf[32];

	if (read_buf(filename, buf, 32) <= 0)
		return 0;

	if (kstrtoull(buf, 16, &ret))
		return 0;

	return ret;
}

static ssize_t write_buf(char *filename, char *buf)
{
	int ret;
	mm_segment_t old_fs;
	struct file *filp;
	loff_t pos = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(filename, O_WRONLY, 0);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		goto err_open;
	}

	ret = vfs_write(filp, buf, strlen(buf), &pos);

	filp_close(filp, NULL);

err_open:
	set_fs(old_fs);

	return ret;
}

int memory_probe_store(phys_addr_t addr, phys_addr_t size)
{
	phys_addr_t block_sz =
		read_0x(BLOCK_SIZE_BYTES);
	long i;

	for (i = 0; i < size / block_sz; i++, addr += block_sz) {
		char s[32];
		ssize_t count;

		snprintf(s, 32, "0x%llx", addr);

		count = write_buf(MEMORY_PROBE, s);
		if (count < 0)
			return count;
	}

	return 0;
}

int store_mem_state(phys_addr_t addr, phys_addr_t size, char *state)
{
	phys_addr_t block_sz = read_0x(BLOCK_SIZE_BYTES);
	unsigned long start_block, end_block, i;

	start_block = addr / block_sz;
	end_block = start_block + size / block_sz;

	for (i = end_block - 1; i >= start_block; i--) {
		char filename[64];
		ssize_t count;

		snprintf(filename, 64,
			 "/sys/devices/system/memory/memory%ld/state", i);

		count = write_buf(filename, state);
		if (count < 0)
			return count;
	}

	return 0;
}

int disable_auto_online(void)
{
	int ret;

	ret = write_buf(AUTO_ONLINE_BLOCKS, "offline");
	if (ret)
		return ret;
	return 0;
}

int enable_auto_online(void)
{
	int ret;

	ret = write_buf(AUTO_ONLINE_BLOCKS, "online");
	if (ret)
		return ret;
	return 0;
}
#endif
