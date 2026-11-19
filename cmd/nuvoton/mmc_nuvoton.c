#include <command.h>
#include <dm.h>
#include <env.h>
#include <getopt.h>
#include <lmb.h>
#include <mapmem.h>
#include <net.h>
#include <serial.h>
#include <video.h>
#include <vsprintf.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <display_options.h>
#include <mmc.h>

DECLARE_GLOBAL_DATA_PTR;

#define RST_BA 0x45990000
#define ACTHLDRLS_A_REG (0x45990000 + 0x1600)
#define EMMC_RESET_RELES_POS   29

#define CLCK_ADDR 0x45980000
#define FIX_CK_DIV (CLCK_ADDR + 0x120)
#define MMC_BASE_CARD_DIV_POS 24

#define SDHCI_ADDR 0x2230000

#define SDHCI_PHY_CNFG_REG (SDHCI_ADDR + 0x300)
#define SDHCI_PHY_CNFG_REG_PHY_RSTN_POS   0
#define SDHCI_PHY_CNFG_REG_PHY_PWRGOOD_POS   1
#define SDHCI_PHY_CNFG_REG_PAD_SP_POS   16
#define SDHCI_PHY_CNFG_REG_PAD_SN_POS   20


void udelay(int);

static int do_mmc(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	uint32_t reg_val;

	reg_val = readl(FIX_CK_DIV);
	reg_val &= ~(0xf << MMC_BASE_CARD_DIV_POS);
	reg_val |= (4 << MMC_BASE_CARD_DIV_POS);
	writel(reg_val, FIX_CK_DIV);


	reg_val = readl(ACTHLDRLS_A_REG);
	reg_val |= (1 << EMMC_RESET_RELES_POS);
	writel(reg_val, ACTHLDRLS_A_REG);



	reg_val = readl(SDHCI_PHY_CNFG_REG);
	reg_val |=  (0 << SDHCI_PHY_CNFG_REG_PHY_RSTN_POS);
	reg_val |=  (1 << SDHCI_PHY_CNFG_REG_PHY_PWRGOOD_POS);
//	reg_val |=  (0xf << SDHCI_PHY_CNFG_REG_PAD_SP_POS);
//	reg_val |=  (0xf << SDHCI_PHY_CNFG_REG_PAD_SN_POS);
	writel(reg_val, SDHCI_PHY_CNFG_REG);
	udelay(200);
	reg_val |=  (1 << SDHCI_PHY_CNFG_REG_PHY_RSTN_POS);
	writel(reg_val, SDHCI_PHY_CNFG_REG);
	udelay(200);


	puts("MMC:   ");
	mmc_initialize(gd->bd);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(nu_mmc, 2, 1, do_mmc,
	   "init mmc", "nu_mmc"
);
