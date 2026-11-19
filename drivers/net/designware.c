// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2010
 * Vipin Kumar, STMicroelectronics, vipin.kumar@st.com.
 */

/*
 * Designware ethernet IP driver for U-Boot
 */

//#define DEBUG

#include <clk.h>
#include <cpu_func.h>
#include <dm.h>
#include <errno.h>
#include <eth_phy.h>
#include <log.h>
#include <miiphy.h>
#include <malloc.h>
#include <net.h>
#include <pci.h>
#include <reset.h>
#include <phys2bus.h>
#include <asm/cache.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <dm/devres.h>
#include <dm/lists.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/printk.h>
#include <power/regulator.h>
#include "designware.h"
#if IS_ENABLED(CONFIG_ARCH_NPCM8XX)
#include <asm/arch/gmac.h>
#endif

#ifdef CONFIG_ARCH_NPCM8XX
#include <asm/io.h>

#define SR_MII_CTRL_SS6_BIT6  6
#define SR_MII_CTRL_ANEN_BIT12  12
#define SR_MII_CTRL_SS13_BIT13  13
#define SR_MII_STS_LINKUP_BIT2  2
#endif

static int dw_mdio_read(struct mii_dev *bus, int addr, int devad, int reg)
{
	struct dw_eth_dev *priv = dev_get_priv((struct udevice *)bus->priv);
	struct eth_mac_regs *mac_p = priv->mac_regs_p;
	ulong start;
	u16 miiaddr;
	int timeout = CFG_MDIO_TIMEOUT;

	miiaddr = ((addr << MIIADDRSHIFT) & MII_ADDRMSK) |
		  ((reg << MIIREGSHIFT) & MII_REGMSK);

	writel(miiaddr | MII_CLKRANGE_150_250M | MII_BUSY, &mac_p->miiaddr);

	start = get_timer(0);
	while (get_timer(start) < timeout) {
		if (!(readl(&mac_p->miiaddr) & MII_BUSY))
			return readl(&mac_p->miidata);
		udelay(10);
	};

	return -ETIMEDOUT;
}

static int dw_mdio_write(struct mii_dev *bus, int addr, int devad, int reg,
			u16 val)
{
	struct dw_eth_dev *priv = dev_get_priv((struct udevice *)bus->priv);
	struct eth_mac_regs *mac_p = priv->mac_regs_p;
	ulong start;
	u16 miiaddr;
	int ret = -ETIMEDOUT, timeout = CFG_MDIO_TIMEOUT;

	writel(val, &mac_p->miidata);
	miiaddr = ((addr << MIIADDRSHIFT) & MII_ADDRMSK) |
		  ((reg << MIIREGSHIFT) & MII_REGMSK) | MII_WRITE;

	writel(miiaddr | MII_CLKRANGE_150_250M | MII_BUSY, &mac_p->miiaddr);

	start = get_timer(0);
	while (get_timer(start) < timeout) {
		if (!(readl(&mac_p->miiaddr) & MII_BUSY)) {
			ret = 0;
			break;
		}
		udelay(10);
	};

	return ret;
}

#if CONFIG_IS_ENABLED(DM_GPIO)
static int __dw_mdio_reset(struct udevice *dev)
{
	struct dw_eth_dev *priv = dev_get_priv(dev);
	struct dw_eth_pdata *pdata = dev_get_plat(dev);
	int ret;

	if (!dm_gpio_is_valid(&priv->reset_gpio))
		return 0;

	/* reset the phy */
	ret = dm_gpio_set_value(&priv->reset_gpio, 0);
	if (ret)
		return ret;

	udelay(pdata->reset_delays[0]);

	ret = dm_gpio_set_value(&priv->reset_gpio, 1);
	if (ret)
		return ret;

	udelay(pdata->reset_delays[1]);

	ret = dm_gpio_set_value(&priv->reset_gpio, 0);
	if (ret)
		return ret;

	udelay(pdata->reset_delays[2]);

	return 0;
}

static int dw_mdio_reset(struct mii_dev *bus)
{
	struct udevice *dev = bus->priv;

	return __dw_mdio_reset(dev);
}

#if IS_ENABLED(CONFIG_BITBANGMII)
static int dw_bb_mdio_reset(struct mii_dev *bus)
{
	struct dw_eth_dev *priv = bus->priv;

	return __dw_mdio_reset(priv->dev);
}
#endif

#endif

#if IS_ENABLED(CONFIG_DM_MDIO)
int designware_eth_mdio_read(struct udevice *mdio_dev, int addr, int devad, int reg)
{
	struct mdio_perdev_priv *pdata = dev_get_uclass_priv(mdio_dev);

	return dw_mdio_read(pdata->mii_bus, addr, devad, reg);
}

int designware_eth_mdio_write(struct udevice *mdio_dev, int addr, int devad, int reg, u16 val)
{
	struct mdio_perdev_priv *pdata = dev_get_uclass_priv(mdio_dev);

	return dw_mdio_write(pdata->mii_bus, addr, devad, reg, val);
}

#if CONFIG_IS_ENABLED(DM_GPIO)
int designware_eth_mdio_reset(struct udevice *mdio_dev)
{
	struct mdio_perdev_priv *mdio_pdata = dev_get_uclass_priv(mdio_dev);
	struct udevice *dev = mdio_pdata->mii_bus->priv;

	return __dw_mdio_reset(dev->parent);
}
#endif

static const struct mdio_ops designware_eth_mdio_ops = {
	.read = designware_eth_mdio_read,
	.write = designware_eth_mdio_write,
#if CONFIG_IS_ENABLED(DM_GPIO)
	.reset = designware_eth_mdio_reset,
#endif
};

static int designware_eth_mdio_probe(struct udevice *dev)
{
	/* Use the priv data of parent */
	dev_set_priv(dev, dev_get_priv(dev->parent));

	return 0;
}

U_BOOT_DRIVER(designware_eth_mdio) = {
	.name = "eth_designware_mdio",
	.id = UCLASS_MDIO,
	.probe = designware_eth_mdio_probe,
	.ops = &designware_eth_mdio_ops,
	.plat_auto = sizeof(struct mdio_perdev_priv),
};
#endif

