// SPDX-License-Identifier: GPL-2.0+
/*
 * NPCM9xx Synopsys DesignWare XPCS.
 *
 * Structured after drivers/net/pcs/pcs-xpcs.c of the Linux kernel
 * (Copyright (c) 2020 Synopsys, Inc. and/or its affiliates, Jose Abreu).
 * The XPCS is reached through the NPCM APB direct-access window instead of
 * MDIO, so the Clause 45 (MMD, register) accessors map onto that window.
 * The PMA is NPCM specific and is programmed from a per-mode table.
 */

#include <dm.h>
#include <errno.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mdio.h>
#include <linux/mii.h>
#include <phy_interface.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include "pcs-xpcs-npcm9xx.h"

/* Vendor regs access */
#define DW_VENDOR			BIT(15)

/* VR_XS_PCS */
#define DW_USXGMII_RST			BIT(10)
#define DW_USXGMII_EN			BIT(9)
#define DW_VR_XS_PCS_DIG_CTRL1		0x0000
#define DW_VR_RST			BIT(15)
#define DW_EN_2_5G_MODE			BIT(2)

/* SR_MII */
#define DW_USXGMII_FULL			BIT(8)
#define DW_USXGMII_SS_MASK		(BIT(13) | BIT(6) | BIT(5))
#define DW_USXGMII_10000		(BIT(13) | BIT(6))
#define DW_USXGMII_5000			(BIT(13) | BIT(5))
#define DW_USXGMII_2500			(BIT(5))
#define DW_USXGMII_1000			(BIT(6))
#define DW_USXGMII_100			(BIT(13))
#define DW_USXGMII_10			(0)

/* VR MII MMD registers offsets */
#define DW_VR_MII_DIG_CTRL1		0x8000
#define DW_VR_MII_DIG_CTRL1_MAC_AUTO_SW	BIT(9)
#define DW_VR_MII_DIG_CTRL1_2G5_EN	BIT(2)
#define DW_VR_MII_DIG_CTRL1_5G_TIMER	BIT(3)

#define DW_VR_MII_AN_CTRL		0x8001
#define DW_VR_MII_TX_CONFIG_MASK	BIT(3)
#define DW_VR_MII_TX_CONFIG_PHY_SIDE_SGMII	0x1
#define DW_VR_MII_TX_CONFIG_MAC_SIDE_SGMII	0x0
#define DW_VR_MII_PCS_MODE_MASK		GENMASK(2, 1)
#define DW_VR_MII_PCS_MODE_C37_SGMII	0x2
#define DW_VR_MII_AN_INTR_EN		BIT(0)
#define DW_VR_MII_AN_LINK_STS		BIT(4)

#define DW_VR_MII_AN_INTR_STS		0x8002

/* NPCM9xx specific PCS registers (MMD 3, vendor space) */
#define NPCM_VR_XS_PCS_DEBUG_CTRL	(DW_VENDOR | 0x05)
#define NPCM_DEBUG_RX_DT_EN_CTL		BIT(6)
#define NPCM_DEBUG_SUPRESS_LOS_DET	BIT(4)
#define NPCM_VR_XS_PCS_KR_CTRL		(DW_VENDOR | 0x07)
#define NPCM_PCS_USXG_MODE_MASK		GENMASK(12, 10)
#define NPCM_PCS_USXG_10G		0
#define NPCM_PCS_USXG_5G		1
#define NPCM_PCS_USXG_2_5G		2
#define NPCM_PCS_TYPE_MASK		GENMASK(3, 0)
#define NPCM_PCS_TYPE_10G		0
#define NPCM_PCS_TYPE_1G		1

