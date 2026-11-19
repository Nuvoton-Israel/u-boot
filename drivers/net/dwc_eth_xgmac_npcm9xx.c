// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023, Intel Corporation
 */

//#define DEBUG

#include <clk.h>
#include <cpu_func.h>
#include <dm.h>
#include <errno.h>
#include <eth_phy.h>
#include <log.h>
#include <malloc.h>
#include <memalign.h>
#include <miiphy.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <reset.h>
#include <wait_bit.h>
//#include <asm/arch/secure_reg_helper.h>
//#include <asm/arch/system_manager.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/cache.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <dm/device_compat.h>
#include "dwc_eth_xgmac.h"
#include "pcs-xpcs-npcm9xx.h"

#define SOCFPGA_XGMAC_SYSCON_ARG_COUNT 2

static int dwxgmac_npcm9xx_do_setphy(struct udevice *dev, u32 modereg)
{
	struct xgmac_priv *xgmac = dev_get_priv(dev);
	int ret;
#if 0
	u32 modemask = SYSMGR_EMACGRP_CTRL_PHYSEL_MASK <<
		       xgmac->syscon_phy_regshift;

	if (!(IS_ENABLED(CONFIG_SPL_BUILD)) && IS_ENABLED(CONFIG_SPL_ATF)) {
		u32 index = ((u64)xgmac->syscon_phy - socfpga_get_sysmgr_addr() -
			     SYSMGR_SOC64_EMAC0) >> 2;

		u32 id = SOCFPGA_SECURE_REG_SYSMGR_SOC64_EMAC0 + index;

		ret = socfpga_secure_reg_update32(id,
						  modemask,
						  modereg <<
						  xgmac->syscon_phy_regshift);
		if (ret) {
			dev_err(dev, "Failed to set PHY register via SMC call\n");
			return ret;
		}

	} else {
		clrsetbits_le32(xgmac->phy, modemask, modereg);
	}
#endif
	return 0;
}

extern void temp_nuvoton_reset(void);


static int xgmac_probe_resources_npcm9xx(struct udevice *dev)
{
	struct xgmac_priv *xgmac = dev_get_priv(dev);
	struct regmap *reg_map;
	struct ofnode_phandle_args args;
	void *range;
	phy_interface_t interface;
	int ret;
	u32 modereg;

	temp_nuvoton_reset();

#if 0
	interface = xgmac->config->interface(dev);

	switch (interface) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_GMII:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII;
		break;
	case PHY_INTERFACE_MODE_RMII:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII;
		break;
	case PHY_INTERFACE_MODE_RGMII:
		modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII;
		break;
	default:
		dev_err(dev, "Unsupported PHY mode\n");
		return -EINVAL;
	}

	/* Get PHY syscon */
	ret = dev_read_phandle_with_args(dev, "altr,sysmgr-syscon", NULL,
					 SOCFPGA_XGMAC_SYSCON_ARG_COUNT,
					 0, &args);

	if (ret) {
		dev_err(dev, "Failed to get syscon: %d\n", ret);
		return ret;
	}

	if (args.args_count != SOCFPGA_XGMAC_SYSCON_ARG_COUNT) {
		dev_err(dev, "Invalid number of syscon args\n");
		return -EINVAL;
	}

	reg_map = syscon_node_to_regmap(args.node);
	if (IS_ERR(reg_map)) {
		ret = PTR_ERR(reg_map);
		dev_err(dev, "Failed to get reg_map: %d\n", ret);
		return ret;
	}

	range = regmap_get_range(reg_map, 0);
	if (!range) {
		dev_err(dev, "Failed to get reg_map: %d\n", ret);
		return -ENOMEM;
	}

	xgmac->syscon_phy = range + args.args[0];
	xgmac->syscon_phy_regshift = args.args[1];

	/* Get Reset Bulk */
	ret = reset_get_bulk(dev, &xgmac->reset_bulk);
	if (ret) {
		dev_err(dev, "Failed to get reset: %d\n", ret);
		return ret;
	}

	ret = reset_assert_bulk(&xgmac->reset_bulk);
	if (ret) {
		dev_err(dev, "XGMAC failed to assert reset: %d\n", ret);
		return ret;
	}

	ret = dwxgmac_npcm9xx_do_setphy(dev, modereg);
	if (ret)
		return ret;

	ret = reset_deassert_bulk(&xgmac->reset_bulk);
	if (ret) {
		dev_err(dev, "XGMAC failed to de-assert reset: %d\n", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "stmmaceth", &xgmac->clk_common);
	if (ret) {
		pr_err("clk_get_by_name(stmmaceth) failed: %d", ret);
		goto err_probe;
	}
#endif
	return 0;

err_probe:
	debug("%s: returns %d\n", __func__, ret);
	return ret;
}

