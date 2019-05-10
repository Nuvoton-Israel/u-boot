#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/arch/clock.h>

#include "../host/ehci.h"

/* Declare global data pointer */
DECLARE_GLOBAL_DATA_PTR;
/**
 * Contains pointers to register base addresses
 * for the usb controller.
 */
struct poleg_usbdev {
	struct ehci_ctrl ctrl;
	fdt_addr_t reg_base;
};

static int usbd_ofdata_to_platdata(struct udevice *dev)
{
	struct poleg_usbdev *priv = dev_get_priv(dev);

	priv = dev_get_priv(dev);
	/*
	 * Get the base address for XHCI controller from the device node
	 */
	priv->reg_base = dev_read_addr(dev);
	if (priv->reg_base == FDT_ADDR_T_NONE) {
		debug("Can't get the USBDEV register base address\n");
		return -ENXIO;
	}

	return 0;
}

static int poleg_usbd_probe(struct udevice *dev)
{
	struct poleg_usbdev *ctx = dev_get_priv(dev);
	struct clk_ctl *clkctl = (struct clk_ctl *)npcm750_get_base_clk();
	struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();
	struct ehci_hccr *hccr;
	struct ehci_hcor *hcor;

	hccr = (struct ehci_hccr *)ctx->reg_base;

	if ((readl(&clkctl->ipsrst3) & (1 << IPSRST3_USBPHY2)) != 0) {
		/* set usb device controller to reset */
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV6), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV5), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV4), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV3), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV2), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV1), &clkctl->ipsrst1);

		/* set usb host to reset */
		writel(readl(&clkctl->ipsrst2) | (1 << IPSRST2_USBHOST), &clkctl->ipsrst2);
		/* set usb hub to reset */
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBHUB), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBDEV9), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBDEV8), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBDEV7), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBDEV0), &clkctl->ipsrst3);

		/* set usb phy1 & phy2 to reset */
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBPHY2), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBPHY1), &clkctl->ipsrst3);

		writel(readl(&gcr->usb1phyctl) & ~(1 << USB1PHYCTL_RS), &gcr->usb1phyctl);
		writel(readl(&gcr->usb2phyctl) & ~(1 << USB2PHYCTL_RS), &gcr->usb2phyctl);

		udelay(1);

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

    /* enable usb device 0 */
	writel(readl(&clkctl->ipsrst3) & ~((1 << IPSRST3_USBDEV0)), &clkctl->ipsrst3);

	hcor = (struct ehci_hcor *)((uint32_t)hccr +
			HC_LENGTH(ehci_readl(&hccr->cr_capbase)));

	return ehci_register(dev, hccr, hcor, NULL, 0, USB_INIT_DEVICE);
}

static int poleg_usbd_remove(struct udevice *dev)
{
	int ret;

	ret = ehci_deregister(dev);
	if (ret)
		return ret;

	return 0;
}

static const struct udevice_id poleg_usb_ids[] = {
	{ .compatible = "nuvoton,poleg-usbd" },
	{ }
};

U_BOOT_DRIVER(poleg_usbd) = {
	.name	= "usbdev",
	.id	= UCLASS_USB,
	.of_match = poleg_usb_ids,
	.ofdata_to_platdata = usbd_ofdata_to_platdata,
	.probe	= poleg_usbd_probe,
	.remove = poleg_usbd_remove,
	.priv_auto_alloc_size = sizeof(struct poleg_usbdev),
};
