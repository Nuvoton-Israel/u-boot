#include <common.h>
#include <dm.h>
#include <errno.h>
#include <miiphy.h>
#include <malloc.h>
#include <pci.h>
#include <linux/compiler.h>
#include <linux/err.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/arch/clock.h>
#include "npcm750_eth.h"
#include <cpu_func.h>
#include <linux/iopoll.h>
#include <serial.h>

DECLARE_GLOBAL_DATA_PTR;

static int npcm750_mdio_read(struct mii_dev *bus, int addr, int devad, int regs)
{
	struct npcm750_eth_dev *priv = (struct npcm750_eth_dev *)bus->priv;
	struct emc_regs *reg = priv->emc_regs_p;
	u32 start, val;
	int timeout = CONFIG_MDIO_TIMEOUT;

	val = (addr << 0x08) | regs | PHYBUSY | (MIIDA_MDCCR_60 << 20);
	writel(val, &reg->miida);

	start = get_timer(0);
	while (get_timer(start) < timeout) {
		if (!(readl(&reg->miida) & PHYBUSY)) {
			val = readl(&reg->miid);
			return val;
		}
		udelay(10);
	};
	return -ETIMEDOUT;
}

static int npcm750_mdio_write(struct mii_dev *bus, int addr, int devad, int regs,
			u16 val)
{
	struct npcm750_eth_dev *priv = (struct npcm750_eth_dev *)bus->priv;
	struct emc_regs *reg = priv->emc_regs_p;
	ulong start;
	int ret = -ETIMEDOUT, timeout = CONFIG_MDIO_TIMEOUT;

	writel(val, &reg->miid);
	writel((addr << 0x08) | regs | PHYBUSY | PHYWR | (MIIDA_MDCCR_60 << 20), &reg->miida);

	start = get_timer(0);
	while (get_timer(start) < timeout) {
		if (!(readl(&reg->miida) & PHYBUSY)) {
			ret = 0;
			break;
		}
		udelay(10);
	};
	return ret;
}

static int npcm750_mdio_reset(struct mii_dev *bus)
{
	return 0;
}

static int npcm750_mdio_init(const char *name, struct npcm750_eth_dev *priv)
{
	struct emc_regs *reg = priv->emc_regs_p;
	struct mii_dev *bus = mdio_alloc();
	if (!bus) {
		printf("Failed to allocate MDIO bus\n");
		return -ENOMEM;
	}

	bus->read = npcm750_mdio_read;
	bus->write = npcm750_mdio_write;
	snprintf(bus->name, sizeof(bus->name), "%s", name);
	bus->reset = npcm750_mdio_reset;

	bus->priv = (void *)priv;

	writel(readl(&reg->mcmdr) | MCMDR_ENMDC, &reg->mcmdr);
	return mdio_register(bus);
}

static void npcm750_tx_descs_init(struct npcm750_eth_dev *priv)
{
	struct emc_regs *reg = priv->emc_regs_p;
	struct npcm750_txbd *desc_table_p = &priv->tdesc[0];
	struct npcm750_txbd *desc_p;
	u8 *txbuffs = &priv->txbuffs[0];
	u32 idx;

	writel((u32)desc_table_p, &reg->txdlsa);
	priv->curr_txd = desc_table_p;

	for (idx = 0; idx < CONFIG_TX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
		desc_p->buffer = (u32)&txbuffs[idx * PKTSIZE_ALIGN];
		desc_p->sl = 0;
		desc_p->mode = 0;
		desc_p->mode = TX_OWEN_CPU | PADDINGMODE | CRCMODE | MACTXINTEN;
		if (idx < (CONFIG_TX_DESCR_NUM - 1))
			desc_p->next = (u32)&desc_table_p[idx + 1];
		else
			desc_p->next = (u32)&priv->tdesc[0];
	}
	flush_dcache_range((ulong)&desc_table_p[0],
		(ulong)&desc_table_p[CONFIG_TX_DESCR_NUM]);
}