static int dw_mdio_init(const char *name, void *priv)
{
	struct mii_dev *bus = mdio_alloc();

	if (!bus) {
		printf("Failed to allocate MDIO bus\n");
		return -ENOMEM;
	}

	bus->read = dw_mdio_read;
	bus->write = dw_mdio_write;
	snprintf(bus->name, sizeof(bus->name), "%s", name);
#if CONFIG_IS_ENABLED(DM_GPIO)
	bus->reset = dw_mdio_reset;
#endif

	bus->priv = priv;

	return mdio_register(bus);
}

#if IS_ENABLED(CONFIG_DM_MDIO)
static int dw_dm_mdio_init(const char *name, void *priv)
{
	struct udevice *dev = priv;
	ofnode node;
	int ret;

	ofnode_for_each_subnode(node, dev_ofnode(dev)) {
		const char *subnode_name = ofnode_get_name(node);
		struct udevice *mdiodev;

		if (strcmp(subnode_name, "mdio"))
			continue;

		ret = device_bind_driver_to_node(dev, "eth_designware_mdio",
						 subnode_name, node, &mdiodev);
		if (ret)
			debug("%s: not able to bind mdio device node\n", __func__);

		return 0;
	}

	printf("%s: mdio node is missing, registering legacy mdio bus", __func__);

	return dw_mdio_init(name, priv);
}
#endif