/* NPCM9xx PMA registers (MMD 1, vendor space) */
#define NPCM_VR_PMA_RX_LSTS		(DW_VENDOR | 0x20)
#define NPCM_VR_PMA_TX_GENCTRL2		(DW_VENDOR | 0x32)
#define NPCM_VR_PMA_TX_RATE_CTRL	(DW_VENDOR | 0x34)
#define NPCM_VR_PMA_TX_EQ_CTRL0		(DW_VENDOR | 0x36)
#define NPCM_VR_PMA_TX_EQ_CTRL1		(DW_VENDOR | 0x37)
#define NPCM_VR_PMA_RX_GENCTRL1		(DW_VENDOR | 0x51)
#define NPCM_VR_PMA_RX_GENCTRL2		(DW_VENDOR | 0x52)
#define NPCM_VR_PMA_RX_RATE_CTRL	(DW_VENDOR | 0x54)
#define NPCM_VR_PMA_RX_CDR_CTRL		(DW_VENDOR | 0x56)
#define NPCM_VR_PMA_RX_EQ_CTRL0		(DW_VENDOR | 0x58)
#define NPCM_VR_PMA_RX_EQ_CTRL4		(DW_VENDOR | 0x5c)
#define NPCM_VR_PMA_AFE_DFE_CTRL	(DW_VENDOR | 0x5d)
#define NPCM_VR_PMA_DFE_TAP_CTRL0	(DW_VENDOR | 0x5e)
#define NPCM_VR_PMA_RX_GENCTRL4		(DW_VENDOR | 0x68)
#define NPCM_VR_PMA_RX_IQ_CTRL0		(DW_VENDOR | 0x6b)
#define NPCM_VR_PMA_MPLL_CMN_CTRL	(DW_VENDOR | 0x70)
#define NPCM_VR_PMA_MPLLA_CTRL0		(DW_VENDOR | 0x71)
#define NPCM_VR_PMA_MPLLA_CTRL1		(DW_VENDOR | 0x72)
#define NPCM_VR_PMA_MPLLA_CTRL2		(DW_VENDOR | 0x73)
#define NPCM_VR_PMA_MPLLA_CTRL3		(DW_VENDOR | 0x77)
#define NPCM_VR_PMA_MISC_CTRL0		(DW_VENDOR | 0x90)
#define NPCM_VR_PMA_REF_CLK_CTRL	(DW_VENDOR | 0x91)
#define NPCM_VR_PMA_VCO_CAL_LD0		(DW_VENDOR | 0x92)
#define NPCM_VR_PMA_VCO_CAL_REF0	(DW_VENDOR | 0x96)
#define NPCM_VR_PMA_MISC_STS		(DW_VENDOR | 0x98)
#define NPCM_VR_PMA_SRAM		(DW_VENDOR | 0x9b)

#define NPCM_PMA_REF_USE_PAD		BIT(1)
#define NPCM_PMA_SRAM_INIT_DN		BIT(0)
#define NPCM_PMA_SRAM_EXT_LD_DN		BIT(1)
#define NPCM_PMA_MPLLB_SEL		BIT(4)
#define NPCM_PMA_MPLLA_FRACN_MASK	GENMASK(15, 5)
#define NPCM_PMA_MPLLA_TX_CLK_DIV	GENMASK(13, 11)
#define NPCM_PMA_MPLLA_DIV16P5_EN	BIT(14)
#define NPCM_PMA_MPLLA_WRD_DIV2_EN	BIT(15)
#define NPCM_PMA_MPLLA_MULT_MASK	GENMASK(7, 0)
#define NPCM_PMA_MPLLA_BW_MASK		GENMASK(5, 0)
#define NPCM_PMA_VCO_LD_MASK		GENMASK(12, 0)
#define NPCM_PMA_VCO_REF_MASK		GENMASK(5, 0)
#define NPCM_PMA_RX_DIV16P5_EN		BIT(12)
#define NPCM_PMA_TX2RX_LB_EN		BIT(0)
#define NPCM_PMA_RX_VALID		BIT(12)
#define NPCM_PMA_RX_AD_REQ		BIT(12)
#define NPCM_PMA_RX_ADPT_ACK		BIT(12)
#define NPCM_PMA_WIDTH_MASK		GENMASK(9, 8)
#define NPCM_PMA_RATE_MASK		GENMASK(2, 0)
#define NPCM_PMA_RX_RATE_MASK		GENMASK(1, 0)
#define NPCM_PMA_RX_VCO_LOW_FREQ	BIT(8)
#define NPCM_PMA_RX_CTLE_MASK		GENMASK(4, 0)
#define NPCM_PMA_RX_AFE_EN		BIT(0)
#define NPCM_PMA_RX_DFE_EN		BIT(4)
#define NPCM_PMA_RX_125MHZ_EN		BIT(0)
#define NPCM_PMA_RX_DELTA_IQ_MASK	GENMASK(13, 8)
#define NPCM_PMA_DFE_TAP1_MASK		GENMASK(7, 0)
#define NPCM_PMA_TX_EQ_MAIN_MASK	GENMASK(13, 8)
#define NPCM_PMA_TX_EQ_PRE_MASK		GENMASK(5, 0)
#define NPCM_PMA_TX_EQ_POST_MASK	GENMASK(5, 0)
#define NPCM_PMA_TX_EQ_OVERRIDE		BIT(6)