static void npcm750_rx_descs_init(struct npcm750_eth_dev *priv)
{
	struct emc_regs *reg = priv->emc_regs_p;
	struct npcm750_rxbd *desc_table_p = &priv->rdesc[0];
	struct npcm750_rxbd *desc_p;
	u8 *rxbuffs = &priv->rxbuffs[0];
	u32 idx;
	flush_dcache_range((ulong)priv->rxbuffs[0],
		(ulong)priv->rxbuffs[CONFIG_RX_DESCR_NUM]);

	writel((u32)desc_table_p, &reg->rxdlsa);
	priv->curr_rxd = desc_table_p;

	for (idx = 0; idx < CONFIG_RX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
		desc_p->sl = RX_OWEN_DMA;
		desc_p->buffer = (u32)&rxbuffs[idx * PKTSIZE_ALIGN];
		if (idx < (CONFIG_RX_DESCR_NUM - 1))
			desc_p->next = (u32)&desc_table_p[idx + 1];
		else
			desc_p->next = (u32)&priv->rdesc[0];
	}
	flush_dcache_range((ulong)&desc_table_p[0],
		(ulong)&desc_table_p[CONFIG_RX_DESCR_NUM]);
}

static void npcm750_set_fifo_threshold(struct npcm750_eth_dev *priv)
{
	struct emc_regs *reg = priv->emc_regs_p;
	unsigned int val;

	val = RXTHD | TXTHD | BLENGTH;
	writel(val, &reg->fftcr);
}

static void npcm750_set_global_maccmd(struct npcm750_eth_dev *priv)
{
	struct emc_regs *reg = priv->emc_regs_p;
	unsigned int val;

	val = readl(&reg->mcmdr);
	val |= MCMDR_SPCRC | MCMDR_ENMDC | MCMDR_ACP | MCMDR_NDEF;
	writel(val, &reg->mcmdr);
}

static void npcm750_set_cam(struct npcm750_eth_dev *priv,
				unsigned int x, unsigned char *pval)
{
	struct emc_regs *reg = priv->emc_regs_p;
	unsigned int msw, lsw;

	msw = (pval[0] << 24) | (pval[1] << 16) | (pval[2] << 8) | pval[3];
	lsw = (pval[4] << 24) | (pval[5] << 16);

	writel(lsw, &reg->cam0l + x * CAM_ENTRY_SIZE);
	writel(msw, &reg->cam0m + x * CAM_ENTRY_SIZE);
	writel(readl(&reg->camen) | CAM0EN, &reg->camen);
	writel(CAMCMR_ECMP | CAMCMR_ABP | CAMCMR_AUP, &reg->camcmr);
}

static void npcm750_adjust_link(struct emc_regs *reg,
			   struct phy_device *phydev)
{
	u32 val = readl(&reg->mcmdr);

	if (!phydev->link) {
		printf("%s: No link.\n", phydev->dev->name);
		return;
	}

	if (phydev->speed == 100)
		val |= MCMDR_OPMOD;
	else
		val &= ~MCMDR_OPMOD;

	if (phydev->duplex)
		val |= MCMDR_FDUP;
	else
		val &= ~MCMDR_FDUP;

	writel(val, &reg->mcmdr);

	debug("Speed: %d, %s duplex%s\n", phydev->speed,
	       (phydev->duplex) ? "full" : "half",
	       (phydev->port == PORT_FIBRE) ? ", fiber mode" : "");
}