#if IS_ENABLED(CONFIG_BITBANGMII) && IS_ENABLED(CONFIG_DM_GPIO)
static int dw_eth_bb_mdio_active(struct mii_dev *miidev)
{
	struct dw_eth_dev *priv = miidev->priv;
	struct gpio_desc *desc = &priv->mdio_gpio;

	desc->flags = 0;
	dm_gpio_set_dir_flags(&priv->mdio_gpio, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

	return 0;
}

static int dw_eth_bb_mdio_tristate(struct mii_dev *miidev)
{
	struct dw_eth_dev *priv = miidev->priv;
	struct gpio_desc *desc = &priv->mdio_gpio;

	desc->flags = 0;
	dm_gpio_set_dir_flags(&priv->mdio_gpio, GPIOD_IS_IN);

	return 0;
}

static int dw_eth_bb_set_mdio(struct mii_dev *miidev, int v)
{
	struct dw_eth_dev *priv = miidev->priv;

	if (v)
		dm_gpio_set_value(&priv->mdio_gpio, 1);
	else
		dm_gpio_set_value(&priv->mdio_gpio, 0);

	return 0;
}

static int dw_eth_bb_get_mdio(struct mii_dev *miidev, int *v)
{
	struct dw_eth_dev *priv = miidev->priv;

	*v = dm_gpio_get_value(&priv->mdio_gpio);

	return 0;
}

static int dw_eth_bb_set_mdc(struct mii_dev *miidev, int v)
{
	struct dw_eth_dev *priv = miidev->priv;

	if (v)
		dm_gpio_set_value(&priv->mdc_gpio, 1);
	else
		dm_gpio_set_value(&priv->mdc_gpio, 0);

	return 0;
}

static int dw_eth_bb_delay(struct mii_dev *miidev)
{
	struct dw_eth_dev *priv = miidev->priv;

	udelay(priv->bb_delay);
	return 0;
}

static const struct bb_miiphy_bus_ops dw_eth_bb_miiphy_bus_ops = {
	.mdio_active	= dw_eth_bb_mdio_active,
	.mdio_tristate	= dw_eth_bb_mdio_tristate,
	.set_mdio	= dw_eth_bb_set_mdio,
	.get_mdio	= dw_eth_bb_get_mdio,
	.set_mdc	= dw_eth_bb_set_mdc,
	.delay		= dw_eth_bb_delay,
};

static int dw_bb_miiphy_read(struct mii_dev *miidev, int addr,
			     int devad, int reg)
{
	return bb_miiphy_read(miidev, &dw_eth_bb_miiphy_bus_ops,
			      addr, devad, reg);
}

static int dw_bb_miiphy_write(struct mii_dev *miidev, int addr,
			      int devad, int reg, u16 value)
{
	return bb_miiphy_write(miidev, &dw_eth_bb_miiphy_bus_ops,
			       addr, devad, reg, value);
}

static int dw_bb_mdio_init(const char *name, struct udevice *dev)
{
	struct dw_eth_dev *dwpriv = dev_get_priv(dev);
	struct mii_dev *bus = mdio_alloc();
	int ret;

	if (!bus) {
		printf("Failed to allocate MDIO bus\n");
		return -ENOMEM;
	}

	debug("\n%s: use bitbang mii..\n", dev->name);
	ret = gpio_request_by_name(dev, "snps,mdc-gpio", 0,
				   &dwpriv->mdc_gpio,
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret) {
		debug("no mdc-gpio\n");
		return ret;
	}
	ret = gpio_request_by_name(dev, "snps,mdio-gpio", 0,
				   &dwpriv->mdio_gpio,
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret) {
		debug("no mdio-gpio\n");
		return ret;
	}
	dwpriv->bb_delay = dev_read_u32_default(dev, "snps,bitbang-delay", 1);

	dwpriv->bus = bus;
	dwpriv->dev = dev;

	snprintf(bus->name, sizeof(bus->name), "%s", name);
	bus->read = dw_bb_miiphy_read;
	bus->write = dw_bb_miiphy_write;
#if CONFIG_IS_ENABLED(DM_GPIO)
	bus->reset = dw_bb_mdio_reset;
#endif
	bus->priv = dwpriv;

	return mdio_register(bus);
}
#endif

static void tx_descs_init(struct dw_eth_dev *priv)
{
	struct eth_dma_regs *dma_p = priv->dma_regs_p;
	struct dmamacdescr *desc_table_p = &priv->tx_mac_descrtable[0];
	char *txbuffs = &priv->txbuffs[0];
	struct dmamacdescr *desc_p;
	u32 idx;

	for (idx = 0; idx < CFG_TX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
		desc_p->dmamac_addr = dev_phys_to_bus(priv->dev,
				(ulong)&txbuffs[idx * CFG_ETH_BUFSIZE]);
		desc_p->dmamac_next = dev_phys_to_bus(priv->dev,
				(ulong)&desc_table_p[idx + 1]);

#if defined(CONFIG_DW_ALTDESCRIPTOR)
		desc_p->txrx_status &= ~(DESC_TXSTS_TXINT | DESC_TXSTS_TXLAST |
				DESC_TXSTS_TXFIRST | DESC_TXSTS_TXCRCDIS |
				DESC_TXSTS_TXCHECKINSCTRL |
				DESC_TXSTS_TXRINGEND | DESC_TXSTS_TXPADDIS);

		desc_p->txrx_status |= DESC_TXSTS_TXCHAIN;
		desc_p->dmamac_cntl = 0;
		desc_p->txrx_status &= ~(DESC_TXSTS_MSK | DESC_TXSTS_OWNBYDMA);
#else
		desc_p->dmamac_cntl = DESC_TXCTRL_TXCHAIN;
		desc_p->txrx_status = 0;
#endif
	}

	/* Correcting the last pointer of the chain */
	desc_p->dmamac_next = dev_phys_to_bus(priv->dev, (ulong)&desc_table_p[0]);

	/* Flush all Tx buffer descriptors at once */
	flush_dcache_range((ulong)priv->tx_mac_descrtable,
			   (ulong)priv->tx_mac_descrtable +
			   sizeof(priv->tx_mac_descrtable));

	writel(dev_phys_to_bus(priv->dev, (ulong)&desc_table_p[0]),
			&dma_p->txdesclistaddr);
	priv->tx_currdescnum = 0;
}

static void rx_descs_init(struct dw_eth_dev *priv)
{
	struct eth_dma_regs *dma_p = priv->dma_regs_p;
	struct dmamacdescr *desc_table_p = &priv->rx_mac_descrtable[0];
	char *rxbuffs = &priv->rxbuffs[0];
	struct dmamacdescr *desc_p;
	u32 idx;

	/* Before passing buffers to GMAC we need to make sure zeros
	 * written there right after "priv" structure allocation were
	 * flushed into RAM.
	 * Otherwise there's a chance to get some of them flushed in RAM when
	 * GMAC is already pushing data to RAM via DMA. This way incoming from
	 * GMAC data will be corrupted. */
	flush_dcache_range((ulong)rxbuffs, (ulong)rxbuffs + RX_TOTAL_BUFSIZE);

	for (idx = 0; idx < CFG_RX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
		desc_p->dmamac_addr = dev_phys_to_bus(priv->dev,
				(ulong)&rxbuffs[idx * CFG_ETH_BUFSIZE]);
		desc_p->dmamac_next = dev_phys_to_bus(priv->dev,
				(ulong)&desc_table_p[idx + 1]);

		desc_p->dmamac_cntl =
			(MAC_MAX_FRAME_SZ & DESC_RXCTRL_SIZE1MASK) |
				      DESC_RXCTRL_RXCHAIN;

		desc_p->txrx_status = DESC_RXSTS_OWNBYDMA;
	}

	/* Correcting the last pointer of the chain */
	desc_p->dmamac_next = dev_phys_to_bus(priv->dev, (ulong)&desc_table_p[0]);

	/* Flush all Rx buffer descriptors at once */
	flush_dcache_range((ulong)priv->rx_mac_descrtable,
			   (ulong)priv->rx_mac_descrtable +
			   sizeof(priv->rx_mac_descrtable));

	writel(dev_phys_to_bus(priv->dev, (ulong)&desc_table_p[0]),
			&dma_p->rxdesclistaddr);
	priv->rx_currdescnum = 0;
}

static int _dw_write_hwaddr(struct dw_eth_dev *priv, u8 *mac_id)
{
	struct eth_mac_regs *mac_p = priv->mac_regs_p;
	u32 macid_lo, macid_hi;

	macid_lo = mac_id[0] + (mac_id[1] << 8) + (mac_id[2] << 16) +
		   (mac_id[3] << 24);
	macid_hi = mac_id[4] + (mac_id[5] << 8);

	writel(macid_hi, &mac_p->macaddr0hi);
	writel(macid_lo, &mac_p->macaddr0lo);

	return 0;
}

extern uint8_t g_gmac_loopback;


static int dw_adjust_link(struct dw_eth_dev *priv, struct eth_mac_regs *mac_p,
			  struct phy_device *phydev)
{
#ifdef CONFIG_ARCH_NPCM8XX
	unsigned int start;
#endif
	u32 conf = readl(&mac_p->conf) | FRAMEBURSTENABLE | DISABLERXOWN;
	if (1 == g_gmac_loopback)
	{
		conf |= BIT(12);
		conf &= ~(DISABLERXOWN);
	}
	else if (2 == g_gmac_loopback)
	{
		conf &= ~(DISABLERXOWN);
	}
	//printf("initial conf=0x%x\n", readl(&mac_p->conf));

	if (!phydev->link) {
		printf("%s: No link.\n", phydev->dev->name);
		return 0;
	}

	if (phydev->speed != 1000)
		conf |= MII_PORTSELECT;
	else
		conf &= ~MII_PORTSELECT;

	if (phydev->speed == 100)
		conf |= FES_100;

	if (phydev->duplex)
		conf |= FULLDPLXMODE;

//	printf("before delay\n");
//	udelay(6000);
//	printf("after delay conf=0x%x\n", conf);
	writel(conf, &mac_p->conf);

	printf("Speed: %d, %s duplex%s\n", phydev->speed,
	       (phydev->duplex) ? "full" : "half",
	       (phydev->port == PORT_FIBRE) ? ", fiber mode" : "");

#ifdef CONFIG_ARCH_NPCM8XX
	if (phydev->interface == PHY_INTERFACE_MODE_SGMII) {
		ulong start;

		/* Indirect access to SR_MII_MMD registers */
		writew((SR_MII_MMD >> 9), PCS_BA + PCS_IND_AC);
		/* Disable SGMII PCS Auto-Negotiation */
		clrbits_le16(PCS_BA + SR_MII_MMD_CTRL, BIT(12));

		switch (phydev->speed) {
		    case SPEED_1000:
			/* SR_MII_CTRL.SS6=1 */
			setbits_le16(PCS_BA, BIT(6));
			/* SR_MII_CTRL.SS13=0 */
			clrbits_le16(PCS_BA, BIT(13));
			break;
		    case SPEED_100:
			/* SR_MII_CTRL.SS6=0 */
			clrbits_le16(PCS_BA, BIT(6));
			/* SR_MII_CTRL.SS13=1 */
			setbits_le16(PCS_BA, BIT(13));
			break;
		    case SPEED_10:
			/* SR_MII_CTRL.SS6=0 */
			clrbits_le16(PCS_BA, BIT(6));
			/* SR_MII_CTRL.SS13=0 */
			clrbits_le16(PCS_BA, BIT(13));
			break;
		    default:
			break;
		}

		printf("SGMII PHY Wait for link up \n");
		/* SGMII PHY Wait for link up */
		start = get_timer(0);
		while (!(readw(PCS_BA + SR_MII_MMD_STS) & BIT(2))) {
			if (get_timer(start) >= LINK_UP_TIMEOUT) {
				printf("PHY link up timeout\n");
				return -ETIMEDOUT;
			}
			mdelay(1);
		};
	}
	/* Pass all Multicast Frames */
	setbits_le32(&mac_p->framefilt, BIT(4));
#endif

	return 0;
}

static void _dw_eth_halt(struct dw_eth_dev *priv)
{
	struct eth_mac_regs *mac_p = priv->mac_regs_p;
	struct eth_dma_regs *dma_p = priv->dma_regs_p;

	writel(readl(&mac_p->conf) & ~(RXENABLE | TXENABLE), &mac_p->conf);
	writel(readl(&dma_p->opmode) & ~(RXSTART | TXSTART), &dma_p->opmode);

	phy_shutdown(priv->phydev);
}

int designware_eth_init(struct dw_eth_dev *priv, u8 *enetaddr)
{
	struct eth_mac_regs *mac_p = priv->mac_regs_p;
	struct eth_dma_regs *dma_p = priv->dma_regs_p;
	ulong start;
	int ret;

	debug("dw_eth_init1\n");

	writel(readl(&dma_p->busmode) | DMAMAC_SRST, &dma_p->busmode);
	debug("dw_eth_init2\n");

	/*
	 * When a MII PHY is used, we must set the PS bit for the DMA
	 * reset to succeed.
	 */
	if (priv->phydev->interface == PHY_INTERFACE_MODE_MII)
		writel(readl(&mac_p->conf) | MII_PORTSELECT, &mac_p->conf);
	else
		writel(readl(&mac_p->conf) & ~MII_PORTSELECT, &mac_p->conf);
	debug("dw_eth_init3\n");

	start = get_timer(0);
	while (readl(&dma_p->busmode) & DMAMAC_SRST) {
		if (get_timer(start) >= CFG_MACRESET_TIMEOUT) {
			printf("DMA reset timeout\n");
			return -ETIMEDOUT;
		}

		mdelay(1);
	};
	debug("dw_eth_init4\n");

	/*
	 * Soft reset above clears HW address registers.
	 * So we have to set it here once again.
	 */
	_dw_write_hwaddr(priv, enetaddr);

	rx_descs_init(priv);
	tx_descs_init(priv);

	writel(FIXEDBURST | PRIORXTX_41 | DMA_PBL, &dma_p->busmode);

#ifndef CONFIG_DW_MAC_FORCE_THRESHOLD_MODE
	writel(readl(&dma_p->opmode) | FLUSHTXFIFO | STOREFORWARD,
	       &dma_p->opmode);
#else
	writel(readl(&dma_p->opmode) | FLUSHTXFIFO,
	       &dma_p->opmode);
#endif

	writel(readl(&dma_p->opmode) | RXSTART | TXSTART, &dma_p->opmode);
	debug("dw_eth_init5\n");

#ifdef CONFIG_DW_AXI_BURST_LEN
	writel((CONFIG_DW_AXI_BURST_LEN & 0x1FF >> 1), &dma_p->axibus);
#endif

	/* Start up the PHY */
	ret = phy_startup(priv->phydev);
	if (ret) {
		printf("Could not initialize PHY %s\n",
		       priv->phydev->dev->name);
		return ret;
	}
	debug("dw_eth_init6\n");

	ret = dw_adjust_link(priv, mac_p, priv->phydev);
	if (ret)
		return ret;

	return 0;
}

int designware_eth_enable(struct dw_eth_dev *priv)
{
	struct eth_mac_regs *mac_p = priv->mac_regs_p;

	if (!priv->phydev->link)
		return -EIO;

	writel(readl(&mac_p->conf) | RXENABLE | TXENABLE, &mac_p->conf);

	return 0;
}

#define ETH_ZLEN	60

static int _dw_eth_send(struct dw_eth_dev *priv, void *packet, int length)
{
	struct eth_dma_regs *dma_p = priv->dma_regs_p;
	u32 desc_num = priv->tx_currdescnum;
	struct dmamacdescr *desc_p = &priv->tx_mac_descrtable[desc_num];
	ulong desc_start = (ulong)desc_p;
	ulong desc_end = desc_start +
		roundup(sizeof(*desc_p), ARCH_DMA_MINALIGN);
	ulong data_start = dev_bus_to_phys(priv->dev, desc_p->dmamac_addr);
	ulong data_end = data_start + roundup(length, ARCH_DMA_MINALIGN);
	/*
	 * Strictly we only need to invalidate the "txrx_status" field
	 * for the following check, but on some platforms we cannot
	 * invalidate only 4 bytes, so we flush the entire descriptor,
	 * which is 16 bytes in total. This is safe because the
	 * individual descriptors in the array are each aligned to
	 * ARCH_DMA_MINALIGN and padded appropriately.
	 */
	invalidate_dcache_range(desc_start, desc_end);

	/* Check if the descriptor is owned by CPU */
	if (desc_p->txrx_status & DESC_TXSTS_OWNBYDMA) {
		printf("CPU not owner of tx frame\n");
		return -EPERM;
	}

	memcpy((void *)data_start, packet, length);
	if (length < ETH_ZLEN) {
		memset(&((char *)data_start)[length], 0, ETH_ZLEN - length);
		length = ETH_ZLEN;
	}

	/* Flush data to be sent */
	flush_dcache_range(data_start, data_end);

#if defined(CONFIG_DW_ALTDESCRIPTOR)
	desc_p->txrx_status |= DESC_TXSTS_TXFIRST | DESC_TXSTS_TXLAST;
	desc_p->dmamac_cntl = (desc_p->dmamac_cntl & ~DESC_TXCTRL_SIZE1MASK) |
			      ((length << DESC_TXCTRL_SIZE1SHFT) &
			      DESC_TXCTRL_SIZE1MASK);

	desc_p->txrx_status &= ~(DESC_TXSTS_MSK);
	desc_p->txrx_status |= DESC_TXSTS_OWNBYDMA;
#else
	desc_p->dmamac_cntl = (desc_p->dmamac_cntl & ~DESC_TXCTRL_SIZE1MASK) |
			      ((length << DESC_TXCTRL_SIZE1SHFT) &
			      DESC_TXCTRL_SIZE1MASK) | DESC_TXCTRL_TXLAST |
			      DESC_TXCTRL_TXFIRST;

	desc_p->txrx_status = DESC_TXSTS_OWNBYDMA;
#endif

	/* Flush modified buffer descriptor */
	flush_dcache_range(desc_start, desc_end);

	/* Test the wrap-around condition. */
	if (++desc_num >= CFG_TX_DESCR_NUM)
		desc_num = 0;

	priv->tx_currdescnum = desc_num;

	uint32_t *p = (uint32_t *)desc_p;
//	printf("desc =" );
//	for (int i = 0; i < sizeof(struct dmamacdescr); i = i + 4 )
//	{
//		debug("0x%X ", *p );
//		p++;
//	}
//	debug("\n");

	/* Start the transmission */
	writel(POLL_DATA, &dma_p->txpolldemand);

	return 0;
}

static int _dw_eth_recv(struct dw_eth_dev *priv, uchar **packetp)
{
	u32 status, desc_num = priv->rx_currdescnum;
	struct dmamacdescr *desc_p = &priv->rx_mac_descrtable[desc_num];
	int length = -EAGAIN;
	ulong desc_start = (ulong)desc_p;
	ulong desc_end = desc_start +
		roundup(sizeof(*desc_p), ARCH_DMA_MINALIGN);
	ulong data_start = dev_bus_to_phys(priv->dev, desc_p->dmamac_addr);
	ulong data_end;

	/* Invalidate entire buffer descriptor */
	invalidate_dcache_range(desc_start, desc_end);

	status = desc_p->txrx_status;

	/* Check  if the owner is the CPU */
	if (!(status & DESC_RXSTS_OWNBYDMA)) {

		length = (status & DESC_RXSTS_FRMLENMSK) >>
			 DESC_RXSTS_FRMLENSHFT;

		/* Invalidate received data */
		data_end = data_start + roundup(length, ARCH_DMA_MINALIGN);
		invalidate_dcache_range(data_start, data_end);
		*packetp = (uchar *)(ulong)dev_bus_to_phys(priv->dev,
				desc_p->dmamac_addr);
	}

	return length;
}

static int _dw_free_pkt(struct dw_eth_dev *priv)
{
	u32 desc_num = priv->rx_currdescnum;
	struct dmamacdescr *desc_p = &priv->rx_mac_descrtable[desc_num];
	ulong desc_start = (ulong)desc_p;
	ulong desc_end = desc_start +
		roundup(sizeof(*desc_p), ARCH_DMA_MINALIGN);
	ulong data_start = dev_bus_to_phys(priv->dev, desc_p->dmamac_addr);
	ulong data_end = data_start + roundup(CFG_ETH_BUFSIZE, ARCH_DMA_MINALIGN);

	/* Invalidate the descriptor buffer data */
	invalidate_dcache_range(data_start, data_end);

	/*
	 * Make the current descriptor valid again and go to
	 * the next one
	 */
	desc_p->txrx_status |= DESC_RXSTS_OWNBYDMA;

	/* Flush only status field - others weren't changed */
	flush_dcache_range(desc_start, desc_end);

	/* Test the wrap-around condition. */
	if (++desc_num >= CFG_RX_DESCR_NUM)
		desc_num = 0;
	priv->rx_currdescnum = desc_num;

	return 0;
}

static int dw_phy_init(struct dw_eth_dev *priv, void *dev)
{
	struct phy_device *phydev;
	int ret;

	if (IS_ENABLED(CONFIG_DM_ETH_PHY))
		eth_phy_set_mdio_bus(dev, NULL);

#if IS_ENABLED(CONFIG_DM_MDIO)
	phydev = dm_eth_phy_connect(dev);
	if (!phydev)
		return -ENODEV;
#else
	int phy_addr = -1;

	if (IS_ENABLED(CONFIG_DM_ETH_PHY))
		phy_addr = eth_phy_get_addr(dev);

#ifdef CONFIG_PHY_ADDR
	phy_addr = CONFIG_PHY_ADDR;
#endif

	phydev = phy_connect(priv->bus, phy_addr, dev, priv->interface);
	if (!phydev)
		return -ENODEV;
#endif

	if (IS_ENABLED(CONFIG_ARCH_NPCM8XX) &&
	    phydev->interface == PHY_INTERFACE_MODE_SGMII)
		phy_reset(phydev);

	phydev->supported &= PHY_GBIT_FEATURES;
	if (priv->max_speed) {
		ret = phy_set_supported(phydev, priv->max_speed);
		if (ret)
			return ret;
	}
	phydev->advertising = phydev->supported;

	priv->phydev = phydev;
	phy_config(phydev);

	return 0;
}

static int designware_eth_start(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct dw_eth_dev *priv = dev_get_priv(dev);
	int ret;

	ret = designware_eth_init(priv, pdata->enetaddr);
	if (ret)
		return ret;
	ret = designware_eth_enable(priv);
	if (ret)
		return ret;

	return 0;
}

int designware_eth_send(struct udevice *dev, void *packet, int length)
{
	struct dw_eth_dev *priv = dev_get_priv(dev);

	return _dw_eth_send(priv, packet, length);
}

int designware_eth_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct dw_eth_dev *priv = dev_get_priv(dev);

	return _dw_eth_recv(priv, packetp);
}

