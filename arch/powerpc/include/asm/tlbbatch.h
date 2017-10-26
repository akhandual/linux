#ifndef _ARCH_POWERPC_TLBBATCH_H
#define _ARCH_POWERPC_TLBBATCH_H

struct arch_tlbflush_unmap_batch {
	/*
	 * Each bit set is a CPU that potentially has a
	 * TLB entry for one of the PFNs being flushed.
	 */
	struct cpumask cpumask;
	struct mm_struct *mm;
};

#endif /* _ARCH_POWERPC_TLBBATCH_H */
