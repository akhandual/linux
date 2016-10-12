/*
 * Demonstrating various aspects of the coherent memory.
 *
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
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <asm/mmu.h>
#include <asm/pgalloc.h>

#define COHERENT_DEV_MAJOR 89
#define COHERENT_DEV_NAME  "coherent_memory"

#define CRNT_NODE_NID1 1
#define CRNT_NODE_NID2 2
#define CRNT_NODE_NID3 3

#define RAM_CRNT_MIGRATE 1
#define CRNT_RAM_MIGRATE 2

struct vma_map_info {
	struct list_head list;
	unsigned long nr_pages;
	spinlock_t lock;
};

static void vma_map_info_init(struct vm_area_struct *vma)
{
	struct vma_map_info *info = kmalloc(sizeof(struct vma_map_info),
								GFP_KERNEL);

	BUG_ON(!info);
	INIT_LIST_HEAD(&info->list);
	spin_lock_init(&info->lock);
	vma->vm_private_data = info;
	info->nr_pages = 0;
}

static void coherent_vmops_open(struct vm_area_struct *vma)
{
	vma_map_info_init(vma);
}

static void coherent_vmops_close(struct vm_area_struct *vma)
{
	struct vma_map_info *info = vma->vm_private_data;

	BUG_ON(!info);
again:
	cond_resched();
	spin_lock(&info->lock);
	while (info->nr_pages) {
		struct page *page, *page2;

		list_for_each_entry_safe(page, page2, &info->list, lru) {
			if (!trylock_page(page)) {
				spin_unlock(&info->lock);
				goto again;
			}

			list_del_init(&page->lru);
			info->nr_pages--;
			unlock_page(page);
			SetPageReclaim(page);
			put_page(page);
		}
		spin_unlock(&info->lock);
		cond_resched();
		spin_lock(&info->lock);
	}
	spin_unlock(&info->lock);
	kfree(info);
	vma->vm_private_data = NULL;
}

static int coherent_vmops_fault(struct vm_area_struct *vma,
					struct vm_fault *vmf)
{
	struct vma_map_info *info;
	struct page *page;
	static int coherent_node = CRNT_NODE_NID1;

	if (coherent_node == CRNT_NODE_NID1)
		coherent_node = CRNT_NODE_NID2;
	else
		coherent_node = CRNT_NODE_NID1;

	page = alloc_pages_node(coherent_node,
				GFP_HIGHUSER_MOVABLE | __GFP_THISNODE, 0);
	if (!page)
		return VM_FAULT_SIGBUS;

	info = (struct vma_map_info *) vma->vm_private_data;
	BUG_ON(!info);
	spin_lock(&info->lock);
	list_add(&page->lru, &info->list);
	info->nr_pages++;
	spin_unlock(&info->lock);

	page->index = vmf->pgoff;
	get_page(page);
	vmf->page = page;
	return 0;
}

static const struct vm_operations_struct coherent_memory_vmops = {
	.open = coherent_vmops_open,
	.close = coherent_vmops_close,
	.fault = coherent_vmops_fault,
};

static int coherent_memory_mmap(struct file *file, struct vm_area_struct *vma)
{
	pr_info("Mmap opened (file: %lx vma: %lx)\n",
			(unsigned long) file, (unsigned long) vma);
	vma->vm_ops = &coherent_memory_vmops;
	coherent_vmops_open(vma);
	return 0;
}

static int coherent_memory_open(struct inode *inode, struct file *file)
{
	pr_info("Device opened (inode: %lx file: %lx)\n",
			(unsigned long) inode, (unsigned long) file);
	return 0;
}

static int coherent_memory_close(struct inode *inode, struct file *file)
{
	pr_info("Device closed (inode: %lx file: %lx)\n",
			(unsigned long) inode, (unsigned long) file);
	return 0;
}

static void lru_ram_coherent_migrate(unsigned long addr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	nodemask_t nmask;
	LIST_HEAD(mlist);

	nodes_clear(nmask);
	nodes_setall(nmask);
	down_write(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if  ((addr < vma->vm_start) || (addr > vma->vm_end))
			continue;
		break;
	}
	up_write(&mm->mmap_sem);
	if (!vma) {
		pr_info("%s: No VMA found\n", __func__);
		return;
	}
	migrate_virtual_range(current->pid, vma->vm_start, vma->vm_end, 2);
}

static void lru_coherent_ram_migrate(unsigned long addr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	nodemask_t nmask;
	LIST_HEAD(mlist);

	nodes_clear(nmask);
	nodes_setall(nmask);
	down_write(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if  ((addr < vma->vm_start) || (addr > vma->vm_end))
			continue;
		break;
	}
	up_write(&mm->mmap_sem);
	if (!vma) {
		pr_info("%s: No VMA found\n", __func__);
		return;
	}
	migrate_virtual_range(current->pid, vma->vm_start, vma->vm_end, 0);
}

static long coherent_memory_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case RAM_CRNT_MIGRATE:
		lru_ram_coherent_migrate(arg);
		break;

	case CRNT_RAM_MIGRATE:
		lru_coherent_ram_migrate(arg);
		break;

	default:
		pr_info("%s Invalid ioctl() command: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations fops = {
	.mmap = coherent_memory_mmap,
	.open = coherent_memory_open,
	.release = coherent_memory_close,
	.unlocked_ioctl = &coherent_memory_ioctl
};

static char kbuf[100];	/* Will store original user passed buffer */
static char str[100];	/* Working copy for individual substring */

