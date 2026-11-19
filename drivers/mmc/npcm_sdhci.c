// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022 Nuvoton Technology Corp.
 */

#include <dm.h>
#include <dm/device_compat.h>
#include <sdhci.h>
#include <clk.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <power/regulator.h>

#define NPCM_SDHC_MIN_FREQ	400000

#define NPCM945_SDHCI		BIT(0)

/* Reset module (RST_BA): ACTHLDRLS_A releases a reset kept by RSTHLD */
#define NPCM945_ACTHLDRLS_A	0x1600
#define NPCM945_RST_EMMC	BIT(29)

/* eMMC external registers (GENERALSS_EXT_REG_BA + 0x1000) */
#define NPCM945_EMMC_CTRL	0x0c
#define NPCM945_EMMC_CFG_DONE	BIT(1)

/* DWC MSHC eMMC PHY registers */
#define DWC_PHY_CNFG		0x300
#define DWC_PHY_RSTN		BIT(0)
#define DWC_PHY_PWRGOOD		BIT(1)
#define DWC_PHY_CMDPAD_CNFG	0x304
#define DWC_PHY_DATPAD_CNFG	0x306
#define DWC_PHY_STBPAD_CNFG	0x30a
#define DWC_PHY_RSTNPAD_CNFG	0x30c
#define DWC_PHY_SDCLKDL_CNFG	0x31d
#define DWC_PHY_SDCLKDL_UPDATE	BIT(4)
#define DWC_PHY_SDCLKDL_DC	0x31e
#define NPCM945_SDCLK_DELAY	0x40
#define DWC_PAD_WEAKPULL_MASK	GENMASK(4, 3)
#define DWC_PAD_WEAKPULL_UP	(1 << 3)
#define DWC_PAD_RXSEL_MASK	GENMASK(2, 0)
#define DWC_PAD_RXSEL_1V8	1

struct npcm_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

static void npcm945_sdhci_set_pads(struct sdhci_host *host)
{
	static const u16 pads[] = {
		DWC_PHY_CMDPAD_CNFG, DWC_PHY_DATPAD_CNFG,
		DWC_PHY_STBPAD_CNFG, DWC_PHY_RSTNPAD_CNFG,
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(pads); i++)
		clrsetbits_le16(host->ioaddr + pads[i],
				DWC_PAD_WEAKPULL_MASK | DWC_PAD_RXSEL_MASK,
				DWC_PAD_WEAKPULL_UP | DWC_PAD_RXSEL_1V8);
}

/* Bus power-up resets RXSEL, so re-apply the pad config like Linux does */
static int npcm945_sdhci_set_ios_post(struct sdhci_host *host)
{
	npcm945_sdhci_set_pads(host);
	return 0;
}

static int npcm945_sdhci_set_delay(struct sdhci_host *host)
{
	/*
	 * Delay the card clock relative to TX data; the reset delay code
	 * causes write failures on wide buses. SDHCI gates the clock here.
	 */
	setbits_8(host->ioaddr + DWC_PHY_SDCLKDL_CNFG,
		  DWC_PHY_SDCLKDL_UPDATE);
	writeb(NPCM945_SDCLK_DELAY, host->ioaddr + DWC_PHY_SDCLKDL_DC);
	clrbits_8(host->ioaddr + DWC_PHY_SDCLKDL_CNFG,
		  DWC_PHY_SDCLKDL_UPDATE);

	return 0;
}

/*
 * The PHY comes up in reset with the pad pull-ups off and the 1.8V receivers
 * disabled (RXSEL=0), so the controller reads as zero and the CMD/DAT lines
 * read low. The controller reset clears this, so it runs after every reset.
 */
static int npcm945_sdhci_post_reset(struct sdhci_host *host)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout(host->ioaddr + DWC_PHY_CNFG, val,
				 val & DWC_PHY_PWRGOOD, 10000);
	if (ret) {
		pr_err("eMMC PHY power not good (PHY_CNFG=0x%x)\n", val);
		return ret;
	}
	setbits_le32(host->ioaddr + DWC_PHY_CNFG, DWC_PHY_RSTN);
	udelay(10);

	/* RXSEL only latches once the PHY is out of reset */
	npcm945_sdhci_set_pads(host);

	return 0;
}

