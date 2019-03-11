/*
 * Copyright (c) 2016 Nuvoton Technology Corp.
 *
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/mach-types.h>
#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <clk.h>

DECLARE_GLOBAL_DATA_PTR;

int board_uart_init(void)
{
	struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();

	/* Mode3 - BMC UART3 connected to Serial Interface 2 */
	writel(((readl(&gcr->spswc) & ~(0x07)) | 2), &gcr->spswc);

	/* HSI2SEL */
	writel((readl(&gcr->mfsel1) | (1 << MFSEL1_HSI2SEL)), &gcr->mfsel1);

	/* BSPASEL */
	writel((readl(&gcr->mfsel4) & ~(1 << MFSEL4_BSPASEL)), &gcr->mfsel4);
	return 0;
}

int board_init(void)
{
	gd->bd->bi_arch_number = CONFIG_MACH_TYPE;
	gd->bd->bi_boot_params = (PHYS_SDRAM_1 + 0x100UL);

	return 0;
}

int dram_init(void)
{
	struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();

	int RAMsize = (readl(&gcr->intcr3) >> 8) & 0x3;

	switch(RAMsize)
	{
		case 0:
				gd->ram_size = 0x08000000; /* 128 MB. */
				break;
		case 1:
				gd->ram_size = 0x10000000; /* 256 MB. */
				break;
		case 2:
		/* 3 and 4 should be 1 GB and 2 GB but as a workaround
			to correctly load linux we set it as 512 MB         */
		case 3:
		case 4:
				gd->ram_size = 0x20000000; /* 512 MB. */
				break;

		default:
			break;
	}

	return 0;
}

#ifdef CONFIG_BOARD_EARLY_INIT_F
int board_early_init_f(void)
{
	int err;

	err = board_uart_init();
	if (err) {
		debug("UART init failed\n");
		return err;
	}

	return 0;
}
#endif

#ifdef CONFIG_GENERIC_MMC
int board_mmc_init(bd_t *bis)
{
	int ret = -1;

	printf("please enable CONFIG_DM_MMC\n");

	if (ret)
		printf("mmc init failed\n");

	return ret;
}
#endif

int board_eth_init(bd_t *bis)
{
	return 0;
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	const char *board_info;

	board_info = fdt_getprop(gd->fdt_blob, 0, "model", NULL);
	printf("Board: %s\n", board_info ? board_info : "unknown");
#ifdef CONFIG_BOARD_TYPES
	board_info = get_board_type();
	if (board_info)
		printf("Type:  %s\n", board_info);
#endif
	return 0;
}
#endif