int designware_eth_free_pkt(struct udevice *dev, uchar *packet, int length)
{
	struct dw_eth_dev *priv = dev_get_priv(dev);

	return _dw_free_pkt(priv);
}

void designware_eth_stop(struct udevice *dev)
{
	struct dw_eth_dev *priv = dev_get_priv(dev);

	return _dw_eth_halt(priv);
}

int designware_eth_write_hwaddr(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct dw_eth_dev *priv = dev_get_priv(dev);

	return _dw_write_hwaddr(priv, pdata->enetaddr);
}

static int designware_eth_bind(struct udevice *dev)
{
	if (IS_ENABLED(CONFIG_PCI)) {
		static int num_cards;
		char name[20];

		/* Create a unique device name for PCI type devices */
		if (device_is_on_pci_bus(dev)) {
			sprintf(name, "eth_designware#%u", num_cards++);
			device_set_name(dev, name);
		}
	}

	return 0;
}


#define PINMUX_BASE 0x45A00000
#define PINMUX_PIN_195 (0x45A00000 + 0 + (195 * 8))
#define MUX_SEL_PIN195_RMII1_TXD1_FUNC  1
#define PINMUX_PIN_194 (0x45A00000 + 0 + (194 * 8))
#define MUX_SEL_PIN194_RMII1_TXD0_FUNC  1
#define PINMUX_PIN_198 (0x45A00000 + 0 + (198 * 8))
#define MUX_SEL_PIN198_RMII1_RXD1_FUNC  1
#define PINMUX_PIN_197 (0x45A00000 + 0 + (197 * 8))
#define MUX_SEL_PIN197_RMII1_RXD0_FUNC  1
#define PINMUX_PIN_199 (0x45A00000 + 0 + (199 * 8))
#define MUX_SEL_PIN199_RMII1_CRSDV_FUNC  1
#define PINMUX_PIN_196 (0x45A00000 + 0 + (196 * 8))
#define MUX_SEL_PIN196_RMII1_TXEN_FUNC  1
#define PINMUX_PIN_200 (0x45A00000 + 0 + (200 * 8))
#define MUX_SEL_PIN200_RMII1_CK_FUNC  1

