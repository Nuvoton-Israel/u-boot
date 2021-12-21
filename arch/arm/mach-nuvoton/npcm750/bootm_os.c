// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2019-2022 Nuvoton Technology Corp.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <cpu_func.h>

#define NPCM750_GCR_INTCR2_SELFTEST_PASSED		BIT(11)
#define NPCM750_GCR_INTCR2_WDC				BIT(21)

#define NPCM750_GCR_FLOCKR1_UPDATE_APPROVE		BIT(28)
#define NPCM750_GCR_FLOCKR1_UPDATE_APPROVE_LOCK		BIT(29)

static int npcm750_check_selftest (void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	int ret = 0;
	int val = 0;

	if (readl(&gcr->intcr2) & NPCM750_GCR_INTCR2_SELFTEST_PASSED)
	{
		val = readl(&gcr->flockr1);
		val |= NPCM750_GCR_FLOCKR1_UPDATE_APPROVE;
		writel(val, &gcr->flockr1);

		/* this will clear INTCR2.WDC */
		reset_misc();

		reset_cpu(0);
	}
	else
	{
		val = readl(&gcr->flockr1);
		val &= ~NPCM750_GCR_FLOCKR1_UPDATE_APPROVE;
		writel(val, &gcr->flockr1);

		val = readl(&gcr->flockr1);
		val |= NPCM750_GCR_FLOCKR1_UPDATE_APPROVE_LOCK;
		writel(val, &gcr->flockr1);
	}
	return ret;
}

/* Allow for arch specific config before we boot */
void arch_preboot_os(void)
{
	/* please define platform specific arch_preboot_os() */
	npcm750_check_selftest();
}

