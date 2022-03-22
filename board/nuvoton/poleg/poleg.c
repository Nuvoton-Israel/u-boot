/*
 * Copyright (c) 2016 Nuvoton Technology Corp.
 *
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <fdtdec.h>
#include <fuse.h>
#include <spi.h>
#include <spi_flash.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <asm/arch/espi.h>
#include <asm/arch/gcr.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/arch/otp.h>
#include <asm/arch/poleg_info.h>

DECLARE_GLOBAL_DATA_PTR;

static void poleg_gfx_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	struct clk_ctl *clkctl = (struct clk_ctl *)npcm_get_base_clk();
	int nodeoff;
	u32 reg_val = 0;

	nodeoff = -1;
	while ((nodeoff = fdt_node_offset_by_compatible(gd->fdt_blob, nodeoff,
                "npcm750,runbmc")) >= 0) {
		/* select DAC2 for VGA output */
		reg_val = (1 << INTCR_DACSEL) |
			(1 << INTCR_DACOSOVR) |
			(0x3 << INTCR_GFXIFDIS);
		writel((readl(&gcr->intcr) | reg_val), &gcr->intcr);

		/* select PLL1 clock for Graphic System */
		writel((readl(&clkctl->clksel) | (1 << CLKSEL_GFXCKSEL)), &clkctl->clksel);

		/* set Graphic Reset Delay to fix host stuck */
		writel((readl(&gcr->intcr3) | (0x7 << INTCR3_GFXRSTDLY) ), &gcr->intcr3);;

	}

}

static void poleg_espi_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	u32 reg_val = 0;
	u32 espi_ch_supp;

	if (readl(&gcr->mfsel4) & (1 << MFSEL4_ESPISEL)) {
		espi_ch_supp = fdtdec_get_config_int(gd->fdt_blob, "espi-channel-support", 0);
		if (espi_ch_supp > 0) {
			reg_val = readl(NPCM_ESPI_BA + ESPICFG);
			writel(reg_val | ((espi_ch_supp & ESPICFG_CHNSUPP_MASK) << ESPICFG_CHNSUPP_SHFT),
					NPCM_ESPI_BA + ESPICFG);
		}
	}
}

static void poleg_uart_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	int nodeoff;

	nodeoff = -1;
	while ((nodeoff = fdt_node_offset_by_compatible(gd->fdt_blob, nodeoff,
                "quanta,olympus")) >= 0) {
		/* Uart Mode7 - BMC UART3 connected to Serial Interface 2 */
		writel(((readl(&gcr->spswc) & ~(SPMOD_MASK)) | SPMOD_MODE7), &gcr->spswc);
	}
}

int board_init(void)
{
	poleg_gfx_init();
	poleg_espi_init();
	poleg_uart_init();

	gd->bd->bi_arch_number = CONFIG_MACH_TYPE;
	gd->bd->bi_boot_params = (PHYS_SDRAM_1 + 0x100UL);

	return 0;
}

int dram_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();

	int RAMsize = (readl(&gcr->intcr3) >> 8) & 0x7;

	switch(RAMsize)
	{
		case 0:
				gd->ram_size = 0x08000000; /* 128 MB. */
				break;
		case 1:
				gd->ram_size = 0x10000000; /* 256 MB. */
				break;
		case 2:
				gd->ram_size = 0x20000000; /* 512 MB. */
				break;
		case 3:
				gd->ram_size = 0x40000000; /* 1024 MB. */
				break;
		case 4:
				gd->ram_size = 0x80000000; /* 2048 MB. */
				break;

		default:
			break;
	}

	return 0;
}

#ifdef CONFIG_LAST_STAGE_INIT
int last_stage_init(void)
{
	char value[32];
	struct udevice *dev = gd->cur_serial_dev;

	if (gd->ram_size > 0) {
		sprintf(value, "%ldM", (gd->ram_size / 0x100000));
		env_set("mem", value);
	}

	if (dev && (dev->seq_ >= 0)) {
		void *addr;
		addr = dev_read_addr_ptr(dev);
		if (addr) {
			sprintf(value, "uart8250,mmio32,0x%x", (u32)addr);
			env_set("earlycon", value);
		}
		sprintf(value, "ttyS%d,115200n8", dev->seq_);
		env_set("console", value);

	}
	arch_preboot_os();

	return 0;
}
#endif