#define PINMUX_PIN_216 (0x45A00000 + 0 + (216 * 8))
#define MUX_SEL_PIN216_RGMII_TXD3_FUNC  1
#define PINMUX_PIN_217 (0x45A00000 + 0 + (217 * 8))
#define MUX_SEL_PIN217_RGMII_TXD2_FUNC  1
#define PINMUX_PIN_218 (0x45A00000 + 0 + (218 * 8))
#define MUX_SEL_PIN218_RGMII_TXD1_FUNC  1
#define PINMUX_PIN_219 (0x45A00000 + 0 + (219 * 8))
#define MUX_SEL_PIN219_RGMII_TXD0_FUNC  1
#define PINMUX_PIN_220 (0x45A00000 + 0 + (220 * 8))
#define MUX_SEL_PIN220_RGMII_TXC_FUNC  1
#define PINMUX_PIN_221 (0x45A00000 + 0 + (221 * 8))
#define MUX_SEL_PIN221_RGMII_TXCTL0_FUNC  1
#define PINMUX_PIN_222 (0x45A00000 + 0 + (222 * 8))
#define MUX_SEL_PIN222_RGMII_RXD3_FUNC  1
#define PINMUX_PIN_223 (0x45A00000 + 0 + (223 * 8))
#define MUX_SEL_PIN223_RGMII_RXD2_FUNC  1
#define PINMUX_PIN_224 (0x45A00000 + 0 + (224 * 8))
#define MUX_SEL_PIN224_RGMII_RXD1_FUNC  1
#define PINMUX_PIN_225 (0x45A00000 + 0 + (225 * 8))
#define MUX_SEL_PIN225_RGMII_RXD0_FUNC  1
#define PINMUX_PIN_226 (0x45A00000 + 0 + (226 * 8))
#define MUX_SEL_PIN226_RGMII_RXC_FUNC  1
#define PINMUX_PIN_227 (0x45A00000 + 0 + (227 * 8))
#define MUX_SEL_PIN227_RGMII_RXCTL_FUNC  1
#define PINMUX_PIN_230 (0x45A00000 + 0 + (230 * 8))
#define MUX_SEL_PIN230_RGMII_REFCLK_FUNC  1


