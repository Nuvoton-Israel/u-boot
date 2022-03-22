// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <dm.h>
#include <sdhci.h>
#include <clk.h>

#define NPCM_SDHC_MIN_FREQ	400000

struct npcm_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

static int npcm_sdhci_probe(struct udevice *dev)
{
	struct npcm_sdhci_plat *plat = dev_get_plat(dev);
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct sdhci_host *host = dev_get_priv(dev);
	int ret;
	struct clk clk;

	host->name = dev->name;
	host->ioaddr = dev_read_addr_ptr(dev);
	host->max_clk = dev_read_u32_default(dev, "clock-frequency", 0);

	if (IS_ENABLED(CONFIG_ARCH_NPCM7xx)) {
		ret = clk_get_by_index(dev, 0, &clk);
		if (ret < 0)
			return ret;
		ret = clk_set_rate(&clk, host->max_clk);
		if (ret < 0)
			return ret;

		host->bus_width = dev_read_u32_default(dev, "bus-width", 0);
		host->index = dev_read_u32_default(dev, "index", 0);
		if (host->bus_width == 4)
                host->host_caps |= MMC_MODE_4BIT;
		if (host->bus_width == 8)
                host->host_caps |= MMC_MODE_8BIT;
	}

	ret = sdhci_setup_cfg(&plat->cfg, host, 0, NPCM_SDHC_MIN_FREQ);
	if (ret)
		return ret;

	host->mmc = &plat->mmc;
	host->mmc->priv = host;
	host->mmc->dev = dev;
	upriv->mmc = host->mmc;

	return sdhci_probe(dev);
}

static int npcm_sdhci_bind(struct udevice *dev)
{
	struct npcm_sdhci_plat *plat = dev_get_plat(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id npcm_mmc_ids[] = {
	{ .compatible = "nuvoton,npcm750-sdhci-eMMC" },
	{ .compatible = "nuvoton,npcm845-sdhci" },
	{ }
};

U_BOOT_DRIVER(npcm_sdc_drv) = {
	.name           = "npcm_sdhci",
	.id             = UCLASS_MMC,
	.of_match       = npcm_mmc_ids,
	.ops            = &sdhci_ops,
	.bind           = npcm_sdhci_bind,
	.probe          = npcm_sdhci_probe,
	.priv_auto      = sizeof(struct sdhci_host),
	.plat_auto      = sizeof(struct npcm_sdhci_plat),
};