#define NPCM_XPCS_INDIRECT_SEG_IDX	0x3fc
#define NPCM_XPCS_GMACSS_XGMAC_CTRL	0x2c
#define NPCM_XPCS_GMACSS_CONFIG_DONE	BIT(0)

#define NPCM_XPCS_TIMEOUT_US		100000
#define NPCM_XPCS_POLL_US		10

enum dw_xpcs_an_mode {
	DW_AN_C73,
	DW_AN_C37_SGMII,
};

struct dw_xpcs_compat {
	phy_interface_t interface;
	int an_mode;
};

/* Per-mode PMA settings; ref_clk_mask == 0 leaves the PLL to platform firmware. */
struct npcm9xx_xpcs_pma {
	u32 ref_clk_mask, ref_clk_val;
	u32 mpll_mult, mpll_ctrl2, mpll_bw;
	u32 vco_ld, vco_ref;
	u32 tx_rate, rx_rate, width;
	u32 afe_dfe, delta_iq;
	u32 tx_eq_main, tx_eq_post;
	bool rx_div16p5, rx_125mhz, tx_eq_override, rx_adapt;
};

struct dw_xpcs {
	struct udevice *dev;
	void __iomem *base;
	void __iomem *gmacss;
	const struct dw_xpcs_compat *compat;
	phy_interface_t interface;
	u32 speed;
	bool inband;
	bool phy_side;
	bool ref_from_pad;
};

extern uint8_t g_gmac_loopback;

static const struct dw_xpcs_compat npcm9xx_xpcs_compat[] = {
	{
		.interface = PHY_INTERFACE_MODE_USXGMII,
		.an_mode = DW_AN_C73,
	}, {
		.interface = PHY_INTERFACE_MODE_SGMII,
		.an_mode = DW_AN_C37_SGMII,
	},
};

static const struct npcm9xx_xpcs_pma npcm9xx_pma_sgmii = {
	.ref_clk_mask = GENMASK(6, 2), .ref_clk_val = 0x54,
	.mpll_mult = 80, .mpll_ctrl2 = 2 << 11, .mpll_bw = 63,
	.vco_ld = 1360, .vco_ref = 17,
	.tx_rate = 1, .rx_rate = 3, .width = 1,
	.tx_eq_main = 24,
};

static const struct npcm9xx_xpcs_pma npcm9xx_pma_usxgmii_10g = {
	.ref_clk_mask = GENMASK(6, 1), .ref_clk_val = 0x56,
	.mpll_mult = 66,
	.mpll_ctrl2 = NPCM_PMA_MPLLA_WRD_DIV2_EN | NPCM_PMA_MPLLA_DIV16P5_EN,
	.mpll_bw = 47, .vco_ld = 1386, .vco_ref = 14,
	.tx_rate = 0, .rx_rate = 0, .width = 3,
	.afe_dfe = NPCM_PMA_RX_AFE_EN | NPCM_PMA_RX_DFE_EN, .delta_iq = 3,
	.tx_eq_main = 18, .tx_eq_post = 24,
	.rx_div16p5 = true, .rx_125mhz = true, .rx_adapt = true,
};

static const struct npcm9xx_xpcs_pma npcm9xx_pma_usxgmii_5g = {
	.tx_rate = 1, .rx_rate = 1, .width = 3,
	.tx_eq_main = 26,
	.rx_125mhz = true, .tx_eq_override = true,
};

static const struct npcm9xx_xpcs_pma npcm9xx_pma_usxgmii_2_5g = {
	.tx_rate = 2, .rx_rate = 2, .width = 3,
	.tx_eq_main = 26,
	.rx_125mhz = true, .tx_eq_override = true,
};

/*
 * {MMD, reg} is a Clause 45 style address; the XPCS is APB3_32 with indirect
 * access, so the real 23-bit address is ({MMD, reg} << 2): bits [22:10] go to
 * the segment register and bits [9:0] are accessed directly.
 */
static u32 xpcs_apb_addr(int dev, u32 reg)
{
	return ((dev << 16) | reg) << 2;
}

static int xpcs_read(struct dw_xpcs *xpcs, int dev, u32 reg)
{
	u32 addr = xpcs_apb_addr(dev, reg);

	writel(addr >> 10, xpcs->base + NPCM_XPCS_INDIRECT_SEG_IDX);

	return readl(xpcs->base + (addr & 0x3ff));
}

static int xpcs_write(struct dw_xpcs *xpcs, int dev, u32 reg, u16 val)
{
	u32 addr = xpcs_apb_addr(dev, reg);

	writel(addr >> 10, xpcs->base + NPCM_XPCS_INDIRECT_SEG_IDX);
	writel(val, xpcs->base + (addr & 0x3ff));

	return 0;
}