static int npcm750_phy_init(struct npcm750_eth_dev *priv, void *dev)
{
	struct phy_device *phydev;
	int ret;
	u32 address = 0x0;

	phydev = phy_connect(priv->bus, address, dev, priv->interface);
	if (!phydev)
		return -ENODEV;

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

static int npcm750_eth_start(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct npcm750_eth_dev *priv = dev_get_priv(dev);
	struct emc_regs *reg = priv->emc_regs_p;
	u8 *enetaddr = pdata->enetaddr;
	int ret;

	writel(readl(&reg->mcmdr) & ~MCMDR_TXON  & ~MCMDR_RXON, &reg->mcmdr);

	writel(readl(&reg->mcmdr) | MCMDR_SWR, &reg->mcmdr);
	do {
		ret = readl(&reg->mcmdr);
	} while (ret & MCMDR_SWR);

	npcm750_rx_descs_init(priv);
	npcm750_tx_descs_init(priv);

	npcm750_set_cam(priv, priv->idx, enetaddr);;
	npcm750_set_global_maccmd(priv);
	npcm750_set_fifo_threshold(priv);

	/* Start up the PHY */
	ret = phy_startup(priv->phydev);
	if (ret) {
		printf("Could not initialize PHY\n");
		return ret;
	}

	npcm750_adjust_link(reg , priv->phydev);
	writel(readl(&reg->mcmdr) | MCMDR_TXON | MCMDR_RXON, &reg->mcmdr);

	return 0;
}

static int npcm750_eth_send(struct udevice *dev, void *packet, int length)
{
	struct npcm750_eth_dev *priv = dev_get_priv(dev);
	struct emc_regs *reg = priv->emc_regs_p;
	struct npcm750_txbd *desc_p;
	struct npcm750_txbd *next_desc_p;

	desc_p = priv->curr_txd;

	invalidate_dcache_range((ulong)desc_p, (ulong)(desc_p+1));
	/* Check if the descriptor is owned by CPU */
	if (desc_p->mode & TX_OWEN_DMA) {
		next_desc_p = (struct npcm750_txbd *)desc_p->next;

        while ((next_desc_p != desc_p) && (next_desc_p->mode & TX_OWEN_DMA)) {
            next_desc_p = (struct npcm750_txbd *)next_desc_p->next;
        }

        if (next_desc_p == desc_p) {
			struct emc_regs *reg = priv->emc_regs_p;
			writel(0, &reg->tsdr);
			serial_printf("TX: overflow and exit\n");
			return -EPERM;
        }

		desc_p = next_desc_p;
	}

	memcpy((void *)desc_p->buffer, packet, length);
	flush_dcache_range((ulong)desc_p->buffer,
		(ulong)desc_p->buffer + roundup(length, ARCH_DMA_MINALIGN));
	desc_p->sl = 0;
	desc_p->sl = length & TX_STAT_TBC;
	desc_p->mode = TX_OWEN_DMA | PADDINGMODE | CRCMODE;
	flush_dcache_range((ulong)desc_p, (ulong)(desc_p+1));

	if (!(readl(&reg->mcmdr) & MCMDR_TXON)) {
		writel(readl(&reg->mcmdr) | MCMDR_TXON, &reg->mcmdr);
	}
	priv->curr_txd = (struct npcm750_txbd *)priv->curr_txd->next;

	writel(0, &reg->tsdr);
	return 0;
}

static int npcm750_eth_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct npcm750_eth_dev *priv = dev_get_priv(dev);
	struct npcm750_rxbd *desc_p;
	struct npcm750_rxbd *next_desc_p;
	int length = -1;

	desc_p = priv->curr_rxd;
	invalidate_dcache_range((ulong)desc_p, (ulong)(desc_p+1));

	if ((desc_p->sl & RX_STAT_OWNER) == RX_OWEN_DMA) {
		next_desc_p = (struct npcm750_rxbd *)desc_p->next;
		while ((next_desc_p != desc_p) && ((next_desc_p->sl & RX_STAT_OWNER) == RX_OWEN_CPU)) {
			next_desc_p = (struct npcm750_rxbd *)next_desc_p->next;
		}

		if (next_desc_p == desc_p) {
			struct emc_regs *reg = priv->emc_regs_p;
			writel(0, &reg->rsdr);
			serial_printf("RX: overflow and exit\n");
			return -EPERM;
        }
		desc_p = next_desc_p;
	}

	/* Check if the descriptor is owned by CPU */
	if ((desc_p->sl & RX_STAT_OWNER) == RX_OWEN_CPU) {
		if (desc_p->sl & RX_STAT_RXGD) {
			length = desc_p->sl & RX_STAT_RBC;
			invalidate_dcache_range((ulong)desc_p->buffer,
				(ulong)(desc_p->buffer + roundup(length, ARCH_DMA_MINALIGN)));
			*packetp = (u8 *)(u32)desc_p->buffer;
			priv->curr_rxd = desc_p;
		}
	}
	return length;
}

static int npcm750_eth_free_pkt(struct udevice *dev, uchar *packet, int length)
{
	struct npcm750_eth_dev *priv = dev_get_priv(dev);
	struct emc_regs *reg = priv->emc_regs_p;
	struct npcm750_rxbd *desc_p = priv->curr_rxd;

	/*
	 * Make the current descriptor valid again and go to
	 * the next one
	 */
	desc_p->sl |= RX_OWEN_DMA;
	flush_dcache_range((ulong)desc_p, (ulong)(desc_p + 1));
	priv->curr_rxd = (struct npcm750_rxbd *)priv->curr_rxd->next;
	writel(0, &reg->rsdr);

	return 0;
}

