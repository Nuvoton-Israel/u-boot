// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/armv8/mmu.h>
#include <asm/system.h>
#include <cpu_func.h>

int print_cpuinfo(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	unsigned int id = 0;
	unsigned long mpidr_val = 0;
	unsigned int mdlr = 0;

	asm volatile("mrs %0, mpidr_el1" : "=r" (mpidr_val));

	mdlr = readl(&gcr->mdlr);

	printf("CPU-%d: ", (unsigned int)(mpidr_val & 0x3));

	switch (mdlr) {
	case ARBEL_NPCM845:
		printf("NPCM845 ");
		break;
	case ARBEL_NPCM830:
		printf("NPCM830 ");
		break;
	case ARBEL_NPCM810:
		printf("NPCM810 ");
		break;
	default:
		printf("NPCM8XX ");
		break;
	}

	id = readl(&gcr->pdid);
	switch (id) {
	case ARBEL_Z1:
		printf("Z1 @ ");
		break;
	case ARBEL_A1:
		printf("A1 @ ");
		break;
	case ARBEL_A2:
		printf("A2 @ ");
		break;
	default:
		printf("Unknown\n");
		break;
	}

	return 0;
}

int arch_cpu_init(void)
{
	if (!IS_ENABLED(CONFIG_SYS_DCACHE_OFF) &&
	    !IS_ENABLED(CONFIG_SYS_NPCM_DCACHE_OFF)) {
		/* enable cache to speed up system running */
		if (get_sctlr() & CR_M)
			return 0;

		icache_enable();
		__asm_invalidate_dcache_all();
		__asm_invalidate_tlb_all();
		set_sctlr(get_sctlr() | CR_C);
	}
	return 0;
}

static struct mm_region npcm_mem_map[] = {
	{
		/* DRAM */
		.phys = 0x0UL,
		.virt = 0x0UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	},
	{
		.phys = 0x80000000UL,
		.virt = 0x80000000UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	},
	{
		.phys = 0x100000000UL,
		.virt = 0x100000000UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	},
	{
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = npcm_mem_map;

void enable_caches(void)
{
	icache_enable();
	if (!IS_ENABLED(CONFIG_SYS_NPCM_DCACHE_OFF))
		dcache_enable();
}