static int xpcs_modify(struct dw_xpcs *xpcs, int dev, u32 reg, u16 mask,
		       u16 set)
{
	int val = xpcs_read(xpcs, dev, reg);

	return xpcs_write(xpcs, dev, reg, (val & ~mask) | (set & mask));
}

static int xpcs_modify_vpcs(struct dw_xpcs *xpcs, int reg, u16 mask, u16 val)
{
	return xpcs_modify(xpcs, MDIO_MMD_PCS, DW_VENDOR | reg, mask, val);
}

static int xpcs_modify_pma(struct dw_xpcs *xpcs, u32 reg, u16 mask, u16 val)
{
	return xpcs_modify(xpcs, MDIO_MMD_PMAPMD, reg, mask, val);
}

static int xpcs_poll_bit(struct dw_xpcs *xpcs, int dev, u32 reg, u16 mask,
			 bool set)
{
	u32 timeout = NPCM_XPCS_TIMEOUT_US;

	while (timeout) {
		if (!!(xpcs_read(xpcs, dev, reg) & mask) == set)
			return 0;
		udelay(NPCM_XPCS_POLL_US);
		timeout -= NPCM_XPCS_POLL_US;
	}

	return -ETIMEDOUT;
}

static int xpcs_poll_reset(struct dw_xpcs *xpcs, int dev)
{
	return xpcs_poll_bit(xpcs, dev, MII_BMCR, BMCR_RESET, false);
}

static const struct dw_xpcs_compat *xpcs_find_compat(phy_interface_t interface)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(npcm9xx_xpcs_compat); i++)
		if (npcm9xx_xpcs_compat[i].interface == interface)
			return &npcm9xx_xpcs_compat[i];

	return NULL;
}

static int xpcs_init_phy_sram(struct dw_xpcs *xpcs)
{
	int ret;

	/* NPCM9 spec: wait for PHY SRAM initialization, then set EXT_LD_DN. */
	ret = xpcs_poll_bit(xpcs, MDIO_MMD_PMAPMD, NPCM_VR_PMA_SRAM,
			    NPCM_PMA_SRAM_INIT_DN, true);
	if (ret)
		return ret;

	return xpcs_modify_pma(xpcs, NPCM_VR_PMA_SRAM, NPCM_PMA_SRAM_EXT_LD_DN,
			       NPCM_PMA_SRAM_EXT_LD_DN);
}

/* Select the PCS type and USXGMII rate; there is no Linux counterpart. */
static void npcm9xx_xpcs_switch_mode(struct dw_xpcs *xpcs)
{
	u32 usx_mode;

	if (xpcs->interface == PHY_INTERFACE_MODE_SGMII) {
		xpcs_modify(xpcs, MDIO_MMD_PCS, MDIO_CTRL2,
			    NPCM_PCS_TYPE_MASK, NPCM_PCS_TYPE_1G);
		xpcs_modify_vpcs(xpcs, DW_VR_XS_PCS_DIG_CTRL1,
				 DW_USXGMII_EN | DW_EN_2_5G_MODE, 0);
		return;
	}

	xpcs_modify(xpcs, MDIO_MMD_PCS, MDIO_CTRL2, NPCM_PCS_TYPE_MASK,
		    NPCM_PCS_TYPE_10G);
	xpcs_modify_vpcs(xpcs, DW_VR_XS_PCS_DIG_CTRL1,
			 DW_USXGMII_EN | DW_EN_2_5G_MODE, DW_USXGMII_EN);

	switch (xpcs->speed) {
	case SPEED_10000:
		usx_mode = NPCM_PCS_USXG_10G;
		break;
	case SPEED_5000:
		usx_mode = NPCM_PCS_USXG_5G;
		break;
	default:
		usx_mode = NPCM_PCS_USXG_2_5G;
		break;
	}
	xpcs_modify(xpcs, MDIO_MMD_PCS, NPCM_VR_XS_PCS_KR_CTRL,
		    NPCM_PCS_USXG_MODE_MASK, usx_mode << 10);
}

