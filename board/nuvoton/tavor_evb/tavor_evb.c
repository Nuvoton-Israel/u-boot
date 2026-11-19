// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022 Nuvoton Technology Corp.
 */

#include <common.h>
#include <dm.h>
#include <asm/io.h>
#include <linux/bitfield.h>
#include <asm/arch/gcr.h>
//#include "../common/common.h"

#define NPCM_GCR_BA		0x45970000
#define MCR_DDR_SIZE_REG (NPCM_GCR_BA + 0x1040)
#define DDR_SIZE_MASK  GENMASK(6, 0)

#define MB (1024 * 1024)

#define NPCM_CLK_BA	0xF0801000
#define CLKSEL		0x4
#define PIXCKSEL_GFX	0
#define PIXCKSEL_MASK	GENMASK(5, 4)

#define DRAM_512MB_ECC_SIZE	0x1C000000ULL
#define DRAM_512MB_SIZE		0x20000000ULL
#define DRAM_1GB_ECC_SIZE	0x38000000ULL
#define DRAM_1GB_SIZE		0x40000000ULL
#define DRAM_2GB_ECC_SIZE	0x70000000ULL
#define DRAM_2GB_SIZE		0x80000000ULL
#define DRAM_4GB_ECC_SIZE	0xE0000000ULL
#define DRAM_4GB_SIZE		0x100000000ULL


#define ESPISS1_EXT_REG_BA 0x44020000
#define ESPISS2_EXT_REG_BA 0x44120000
#define ESPISS1_ESPISS_SPSWC_REG (ESPISS1_EXT_REG_BA + 0x80)
#define ESPISS2_ESPISS_SPSWC_REG (ESPISS2_EXT_REG_BA + 0x80)
#define SPISS_SPSWC_UA1_EN  8

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	uint32_t reg_val;

//	u32 val;

	reg_val = readl(ESPISS1_ESPISS_SPSWC_REG);
	reg_val |=  (1 << SPISS_SPSWC_UA1_EN);
	writel(reg_val, ESPISS1_ESPISS_SPSWC_REG);
	printf("ESPISS1_ESPISS_SPSWC_REG = 0x%x\n", reg_val);

	reg_val = readl(ESPISS2_ESPISS_SPSWC_REG);
	reg_val |=  (1 << SPISS_SPSWC_UA1_EN);
	writel(reg_val, ESPISS2_ESPISS_SPSWC_REG);
	printf("ESPISS2_ESPISS_SPSWC_REG = 0x%x\n", reg_val);


//	/* Select GFX_PLL as PIXCK source */
//	val = readl(NPCM_CLK_BA + CLKSEL);
//	val &= ~PIXCKSEL_MASK;
//	val |= FIELD_PREP(PIXCKSEL_MASK, PIXCKSEL_GFX);
//	writel(val, NPCM_CLK_BA + CLKSEL);
	return 0;
}

phys_size_t get_effective_memsize(void)
{
	/* Use bank0 only */
	if (gd->ram_size > DRAM_2GB_SIZE)
		return DRAM_2GB_SIZE;

	return gd->ram_size;
}


#define DDRSIZE_REG 0x45971040


int dram_init(void)
{
	uint32_t ddr_size_factor;

	ddr_size_factor = readl(MCR_DDR_SIZE_REG) & DDR_SIZE_MASK;
	gd->ram_size = (ddr_size_factor + 1) * 128 * MB;

#ifdef CONFIG_TARGET_TAVOR_VELOCE
	gd->flags |= GD_FLG_SKIP_RELOC;
#endif
	debug("ram_size: %llx ", gd->ram_size);

	return 0;
}

int dram_init_banksize(void)
{
	phys_size_t ram_size = gd->ram_size;

	gd->bd->bi_dram[0].start = 0x80000000;

#if defined(CONFIG_SYS_MEM_TOP_HIDE)
	ram_size += CONFIG_SYS_MEM_TOP_HIDE;
#endif
	switch (ram_size) {
	case DRAM_512MB_ECC_SIZE:
	case DRAM_512MB_SIZE:
	case DRAM_1GB_ECC_SIZE:
	case DRAM_1GB_SIZE:
	case DRAM_2GB_ECC_SIZE:
	case DRAM_2GB_SIZE:
		gd->bd->bi_dram[0].size = ram_size;
		gd->bd->bi_dram[1].start = 0;
		gd->bd->bi_dram[1].size = 0;
		break;
	case DRAM_4GB_ECC_SIZE:
		gd->bd->bi_dram[0].size = DRAM_2GB_SIZE;
		gd->bd->bi_dram[1].start = DRAM_4GB_SIZE;
		gd->bd->bi_dram[1].size = DRAM_2GB_SIZE -
			(DRAM_4GB_SIZE - DRAM_4GB_ECC_SIZE);
		break;
	case DRAM_4GB_SIZE:
		gd->bd->bi_dram[0].size = DRAM_2GB_SIZE;
		gd->bd->bi_dram[1].start = DRAM_4GB_SIZE;
		gd->bd->bi_dram[1].size = DRAM_2GB_SIZE;
		break;
	default:
		gd->bd->bi_dram[0].size = DRAM_1GB_SIZE;
		gd->bd->bi_dram[1].start = 0;
		gd->bd->bi_dram[1].size = 0;
		break;
	}

	return 0;
}

int last_stage_init(void)
{
//	board_set_console();

	return 0;
}

int g_dnl_board_usb_cable_connected(void)
{
	return 1;
}