static const struct sdhci_ops npcm945_sdhci_ops = {
	.set_delay = npcm945_sdhci_set_delay,
	.set_ios_post = npcm945_sdhci_set_ios_post,
	.post_reset = npcm945_sdhci_post_reset,
};

/*
 * Tavor: the eMMC is kept in reset by RSTHLD (normally released by the MCU
 * after loading the crypto keys).
 */
static int npcm945_sdhci_init(struct udevice *dev, struct sdhci_host *host)
{
	void __iomem *rst, *ext;

	rst = dev_read_addr_name_ptr(dev, "rst");
	if (rst) {
		writel(NPCM945_RST_EMMC, rst + NPCM945_ACTHLDRLS_A);
		udelay(10);
	}

	ext = dev_read_addr_name_ptr(dev, "ext");
	if (ext)
		setbits_le32(ext + NPCM945_EMMC_CTRL, NPCM945_EMMC_CFG_DONE);

	host->ops = &npcm945_sdhci_ops;

	return 0;
}

static int npcm_sdhci_probe(struct udevice *dev)
{
	struct npcm_sdhci_plat *plat = dev_get_plat(dev);
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct sdhci_host *host = dev_get_priv(dev);
	struct udevice *vqmmc_supply;
	int vqmmc_uv, ret;
	struct clk clk;

	host->name = dev->name;
	host->ioaddr = dev_read_addr_ptr(dev);
	host->max_clk = dev_read_u32_default(dev, "clock-frequency", 0);

	if (dev_get_driver_data(dev) & NPCM945_SDHCI) {
		ret = npcm945_sdhci_init(dev, host);
		if (ret)
			return ret;
	}

	ret = clk_get_by_index(dev, 0, &clk);
	if (!ret && host->max_clk) {
		ret = clk_set_rate(&clk, host->max_clk);
		if (ret < 0)
			return ret;
	}

	if (CONFIG_IS_ENABLED(DM_REGULATOR)) {
		device_get_supply_regulator(dev, "vqmmc-supply", &vqmmc_supply);
		vqmmc_uv = dev_read_u32_default(dev, "vqmmc-microvolt", 0);
		/* Set IO voltage */
		if (vqmmc_supply && vqmmc_uv)
			regulator_set_value(vqmmc_supply, vqmmc_uv);
	}

	host->quirks |= SDHCI_QUIRK_CAPS_BIT63_FOR_HS400;

	host->index = dev_read_u32_default(dev, "index", 0);
	ret = mmc_of_parse(dev, &plat->cfg);
	if (ret)
		return ret;

	host->mmc = &plat->mmc;
	host->mmc->priv = host;
	host->mmc->dev = dev;
	upriv->mmc = host->mmc;

	ret = sdhci_setup_cfg(&plat->cfg, host, 0, NPCM_SDHC_MIN_FREQ);
	if (ret)
		return ret;

	/* sdhci_setup_cfg() always adds 4-bit; honor a DT bus-width of 1 */
	if (dev_read_u32_default(dev, "bus-width", 1) == 1)
		plat->cfg.host_caps &= ~(MMC_MODE_4BIT | MMC_MODE_8BIT);

	return sdhci_probe(dev);
}

static int npcm_sdhci_bind(struct udevice *dev)
{
	struct npcm_sdhci_plat *plat = dev_get_plat(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id npcm_mmc_ids[] = {
	{ .compatible = "nuvoton,npcm750-sdhci" },
	{ .compatible = "nuvoton,npcm845-sdhci" },
	{ .compatible = "nuvoton,npcm945-sdhci", .data = NPCM945_SDHCI },
	{ }
};

U_BOOT_DRIVER(npcm_sdhci_drv) = {
	.name           = "npcm_sdhci",
	.id             = UCLASS_MMC,
	.of_match       = npcm_mmc_ids,
	.ops            = &sdhci_ops,
	.bind           = npcm_sdhci_bind,
	.probe          = npcm_sdhci_probe,
	.priv_auto      = sizeof(struct sdhci_host),
	.plat_auto      = sizeof(struct npcm_sdhci_plat),
};