static void xpcs_link_up_usxgmii(struct dw_xpcs *xpcs, int speed)
{
	int speed_sel;

	switch (speed) {
	case SPEED_10:
		speed_sel = DW_USXGMII_10;
		break;
	case SPEED_100:
		speed_sel = DW_USXGMII_100;
		break;
	case SPEED_1000:
		speed_sel = DW_USXGMII_1000;
		break;
	case SPEED_2500:
		speed_sel = DW_USXGMII_2500;
		break;
	case SPEED_5000:
		speed_sel = DW_USXGMII_5000;
		break;
	case SPEED_10000:
		speed_sel = DW_USXGMII_10000;
		break;
	default:
		return;
	}

	xpcs_modify_vpcs(xpcs, MDIO_CTRL1, DW_USXGMII_EN, DW_USXGMII_EN);
	xpcs_modify(xpcs, MDIO_MMD_VEND2, MII_BMCR, DW_USXGMII_SS_MASK,
		    speed_sel | DW_USXGMII_FULL);
	xpcs_modify_vpcs(xpcs, MDIO_CTRL1, DW_USXGMII_RST, DW_USXGMII_RST);
}

static int xpcs_config_aneg_c37_sgmii(struct dw_xpcs *xpcs)
{
	int mdio_ctrl, tx_conf;
	u16 mask, val;

	/*
	 * Same steps as Linux: disable AN, select SGMII PCS mode and the
	 * TX_CONFIG side, program the MAC_AUTO_SW, clear the AN status and
	 * re-enable AN only for in-band operation.
	 */
	mdio_ctrl = xpcs_read(xpcs, MDIO_MMD_VEND2, MII_BMCR);
	if (mdio_ctrl & BMCR_ANENABLE)
		xpcs_write(xpcs, MDIO_MMD_VEND2, MII_BMCR,
			   mdio_ctrl & ~BMCR_ANENABLE);

	mask = DW_VR_MII_PCS_MODE_MASK | DW_VR_MII_TX_CONFIG_MASK |
	       DW_VR_MII_AN_INTR_EN | DW_VR_MII_AN_LINK_STS;
	val = FIELD_PREP(DW_VR_MII_PCS_MODE_MASK,
			 DW_VR_MII_PCS_MODE_C37_SGMII);
	tx_conf = xpcs->phy_side ? DW_VR_MII_TX_CONFIG_PHY_SIDE_SGMII :
				   DW_VR_MII_TX_CONFIG_MAC_SIDE_SGMII;
	val |= FIELD_PREP(DW_VR_MII_TX_CONFIG_MASK, tx_conf);
	if (xpcs->inband)
		val |= DW_VR_MII_AN_INTR_EN;
	else if (xpcs->phy_side)
		val |= DW_VR_MII_AN_LINK_STS;
	xpcs_modify(xpcs, MDIO_MMD_VEND2, DW_VR_MII_AN_CTRL, mask, val);

	if (xpcs->inband)
		xpcs_modify(xpcs, MDIO_MMD_VEND2, DW_VR_MII_DIG_CTRL1,
			    DW_VR_MII_DIG_CTRL1_MAC_AUTO_SW,
			    DW_VR_MII_DIG_CTRL1_MAC_AUTO_SW);

	xpcs_write(xpcs, MDIO_MMD_VEND2, DW_VR_MII_AN_INTR_STS, 0);

	if (xpcs->inband)
		xpcs_write(xpcs, MDIO_MMD_VEND2, MII_BMCR,
			   mdio_ctrl | BMCR_ANENABLE | BMCR_ANRESTART);

	return 0;
}

static void xpcs_link_up_sgmii(struct dw_xpcs *xpcs, int speed)
{
	u16 bmcr = BMCR_FULLDPLX;

	if (xpcs->inband)
		return;

	if (speed == SPEED_1000)
		bmcr |= BMCR_SPEED1000;
	else if (speed == SPEED_100)
		bmcr |= BMCR_SPEED100;

	xpcs_write(xpcs, MDIO_MMD_VEND2, MII_BMCR, bmcr);
}

static const struct npcm9xx_xpcs_pma *
npcm9xx_xpcs_find_pma(phy_interface_t interface, u32 speed)
{
	if (interface == PHY_INTERFACE_MODE_SGMII)
		return &npcm9xx_pma_sgmii;

	switch (speed) {
	case SPEED_10000:
		return &npcm9xx_pma_usxgmii_10g;
	case SPEED_5000:
		return &npcm9xx_pma_usxgmii_5g;
	case SPEED_2500:
		return &npcm9xx_pma_usxgmii_2_5g;
	default:
		return NULL;
	}
}