static void npcm750_eth_stop(struct udevice *dev)
{
	struct npcm750_eth_dev *priv = dev_get_priv(dev);
	struct emc_regs *reg = priv->emc_regs_p;

	writel(readl(&reg->mcmdr) & ~MCMDR_TXON, &reg->mcmdr);
	writel(readl(&reg->mcmdr) & ~MCMDR_RXON, &reg->mcmdr);
	priv->curr_txd= (struct npcm750_txbd *)readl(&reg->txdlsa);
	priv->curr_rxd= (struct npcm750_rxbd *)readl(&reg->rxdlsa);
	phy_shutdown(priv->phydev);
}

static int npcm750_eth_write_hwaddr(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct npcm750_eth_dev *priv = dev_get_priv(dev);

	npcm750_set_cam(priv, CAM0, pdata->enetaddr);
	return 0;
}

static int npcm750_eth_bind(struct udevice *dev)
{
	return 0;
}

static int npcm750_eth_probe(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct npcm750_eth_dev *priv = dev_get_priv(dev);
	u32 iobase = pdata->iobase;
	int ret;

    struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();

	memset(priv, 0, sizeof(struct npcm750_eth_dev));
	priv->idx = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					"id", dev->seq_);
	priv->emc_regs_p = (struct emc_regs *)iobase;
	priv->interface = pdata->phy_interface;
	priv->max_speed = pdata->max_speed;

	if (priv->idx == 0) {
		/* Enable RMII for EMC1 module */
		writel((readl(&gcr->intcr)  | (1 << INTCR_R1EN)), &gcr->intcr);
	}

	npcm750_mdio_init(dev->name, priv);
	priv->bus = miiphy_get_dev_by_name(dev->name);

	ret = npcm750_phy_init(priv, dev);

	return ret;
}

static int npcm750_eth_remove(struct udevice *dev)
{
	struct npcm750_eth_dev *priv = dev_get_priv(dev);

	free(priv->phydev);
	mdio_unregister(priv->bus);
	mdio_free(priv->bus);

	return 0;
}

static const struct eth_ops npcm750_eth_ops = {
	.start			= npcm750_eth_start,
	.send			= npcm750_eth_send,
	.recv			= npcm750_eth_recv,
	.free_pkt		= npcm750_eth_free_pkt,
	.stop			= npcm750_eth_stop,
	.write_hwaddr	= npcm750_eth_write_hwaddr,
};

static int npcm750_eth_ofdata_to_platdata(struct udevice *dev)
{
	struct npcm750_eth_pdata *npcm750_pdata = dev_get_plat(dev);
	struct eth_pdata *pdata = &npcm750_pdata->eth_pdata;
	const char *phy_mode;
	const fdt32_t *cell;
	int ret = 0;

	pdata->iobase = (phys_addr_t)dev_read_addr_ptr(dev);

	pdata->phy_interface = -1;
	phy_mode = fdt_getprop(gd->fdt_blob, dev_of_offset(dev), "phy-mode", NULL);
	if (phy_mode)
		pdata->phy_interface = phy_get_interface_by_name(phy_mode);
	if (pdata->phy_interface == -1) {
		printf("%s: Invalid PHY interface '%s'\n", __func__, phy_mode);
		return -EINVAL;
	}

	pdata->max_speed = 0;
	cell = fdt_getprop(gd->fdt_blob, dev_of_offset(dev), "max-speed", NULL);
	if (cell)
		pdata->max_speed = fdt32_to_cpu(*cell);

	return ret;
}

static const struct udevice_id npcm750_eth_ids[] = {
	{ .compatible = "nuvoton,npcm750-emc" },
	{ }
};

U_BOOT_DRIVER(eth_npcm750) = {
	.name	= "eth_npcm750",
	.id	= UCLASS_ETH,
	.of_match = npcm750_eth_ids,
	.of_to_plat = npcm750_eth_ofdata_to_platdata,
	.bind	= npcm750_eth_bind,
	.probe	= npcm750_eth_probe,
	.remove	= npcm750_eth_remove,
	.ops	= &npcm750_eth_ops,
	.priv_auto = sizeof(struct npcm750_eth_dev),
	.plat_auto = sizeof(struct npcm750_eth_pdata),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