#define MUX_SEL_POS  0

static void temp_set_single_pinmux(uint32_t reg, uint32_t func)
{
	uint32_t reg_val;
	reg_val = readl(reg);
	reg_val &= ~(0x7 << MUX_SEL_POS);
	reg_val |=  (func << MUX_SEL_POS);
	writel(reg_val, reg);
}


static void temp_set_pinmux(void)
{
	//GMAC1
	temp_set_single_pinmux(PINMUX_PIN_195, MUX_SEL_PIN195_RMII1_TXD1_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_194, MUX_SEL_PIN194_RMII1_TXD0_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_198, MUX_SEL_PIN198_RMII1_RXD1_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_197, MUX_SEL_PIN197_RMII1_RXD0_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_199, MUX_SEL_PIN199_RMII1_CRSDV_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_196, MUX_SEL_PIN196_RMII1_TXEN_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_200, MUX_SEL_PIN200_RMII1_CK_FUNC);

	//GMAC2
	temp_set_single_pinmux(PINMUX_PIN_216, MUX_SEL_PIN216_RGMII_TXD3_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_217, MUX_SEL_PIN217_RGMII_TXD2_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_218, MUX_SEL_PIN218_RGMII_TXD1_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_219, MUX_SEL_PIN219_RGMII_TXD0_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_220, MUX_SEL_PIN220_RGMII_TXC_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_221, MUX_SEL_PIN221_RGMII_TXCTL0_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_222, MUX_SEL_PIN222_RGMII_RXD3_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_223, MUX_SEL_PIN223_RGMII_RXD2_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_224, MUX_SEL_PIN224_RGMII_RXD1_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_225, MUX_SEL_PIN225_RGMII_RXD0_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_226, MUX_SEL_PIN226_RGMII_RXC_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_227, MUX_SEL_PIN227_RGMII_RXCTL_FUNC);
	temp_set_single_pinmux(PINMUX_PIN_230, MUX_SEL_PIN230_RGMII_REFCLK_FUNC);
}