/* The compat pma_config step: program the NPCM PHY for the selected mode. */
static int npcm9xx_xpcs_pma_config(struct dw_xpcs *xpcs,
				   const struct npcm9xx_xpcs_pma *pma)
{
	int ret;

	/* A loopback left on by a previous start must be off across VR_RST. */
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_MISC_CTRL0, NPCM_PMA_TX2RX_LB_EN, 0);

	if (pma->ref_clk_mask) {
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_MPLL_CMN_CTRL,
				NPCM_PMA_MPLLB_SEL, 0);
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_REF_CLK_CTRL,
				pma->ref_clk_mask, pma->ref_clk_val);
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_MPLLA_CTRL0,
				NPCM_PMA_MPLLA_MULT_MASK, pma->mpll_mult);
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_MPLLA_CTRL1,
				NPCM_PMA_MPLLA_FRACN_MASK, 0);
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_MPLLA_CTRL2,
				NPCM_PMA_MPLLA_WRD_DIV2_EN |
				NPCM_PMA_MPLLA_DIV16P5_EN |
				NPCM_PMA_MPLLA_TX_CLK_DIV, pma->mpll_ctrl2);
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_MPLLA_CTRL3,
				NPCM_PMA_MPLLA_BW_MASK, pma->mpll_bw);
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_VCO_CAL_LD0,
				NPCM_PMA_VCO_LD_MASK, pma->vco_ld);
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_VCO_CAL_REF0,
				NPCM_PMA_VCO_REF_MASK, pma->vco_ref);
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_RX_GENCTRL1,
				NPCM_PMA_RX_DIV16P5_EN,
				pma->rx_div16p5 ? NPCM_PMA_RX_DIV16P5_EN : 0);
	}

	xpcs_modify_pma(xpcs, NPCM_VR_PMA_TX_RATE_CTRL, NPCM_PMA_RATE_MASK,
			pma->tx_rate);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_RX_RATE_CTRL, NPCM_PMA_RX_RATE_MASK,
			pma->rx_rate);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_TX_GENCTRL2, NPCM_PMA_WIDTH_MASK,
			pma->width << 8);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_RX_GENCTRL2, NPCM_PMA_WIDTH_MASK,
			pma->width << 8);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_RX_GENCTRL4, NPCM_PMA_RX_125MHZ_EN,
			pma->rx_125mhz ? NPCM_PMA_RX_125MHZ_EN : 0);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_AFE_DFE_CTRL,
			NPCM_PMA_RX_AFE_EN | NPCM_PMA_RX_DFE_EN, pma->afe_dfe);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_RX_EQ_CTRL0, NPCM_PMA_RX_CTLE_MASK,
			7);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_RX_IQ_CTRL0,
			NPCM_PMA_RX_DELTA_IQ_MASK, pma->delta_iq << 8);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_DFE_TAP_CTRL0,
			NPCM_PMA_DFE_TAP1_MASK, 0);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_RX_CDR_CTRL,
			NPCM_PMA_RX_VCO_LOW_FREQ, 0);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_REF_CLK_CTRL, NPCM_PMA_REF_USE_PAD,
			xpcs->ref_from_pad ? NPCM_PMA_REF_USE_PAD : 0);

	xpcs_modify_vpcs(xpcs, DW_VR_XS_PCS_DIG_CTRL1, DW_VR_RST, DW_VR_RST);
	ret = xpcs_poll_bit(xpcs, MDIO_MMD_PCS, DW_VENDOR | DW_VR_XS_PCS_DIG_CTRL1,
			    DW_VR_RST, false);
	if (ret)
		return ret;

	xpcs_modify_pma(xpcs, NPCM_VR_PMA_TX_EQ_CTRL0,
			NPCM_PMA_TX_EQ_PRE_MASK | NPCM_PMA_TX_EQ_MAIN_MASK,
			pma->tx_eq_main << 8);
	xpcs_modify_pma(xpcs, NPCM_VR_PMA_TX_EQ_CTRL1,
			NPCM_PMA_TX_EQ_POST_MASK | NPCM_PMA_TX_EQ_OVERRIDE,
			pma->tx_eq_post |
			(pma->tx_eq_override ? NPCM_PMA_TX_EQ_OVERRIDE : 0));
	xpcs_modify(xpcs, MDIO_MMD_PCS, NPCM_VR_XS_PCS_DEBUG_CTRL,
		    NPCM_DEBUG_RX_DT_EN_CTL | NPCM_DEBUG_SUPRESS_LOS_DET, 0);

	/* Enable the analog TX-to-RX serial loopback on lane 0 if requested. */
	if (g_gmac_loopback == 2) {
		udelay(3);
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_MISC_CTRL0,
				NPCM_PMA_TX2RX_LB_EN, NPCM_PMA_TX2RX_LB_EN);
		if (!(xpcs_read(xpcs, MDIO_MMD_PMAPMD,
				NPCM_VR_PMA_MISC_CTRL0) & NPCM_PMA_TX2RX_LB_EN))
			return -EIO;
		udelay(30);
	}

	if (pma->rx_adapt) {
		/* Wait for lane-0 RX lock, request adaptation, wait for ACK. */
		ret = xpcs_poll_bit(xpcs, MDIO_MMD_PMAPMD, NPCM_VR_PMA_RX_LSTS,
				    NPCM_PMA_RX_VALID, true);
		if (ret)
			return ret;

		xpcs_modify_pma(xpcs, NPCM_VR_PMA_RX_EQ_CTRL4,
				NPCM_PMA_RX_AD_REQ, NPCM_PMA_RX_AD_REQ);
		ret = xpcs_poll_bit(xpcs, MDIO_MMD_PMAPMD, NPCM_VR_PMA_MISC_STS,
				    NPCM_PMA_RX_ADPT_ACK, true);
		if (ret)
			return ret;
		xpcs_modify_pma(xpcs, NPCM_VR_PMA_RX_EQ_CTRL4,
				NPCM_PMA_RX_AD_REQ, 0);

		/* Let the serial clocks settle before the USRA reset. */
		mdelay(2);
	}

	return 0;
}