static u64 args[4];
static u64 index;
static void convert_substring(const char *buf)
{
	u64 val = 0;

	if (kstrtou64(buf, 0, &val))
		pr_info("String conversion failed\n");

	args[index] = val;
	index++;
}

static ssize_t coherent_debug_write(struct file *file,
					const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char *tmp, *tmp1;
	size_t ret;

	memset(args, 0, sizeof(args));
	index = 0;

	ret = simple_write_to_buffer(kbuf, sizeof(kbuf), ppos, user_buf, count);
	if (ret < 0)
		return ret;

	kbuf[ret] = '\0';
	tmp = kbuf;
	do {
		tmp1 = strchr(tmp, ',');
		if (tmp1) {
			*tmp1 = '\0';
			strncpy(str, (const char *)tmp, strlen(tmp));
			convert_substring(str);
		} else {
			strncpy(str, (const char *)tmp, strlen(tmp));
			convert_substring(str);
			break;
		}
		tmp = tmp1 + 1;
		memset(str, 0, sizeof(str));
	} while (true);
	migrate_virtual_range(args[0], args[1], args[2], args[3]);
	return ret;
}

static int coherent_debug_show(struct seq_file *m, void *v)
{
	seq_puts(m, "Expected Value: <pid,vaddr,size,nid>\n");
	return 0;
}

static int coherent_debug_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, coherent_debug_show, NULL);
}

static const struct file_operations coherent_debug_fops = {
	.open		= coherent_debug_open,
	.write		= coherent_debug_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *debugfile;

static void coherent_memory_debugfs(void)
{

	debugfile = debugfs_create_file("coherent_debug", 0644, NULL, NULL,
				&coherent_debug_fops);
	if (!debugfile)
		pr_warn("Failed to create coherent_memory in debugfs");
}

static void __exit coherent_memory_exit(void)
{
	pr_info("%s\n", __func__);
	debugfs_remove(debugfile);
	unregister_chrdev(COHERENT_DEV_MAJOR, COHERENT_DEV_NAME);
}

static int __init coherent_memory_init(void)
{
	int ret;

	pr_info("%s\n", __func__);
	ret = register_chrdev(COHERENT_DEV_MAJOR, COHERENT_DEV_NAME, &fops);
	if (ret < 0) {
		pr_info("%s register_chrdev() failed\n", __func__);
		return -1;
	}
	coherent_memory_debugfs();
	return 0;
}

module_init(coherent_memory_init);
module_exit(coherent_memory_exit);
MODULE_LICENSE("GPL");
