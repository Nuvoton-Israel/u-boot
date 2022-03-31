// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <fdtdec.h>
#include <asm/arch/cpu.h>
#include <asm/arch/espi.h>
#include <asm/arch/gcr.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <linux/bitfield.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

#define CLKSEL	0x4
#define PIXCKSEL_GFX	0
#define PIXCKSEL_MASK	GENMASK(5, 4)

static void espi_config(u8 mode, u8 max_freq, u32 ch_supp)
{
	u32 val;

	val = readl(NPCM_ESPI_BA + ESPICFG);
	val |= mode << ESPICFG_IOMODE_SHIFT;
	val |= max_freq << ESPICFG_MAXFREQ_SHIFT;
	val |= ((ch_supp & ESPICFG_CHNSUPP_MASK) << ESPICFG_CHNSUPP_SHFT);
	writel(val, NPCM_ESPI_BA + ESPICFG);
}

#define SR_MII_CTRL_SWR_BIT15   15
#define VR_MII_MMD_DIG_CTRL1_R2TLBE_BIT14 14

static void arbel_eth_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)(uintptr_t)npcm_get_base_gcr();
	u32 val;
	char *evb_ver;
	unsigned int start;

	/* Power voltage select setup */
	val = readl(&gcr->vsrcr);
	writel(val | BIT(30), &gcr->vsrcr);

	/* EVB X00 version - need to swap sgmii lane polarity HW issue */
	evb_ver = env_get("evb_version");
	if (evb_ver && !strcmp(evb_ver, "X00")) {
		/* SGMII PHY reset */
		writew(0x1F00, 0xF07801FE);           /* Get access to 0x3E... (SR_MII_CTRL) */
		writew(readw(0xF0780000) | (1 << SR_MII_CTRL_SWR_BIT15), 0xF0780000);
		start = get_timer(0);

		printf("SGMII PCS PHY reset wait\n");
		while (readw(0xF0780000) & (1 << SR_MII_CTRL_SWR_BIT15)) {
			if (get_timer(start) >= 3 * CONFIG_SYS_HZ) {
				printf("SGMII PHY reset timeout\n");
				return;
			}
			mdelay(1);
		};
		/* Get access to 0x3F... (VR_MII_MMD_DIG_CTRL1) */
		writew(0x1F80, 0xF07801FE);
		/* Swap lane polarity on EVB only */
		writew(readw(0xf07801c2) | BIT(0), 0xf07801c2);
		/* Set SGMII MDC/MDIO pins to output slew-rate high */
		writel(readl(0xf001305) | 0x3000, 0xf001305c);
		printf("EVB-X00 SGMII Work-Around\n");
	}
}

static void arbel_clk_init(void)
{
	u32 val;

	/* Select GFX_PLL as PIXCK source */
	val = readl(NPCM_CLK_BA + CLKSEL);
	val &= ~PIXCKSEL_MASK;
	val |= FIELD_PREP(PIXCKSEL_MASK, PIXCKSEL_GFX);
	writel(val, NPCM_CLK_BA + CLKSEL);
}

int board_init(void)
{
	arbel_clk_init();
	arbel_eth_init();

	gd->bd->bi_arch_number = CONFIG_MACH_TYPE;
	gd->bd->bi_boot_params = (PHYS_SDRAM_1 + 0x100UL);

	return 0;
}

int dram_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();

	/*
	 * get dram active size value from bootblock.
	 * Value sent using scrpad_02 register.
	 * feature available in bootblock 0.0.6 and above.
	 */
	gd->ram_size = readl(&gcr->scrpad_b);

	return 0;
}