static int xpcs_do_config(struct dw_xpcs *xpcs)
{
	const struct dw_xpcs_compat *compat = xpcs->compat;
	const struct npcm9xx_xpcs_pma *pma;
	int ret;

	switch (compat->an_mode) {
	case DW_AN_C73:
		/*
		 * USXGMII auto-negotiation on this PCS is enabled through the
		 * MII control register once the link speed is programmed.
		 */
		break;
	case DW_AN_C37_SGMII:
		ret = xpcs_config_aneg_c37_sgmii(xpcs);
		if (ret)
			return ret;
		break;
	default:
		return -EINVAL;
	}

	pma = npcm9xx_xpcs_find_pma(xpcs->interface, xpcs->speed);
	if (!pma)
		return -EINVAL;

	if (xpcs->interface == PHY_INTERFACE_MODE_SGMII)
		xpcs_link_up_sgmii(xpcs, xpcs->speed);

	ret = npcm9xx_xpcs_pma_config(xpcs, pma);
	if (ret)
		return ret;

	/* Signal the end of the PHY configuration to the GMAC subsystem. */
	setbits_le32(xpcs->gmacss + NPCM_XPCS_GMACSS_XGMAC_CTRL,
		     NPCM_XPCS_GMACSS_CONFIG_DONE);

	return 0;
}

static void xpcs_link_up(struct dw_xpcs *xpcs)
{
	switch (xpcs->interface) {
	case PHY_INTERFACE_MODE_USXGMII:
		xpcs_link_up_usxgmii(xpcs, xpcs->speed);
		if (xpcs->speed == SPEED_5000)
			xpcs_modify(xpcs, MDIO_MMD_VEND2, DW_VR_MII_DIG_CTRL1,
				    DW_VR_MII_DIG_CTRL1_5G_TIMER,
				    DW_VR_MII_DIG_CTRL1_5G_TIMER);
		if (xpcs->inband)
			xpcs_modify(xpcs, MDIO_MMD_VEND2, MII_BMCR,
				    BMCR_ANENABLE, BMCR_ANENABLE);
		break;
	case PHY_INTERFACE_MODE_SGMII:
		xpcs_link_up_sgmii(xpcs, xpcs->speed);
		break;
	default:
		break;
	}
}

static int xpcs_get_link_config(struct udevice *dev, phy_interface_t interface,
				u32 *speed, bool *inband)
{
	ofnode fixed_link = ofnode_find_subnode(dev_ofnode(dev), "fixed-link");
	const char *managed;

	*inband = !ofnode_valid(fixed_link);
	if (ofnode_valid(fixed_link))
		*speed = ofnode_read_u32_default(fixed_link, "speed", 0);
	else
		*speed = dev_read_u32_default(dev, "max-speed",
				interface == PHY_INTERFACE_MODE_SGMII ?
				SPEED_1000 : SPEED_10000);

	managed = dev_read_string(dev, "managed");
	if (managed && !strcmp(managed, "in-band-status"))
		*inband = true;

	if (interface == PHY_INTERFACE_MODE_SGMII) {
		if (*speed != SPEED_10 && *speed != SPEED_100 &&
		    *speed != SPEED_1000)
			return -EINVAL;
	} else if (*speed != SPEED_2500 && *speed != SPEED_5000 &&
		   *speed != SPEED_10000) {
		return -EINVAL;
	}

	return 0;
}