static int xgmac_get_enetaddr_npcm9xx(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct xgmac_priv *xgmac = dev_get_priv(dev);
	u32 hi_addr, lo_addr;

	debug("%s(dev=%p):\n", __func__, dev);

	/* Read the MAC Address from the hardawre */
	hi_addr	= readl(&xgmac->mac_regs->address0_high);
	lo_addr	= readl(&xgmac->mac_regs->address0_low);

	pdata->enetaddr[0] = lo_addr & 0xff;
	pdata->enetaddr[1] = (lo_addr >> 8) & 0xff;
	pdata->enetaddr[2] = (lo_addr >> 16) & 0xff;
	pdata->enetaddr[3] = (lo_addr >> 24) & 0xff;
	pdata->enetaddr[4] = hi_addr & 0xff;
	pdata->enetaddr[5] = (hi_addr >> 8) & 0xff;
	debug("%s end:\n", __func__);

	return !is_valid_ethaddr(pdata->enetaddr);
}

#define NPCM_XPCS_GMACSS_XGMAC_CTRL	0x2c
#define NPCM_XPCS_GMACSS_CLK_SEL		BIT(1)
#define NPCM_XPCS_GMACSS_CONFIG_DONE	BIT(0)

static int xgmac_start_resets_npcm9xx(struct udevice *dev)
{
	struct xgmac_priv *xgmac = dev_get_priv(dev);
	fdt_addr_t gmacss_addr;
	int ret;
	bool ref_from_pad;

	debug("%s(dev=%p):\n", __func__, dev);
	gmacss_addr = dev_read_addr_name(dev, "gmacss");
	if (gmacss_addr == FDT_ADDR_T_NONE) {
		dev_err(dev, "missing named GMACSS register range\n");
		return -EINVAL;
	}
#if 0

	ret = reset_assert_bulk(&xgmac->reset_bulk);
	if (ret < 0) {
		pr_err("xgmac reset assert failed: %d", ret);
		return ret;
	}

	udelay(2);

	ret = reset_deassert_bulk(&xgmac->reset_bulk);
	if (ret < 0) {
		pr_err("xgmac reset de-assert failed: %d", ret);
		return ret;
	}
#endif
	ref_from_pad = dev_read_bool(dev, "nuvoton,ref-clock-from-pad");
	clrsetbits_le32((void __iomem *)(gmacss_addr +
					 NPCM_XPCS_GMACSS_XGMAC_CTRL),
			NPCM_XPCS_GMACSS_CLK_SEL,
			ref_from_pad ? NPCM_XPCS_GMACSS_CLK_SEL : 0);
	setbits_le32((void __iomem *)(gmacss_addr +
				      NPCM_XPCS_GMACSS_XGMAC_CTRL),
		     NPCM_XPCS_GMACSS_CONFIG_DONE);

	ret = npcm9xx_xpcs_configure(dev);
	if (ret) {
		dev_err(dev, "XPCS configuration failed: %d\n", ret);
		return ret;
	}

	/* Discard DMA state and prefetched descriptors before rebuilding rings. */
	setbits_le32(&xgmac->dma_regs->mode, XGMAC_DMA_MODE_SWR);

	return 0;
}

static struct xgmac_ops xgmac_npcm9xx_ops = {
	.xgmac_inval_desc = xgmac_inval_desc_generic,
	.xgmac_flush_desc = xgmac_flush_desc_generic,
	.xgmac_inval_buffer = xgmac_inval_buffer_generic,
	.xgmac_flush_buffer = xgmac_flush_buffer_generic,
	.xgmac_probe_resources = xgmac_probe_resources_npcm9xx,
	.xgmac_remove_resources = xgmac_null_ops,
	.xgmac_stop_resets = xgmac_null_ops,
	.xgmac_start_resets = xgmac_start_resets_npcm9xx,
	.xgmac_stop_clks = xgmac_null_ops,
	.xgmac_start_clks = xgmac_null_ops,
	.xgmac_calibrate_pads = xgmac_null_ops,
	.xgmac_disable_calibration = xgmac_null_ops,
	.xgmac_get_enetaddr = xgmac_get_enetaddr_npcm9xx,
};

struct xgmac_config __maybe_unused xgmac_npcm9xx_config = {
	.reg_access_always_ok = false,
	.swr_wait = 50,
	.config_mac = XGMAC_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB,
	.config_mac_mdio = XGMAC_MAC_MDIO_ADDRESS_CR_350_400,
	.axi_bus_width = XGMAC_AXI_WIDTH_64,
	.interface = dev_read_phy_mode,
	.ops = &xgmac_npcm9xx_ops
};
