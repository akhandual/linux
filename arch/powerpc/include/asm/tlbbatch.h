#ifndef _ARCH_POWERPC_TLBBATCH_H
#define _ARCH_POWERPC_TLBBATCH_H

#include <linux/spinlock.h>

#ifdef CONFIG_ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH

#define MAX_BATCHED_MM 1024

struct arch_tlbflush_unmap_batch {
	/*
	 * Each bit set is a CPU that potentially has a
	 * TLB entry for one of the PFN being flushed.
	 * This represents whether all deferred struct
	 * mm will be flushed for any given CPU.
	 */
	struct cpumask cpumask;

	/* All the deferred struct mm */
	struct mm_struct *mm[MAX_BATCHED_MM];
	unsigned long int nr_mm;
	
};

extern bool arch_tlbbatch_should_defer(struct mm_struct *mm);
extern void arch_tlbbatch_flush(struct arch_tlbflush_unmap_batch *batch);
extern void arch_tlbbatch_add_mm(struct arch_tlbflush_unmap_batch *batch,
					struct mm_struct *mm);
#endif /* CONFIG_ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH */
#endif /* _ARCH_POWERPC_TLBBATCH_H */