int npcm9xx_xpcs_set_pma_loopback(struct udevice *dev, bool on)
{
	struct dw_xpcs xpcs;
	fdt_addr_t pcs_addr = dev_read_addr_name(dev, "pcs");
	int val;

	if (pcs_addr == FDT_ADDR_T_NONE)
		return -EINVAL;
	xpcs.base = (void __iomem *)pcs_addr;

	g_gmac_loopback = on ? 2 : 0;

	/* Before the PHY SRAM handshake the XPCS is in reset; apply at start. */
	if (!(xpcs_read(&xpcs, MDIO_MMD_PMAPMD, NPCM_VR_PMA_SRAM) &
	      NPCM_PMA_SRAM_EXT_LD_DN))
		return 0;

	xpcs_modify_pma(&xpcs, NPCM_VR_PMA_MISC_CTRL0, NPCM_PMA_TX2RX_LB_EN,
			on ? NPCM_PMA_TX2RX_LB_EN : 0);
	val = xpcs_read(&xpcs, MDIO_MMD_PMAPMD, NPCM_VR_PMA_MISC_CTRL0);
	if (!!(val & NPCM_PMA_TX2RX_LB_EN) != on)
		return -EIO;

	/* The PCS RX needs a few us to comma-align on the looped back stream. */
	udelay(100);

	return 0;
}

int npcm9xx_xpcs_configure(struct udevice *dev)
{
	struct dw_xpcs xpcs = { .dev = dev };
	phy_interface_t interface = dev_read_phy_mode(dev);
	fdt_addr_t pcs_addr, gmacss_addr;
	u32 ref_clk, expected_clk;
	int ret;

	pcs_addr = dev_read_addr_name(dev, "pcs");
	if (pcs_addr == FDT_ADDR_T_NONE) {
		dev_err(dev, "missing named PCS register range\n");
		return -EINVAL;
	}
	xpcs.base = (void __iomem *)pcs_addr;

	gmacss_addr = dev_read_addr_name(dev, "gmacss");
	if (gmacss_addr == FDT_ADDR_T_NONE) {
		dev_err(dev, "missing named GMACSS register range\n");
		return -EINVAL;
	}
	xpcs.gmacss = (void __iomem *)gmacss_addr;

	xpcs.compat = xpcs_find_compat(interface);
	if (!xpcs.compat) {
		dev_err(dev, "unsupported NPCM XPCS phy-mode %s\n",
			phy_string_for_interface(interface));
		return -EOPNOTSUPP;
	}
	xpcs.interface = interface;

	ret = xpcs_get_link_config(dev, interface, &xpcs.speed, &xpcs.inband);
	if (ret) {
		dev_err(dev, "unsupported %s link speed %u\n",
			phy_string_for_interface(interface), xpcs.speed);
		return ret;
	}

	expected_clk = interface == PHY_INTERFACE_MODE_SGMII ?
		125000000 : 156250000;
	ref_clk = dev_read_u32_default(dev, "nuvoton,ref-clock-frequency",
				       expected_clk);
	if (ref_clk != expected_clk) {
		dev_err(dev, "%s requires a %u Hz XPCS reference clock\n",
			phy_string_for_interface(interface), expected_clk);
		return -EINVAL;
	}

	xpcs.ref_from_pad = dev_read_bool(dev, "nuvoton,ref-clock-from-pad");
	if (interface == PHY_INTERFACE_MODE_USXGMII && !xpcs.ref_from_pad) {
		dev_err(dev, "USXGMII requires an external 156.25 MHz reference clock\n");
		return -EINVAL;
	}
	xpcs.phy_side = dev_read_bool(dev, "nuvoton,phy-side");

	/* XGMAC_Config_Done releases the XPCS/PHY power-on reset. */
	setbits_le32(xpcs.gmacss + NPCM_XPCS_GMACSS_XGMAC_CTRL,
		     NPCM_XPCS_GMACSS_CONFIG_DONE);

	ret = xpcs_init_phy_sram(&xpcs);
	if (ret) {
		dev_err(dev, "XPCS PHY SRAM initialization did not complete\n");
		return ret;
	}

	ret = xpcs_poll_reset(&xpcs, MDIO_MMD_PCS);
	if (ret) {
		dev_err(dev, "XPCS reset did not complete\n");
		return ret;
	}

	npcm9xx_xpcs_switch_mode(&xpcs);

	ret = xpcs_do_config(&xpcs);
	if (ret) {
		dev_err(dev, "XPCS configuration failed: %d\n", ret);
		return ret;
	}

	xpcs_link_up(&xpcs);

	return 0;
}