#define RST_BA 0x45990000

#define ACTIVE_HOLD_RELEASE_REG_B (RST_BA + 0x1640)
#define ACTIVE_HOLD_RELEASE_REG_B_XGMAC_POS 25
#define ACTIVE_HOLD_RELEASE_REG_B_GMAC1_POS 26
#define ACTIVE_HOLD_RELEASE_REG_B_GMAC2_POS 27

#define RESET_HOLD_REG_B (RST_BA + 0x440)
#define RESET_HOLD_REG_B_XGMAC_POS 25
#define RESET_HOLD_REG_B_GMAC1_POS 26
#define RESET_HOLD_REG_B_GMAC2_POS 27

#define GMACSS_EXT_REG_BA 0x44900000
#define GMACSS_GMAC_CTRL_REG (GMACSS_EXT_REG_BA + 0x20)
#define GMACSS_GMAC_CTRL_REG_GMAC1_CONFIG_DONE_POS 2
#define GMACSS_GMAC_CTRL_REG_GMAC2_CONFIG_DONE_POS 3

//#define GMACSS_XGMAC_CTRL_REG (GMACSS_EXT_REG_BA + 0x2C)
//#define GMACSS_XGMAC_CTRL_REG_XGMAC_CONFIG_DONE_POS 0

void temp_nuvoton_reset(void)
{
	uint32_t reg_val;
	static uint8_t reset_done = 0;

	if (reset_done) return;

	reg_val = (1 << ACTIVE_HOLD_RELEASE_REG_B_GMAC1_POS) |
			(1 << ACTIVE_HOLD_RELEASE_REG_B_GMAC2_POS)|
			(1 << ACTIVE_HOLD_RELEASE_REG_B_XGMAC_POS) ;
	writel(reg_val, ACTIVE_HOLD_RELEASE_REG_B);

	reg_val = readl(RESET_HOLD_REG_B);
	reg_val &= ~((1 << RESET_HOLD_REG_B_GMAC1_POS) |
			(1 << RESET_HOLD_REG_B_GMAC2_POS)|
			(1 << RESET_HOLD_REG_B_XGMAC_POS));
	writel(reg_val, RESET_HOLD_REG_B);

	reg_val = readl(GMACSS_GMAC_CTRL_REG);
	reg_val |= (1 << GMACSS_GMAC_CTRL_REG_GMAC1_CONFIG_DONE_POS) |
			(1 << GMACSS_GMAC_CTRL_REG_GMAC2_CONFIG_DONE_POS);
	writel(reg_val, GMACSS_GMAC_CTRL_REG);
//
//	reg_val = readl(GMACSS_XGMAC_CTRL_REG);
//	reg_val |= (1 << GMACSS_XGMAC_CTRL_REG_XGMAC_CONFIG_DONE_POS);
//	writel(reg_val, GMACSS_XGMAC_CTRL_REG);
	reset_done = 1;
}

