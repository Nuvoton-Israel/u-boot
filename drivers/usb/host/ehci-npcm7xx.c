/*
 * NUVOTON POLEG USB HOST EHCI Controller
 *
 * Copyright (C) 2019 Nuvoton Co.Ltd
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <usb.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/arch/clock.h>
#include "ehci.h"


/* Declare global data pointer */
DECLARE_GLOBAL_DATA_PTR;

struct poleg_ehci_platdata {
	struct usb_platdata usb_plat;
	fdt_addr_t hcd_base;
};

/**
 * Contains pointers to register base addresses
 * for the usb controller.
 */
struct poleg_ehci {
	struct ehci_ctrl ctrl;
	struct ehci_hccr *hcd;
};

static int ehci_usb_ofdata_to_platdata(struct udevice *dev)
{
	struct poleg_ehci_platdata *plat = dev_get_platdata(dev);

	/*
	 * Get the base address for EHCI controller from the device node
	 */
	plat->hcd_base = (fdt_addr_t) dev_read_addr_ptr(dev);
	if (plat->hcd_base == FDT_ADDR_T_NONE) {
		debug("Can't get the XHCI register base address\n");
		return -ENXIO;
	}

	return 0;
}

/* Reset the EHCI host controller. */

static int ehci_usb_probe(struct udevice *dev)
{
	struct poleg_ehci_platdata *plat = dev_get_platdata(dev);
	struct poleg_ehci *ctx = dev_get_priv(dev);
	struct ehci_hcor *hcor;
	struct clk_ctl *clkctl = (struct clk_ctl *)npcm750_get_base_clk();
	struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();

	ctx->hcd = (struct ehci_hccr *)plat->hcd_base;

	if ((readl(&clkctl->ipsrst3) & (1 << IPSRST3_USBPHY2)) != 0) {
		/* set usb device controller to reset */
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV3), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV6), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV5), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV4), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV2), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV1), &clkctl->ipsrst1);

		/* set usb host controller to reset */
		writel(readl(&clkctl->ipsrst2) | (1 << IPSRST2_USBHOST), &clkctl->ipsrst2);

		/* set usb phy1 & phy2 to reset */
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBPHY2), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBPHY1), &clkctl->ipsrst3);

		writel(readl(&gcr->usb1phyctl) & ~(1 << USB1PHYCTL_RS), &gcr->usb1phyctl);
		writel(readl(&gcr->usb2phyctl) & ~(1 << USB2PHYCTL_RS), &gcr->usb2phyctl);

		udelay(1000);

		/* enable phy */
		writel(readl(&clkctl->ipsrst3) & ~(1 << IPSRST3_USBPHY2), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) & ~(1 << IPSRST3_USBPHY1), &clkctl->ipsrst3);

		/* wait for PHY clocks to stablize for 50us or more */
		udelay(1000);

		/* set RS bit after PHY reset bit is released */
		writel(readl(&gcr->usb1phyctl) | (1 << USB1PHYCTL_RS), &gcr->usb1phyctl);
		writel(readl(&gcr->usb2phyctl) | (1 << USB2PHYCTL_RS), &gcr->usb2phyctl);

		/* enable hub */
		writel(readl(&clkctl->ipsrst3) & ~(1 << IPSRST3_USBHUB), &clkctl->ipsrst3);
	}

	/* enable host */
	writel(readl(&clkctl->ipsrst2) & ~(1 << IPSRST2_USBHOST), &clkctl->ipsrst2);

	hcor = (struct ehci_hcor *)((uint32_t)ctx->hcd +
			HC_LENGTH(ehci_readl(&ctx->hcd->cr_capbase)));

	return ehci_register(dev, ctx->hcd, hcor, NULL, 0, USB_INIT_HOST);
}

static int ehci_usb_remove(struct udevice *dev)
{
	int ret;

	ret = ehci_deregister(dev);
	if (ret)
		return ret;

	return 0;
}

static const struct udevice_id ehci_usb_ids[] = {
	{ .compatible = "nuvoton,poleg-ehci" },
	{ }
};

U_BOOT_DRIVER(usb_poleg) = {
	.name	= "ehci_poleg",
	.id	= UCLASS_USB,
	.of_match = ehci_usb_ids,
	.ofdata_to_platdata = ehci_usb_ofdata_to_platdata,
	.probe = ehci_usb_probe,
	.remove = ehci_usb_remove,
	.ops	= &ehci_usb_ops,
	.priv_auto_alloc_size = sizeof(struct poleg_ehci),
	.platdata_auto_alloc_size = sizeof(struct poleg_ehci_platdata),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};