int designware_eth_probe(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct dw_eth_dev *priv = dev_get_priv(dev);
	phys_addr_t iobase = pdata->iobase;
	void *ioaddr;
	int ret, err;
	struct reset_ctl_bulk reset_bulk;

	temp_nuvoton_reset();
	temp_set_pinmux();

#ifdef CONFIG_CLK
	int i, clock_nb;

	priv->clock_count = 0;
	clock_nb = dev_count_phandle_with_args(dev, "clocks", "#clock-cells",
					       0);
	if (clock_nb > 0) {
		priv->clocks = devm_kcalloc(dev, clock_nb, sizeof(struct clk),
					    GFP_KERNEL);
		if (!priv->clocks)
			return -ENOMEM;

		for (i = 0; i < clock_nb; i++) {
			err = clk_get_by_index(dev, i, &priv->clocks[i]);
			if (err < 0)
				break;

			err = clk_enable(&priv->clocks[i]);
			if (err && err != -ENOSYS && err != -ENOTSUPP) {
				pr_err("failed to enable clock %d\n", i);
				goto clk_err;
			}
			priv->clock_count++;
		}
	} else if (clock_nb != -ENOENT) {
		pr_err("failed to get clock phandle(%d)\n", clock_nb);
		return clock_nb;
	}
#endif

#if defined(CONFIG_DM_REGULATOR)
	struct udevice *phy_supply;
#ifdef CONFIG_ARCH_NPCM8XX
	int phy_uv;
#endif

	ret = device_get_supply_regulator(dev, "phy-supply",
					  &phy_supply);
	if (ret) {
		debug("%s: No phy supply\n", dev->name);
	} else {
		ret = regulator_set_enable_if_allowed(phy_supply, true);
		if (ret) {
			puts("Error enabling phy supply\n");
			return ret;
		}
#if IS_ENABLED(CONFIG_ARCH_NPCM8XX)
		phy_uv = dev_read_u32_default(dev, "phy-supply-microvolt", 0);
		if (phy_uv) {
			ret = regulator_set_value(phy_supply, phy_uv);
			if (ret) {
				puts("Error setting phy voltage\n");
				return ret;
			}
		}
#endif
	}
#endif
	debug("dw_eth_probe1\n");

	ret = reset_get_bulk(dev, &reset_bulk);
	if (ret)
		dev_warn(dev, "Can't get reset: %d\n", ret);
	else {
#ifdef CONFIG_ARCH_NPCM8XX
		reset_assert_bulk(&reset_bulk);
#endif
		reset_deassert_bulk(&reset_bulk);
	}

	/*
	 * If we are on PCI bus, either directly attached to a PCI root port,
	 * or via a PCI bridge, fill in plat before we probe the hardware.
	 */
	if (IS_ENABLED(CONFIG_PCI) && device_is_on_pci_bus(dev)) {
		u32 pcibase;

		dm_pci_read_config32(dev, PCI_BASE_ADDRESS_0, &pcibase);
		pcibase &= PCI_BASE_ADDRESS_MEM_MASK;

		iobase = dm_pci_mem_to_phys(dev, pcibase);
		pdata->iobase = iobase;
		pdata->phy_interface = PHY_INTERFACE_MODE_RMII;
	}

	debug("%s, iobase=%pa, priv=%p\n", __func__, &iobase, priv);
	ioaddr = phys_to_virt(iobase);
	priv->mac_regs_p = (struct eth_mac_regs *)ioaddr;
	priv->dma_regs_p = (struct eth_dma_regs *)(ioaddr + DW_DMA_BASE_OFFSET);
	priv->interface = pdata->phy_interface;
	priv->max_speed = pdata->max_speed;
	debug("dw_eth_probe2\n");

#if IS_ENABLED(CONFIG_BITBANGMII) && IS_ENABLED(CONFIG_DM_GPIO)
	if (dev_read_bool(dev, "snps,bitbang-mii")) {
		ret = dw_bb_mdio_init(dev->name, dev);
		if (ret) {
			err = ret;
			goto mdio_err;
		}
	} else
#endif
	{
#if IS_ENABLED(CONFIG_DM_MDIO)
		ret = dw_dm_mdio_init(dev->name, dev);
#else
		ret = dw_mdio_init(dev->name, dev);
#endif
		if (ret) {
			err = ret;
			goto mdio_err;
		}
		priv->bus = miiphy_get_dev_by_name(dev->name);
		priv->dev = dev;
	}
	debug("dw_eth_probe3\n");


	ret = dw_phy_init(priv, dev);
	debug("%s, ret=%d\n", __func__, ret);
	if (!ret)
		return 0;

	/* continue here for cleanup if no PHY found */
	err = ret;
	mdio_unregister(priv->bus);
	mdio_free(priv->bus);
mdio_err:

#ifdef CONFIG_CLK
clk_err:
	ret = clk_release_all(priv->clocks, priv->clock_count);
	if (ret)
		pr_err("failed to disable all clocks\n");

#endif
	return err;
}

static int designware_eth_remove(struct udevice *dev)
{
	struct dw_eth_dev *priv = dev_get_priv(dev);

	free(priv->phydev);
	mdio_unregister(priv->bus);
	mdio_free(priv->bus);

#ifdef CONFIG_CLK
	return clk_release_all(priv->clocks, priv->clock_count);
#else
	return 0;
#endif
}

const struct eth_ops designware_eth_ops = {
	.start			= designware_eth_start,
	.send			= designware_eth_send,
	.recv			= designware_eth_recv,
	.free_pkt		= designware_eth_free_pkt,
	.stop			= designware_eth_stop,
	.write_hwaddr		= designware_eth_write_hwaddr,
};

int designware_eth_of_to_plat(struct udevice *dev)
{
	struct dw_eth_pdata *dw_pdata = dev_get_plat(dev);
#if CONFIG_IS_ENABLED(DM_GPIO)
	struct dw_eth_dev *priv = dev_get_priv(dev);
#endif
	struct eth_pdata *pdata = &dw_pdata->eth_pdata;
#if CONFIG_IS_ENABLED(DM_GPIO)
	int reset_flags = GPIOD_IS_OUT;
#endif
	int ret = 0;

	pdata->iobase = dev_read_addr(dev);
	pdata->phy_interface = dev_read_phy_mode(dev);
	if (pdata->phy_interface == PHY_INTERFACE_MODE_NA)
		return -EINVAL;

	pdata->max_speed = dev_read_u32_default(dev, "max-speed", 0);

#if CONFIG_IS_ENABLED(DM_GPIO)
	if (dev_read_bool(dev, "snps,reset-active-low"))
		reset_flags |= GPIOD_ACTIVE_LOW;

	ret = gpio_request_by_name(dev, "snps,reset-gpio", 0,
		&priv->reset_gpio, reset_flags);
	if (ret == 0) {
		ret = dev_read_u32_array(dev, "snps,reset-delays-us",
					 dw_pdata->reset_delays, 3);
	} else if (ret == -ENOENT) {
		ret = 0;
	}
#endif

	return ret;
}

static const struct udevice_id designware_eth_ids[] = {
	{ .compatible = "allwinner,sun7i-a20-gmac" },
	{ .compatible = "amlogic,meson6-dwmac" },
	{ .compatible = "st,stm32-dwmac" },
	{ .compatible = "snps,arc-dwmac-3.70a" },
	{ .compatible = "sophgo,cv1800b-dwmac" },
	{ }
};

U_BOOT_DRIVER(eth_designware) = {
	.name	= "eth_designware",
	.id	= UCLASS_ETH,
	.of_match = designware_eth_ids,
	.of_to_plat = designware_eth_of_to_plat,
	.bind	= designware_eth_bind,
	.probe	= designware_eth_probe,
	.remove	= designware_eth_remove,
	.ops	= &designware_eth_ops,
	.priv_auto	= sizeof(struct dw_eth_dev),
	.plat_auto	= sizeof(struct dw_eth_pdata),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};

static struct pci_device_id supported[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_QRK_EMAC) },
	{ }
};

U_BOOT_PCI_DEVICE(eth_designware, supported);

