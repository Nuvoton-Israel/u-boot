// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2006
 * DENX Software Engineering <mk@denx.de>
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/mach-types.h>
#include <asm/arch/clock.h>
#include <asm/arch/info.h>
#include <dm.h>
#include <usb.h>
#include "ohci.h"
#include <generic-phy.h>


#if defined(CONFIG_USB_OHCI_NEW) && defined(CONFIG_SYS_USB_OHCI_CPU_INIT)


struct ohci_npcm8xx_priv {
	ohci_t ohci;
	struct phy phy;
};


int usb_cpu_init(void)
{
	struct npcm850_gcr *gcr = (struct npcm850_gcr *)npcm850_get_base_gcr();
	struct clk_ctl *clkctl = (struct clk_ctl *)npcm850_get_base_clk();


	if ((readl(&clkctl->ipsrst3) & (1 << IPSRST3_USBPHY2)) != 0) {

		/* Set usb host controller to reset */
		writel(readl(&clkctl->ipsrst2) | (1 << IPSRST2_USBHOST1), &clkctl->ipsrst2);
		writel(readl(&clkctl->ipsrst4) | (1 << IPSRST4_USBHOST2), &clkctl->ipsrst4);

		/* Set usb device controller to reset */
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV6), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV5), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV4), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV3), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV2), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_USBDEV1), &clkctl->ipsrst1);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBDEV9), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBDEV8), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBDEV7), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBDEV0), &clkctl->ipsrst3);

		/* Set usb hub to reset */
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBHUB), &clkctl->ipsrst3);

		/* Set usb phy1/2/3 to reset */
		writel(readl(&clkctl->ipsrst4) | (1 << IPSRST4_USBPHY3), &clkctl->ipsrst4);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBPHY2), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) | (1 << IPSRST3_USBPHY1), &clkctl->ipsrst3);

		writel(readl(&gcr->usb1phyctl) & ~(1 << USB1PHYCTL_RS), &gcr->usb1phyctl);
		writel(readl(&gcr->usb2phyctl) & ~(1 << USB2PHYCTL_RS), &gcr->usb2phyctl);
		writel(readl(&gcr->usb3phyctl) & ~(1 << USB3PHYCTL_RS), &gcr->usb3phyctl);

		udelay(1000);

		/* Enable phy1/2/3*/
		writel(readl(&clkctl->ipsrst4) & ~(1 << IPSRST4_USBPHY3), &clkctl->ipsrst4);
		writel(readl(&clkctl->ipsrst3) & ~(1 << IPSRST3_USBPHY2), &clkctl->ipsrst3);
		writel(readl(&clkctl->ipsrst3) & ~(1 << IPSRST3_USBPHY1), &clkctl->ipsrst3);


		/* Wait for PHY clocks to stablize for 50us or more */
		udelay(1000);

		/* Set RS bit after PHY reset bit is released */
		writel(readl(&gcr->usb1phyctl) | (1 << USB1PHYCTL_RS), &gcr->usb1phyctl);
		writel(readl(&gcr->usb2phyctl) | (1 << USB2PHYCTL_RS), &gcr->usb2phyctl);
		writel(readl(&gcr->usb3phyctl) | (1 << USB3PHYCTL_RS), &gcr->usb3phyctl);

		/* Enable hub */
		writel(readl(&clkctl->ipsrst3) & ~(1 << IPSRST3_USBHUB), &clkctl->ipsrst3);
	}

	/* enable host1/2 */
	writel(readl(&clkctl->ipsrst2) & ~(1 << IPSRST2_USBHOST1), &clkctl->ipsrst2);
	writel(readl(&clkctl->ipsrst4) & ~(1 << IPSRST4_USBHOST2), &clkctl->ipsrst4);

	return 0;
}

int usb_cpu_stop(void)
{
	struct clk_ctl *clkctl = (struct clk_ctl *)npcm850_get_base_clk();

	writel(readl(&clkctl->ipsrst3) & ~(1 << IPSRST3_USBPHY2), &clkctl->ipsrst3);
	writel(readl(&clkctl->ipsrst3) & ~(1 << IPSRST3_USBPHY1), &clkctl->ipsrst3);

	return 0;
}



int usb_cpu_init_fail(void)
{
	return usb_cpu_stop();
}

#if CONFIG_IS_ENABLED(DM_USB)
static int ohci_npcm8xx_probe(struct udevice *dev)
{
	struct ohci_regs *regs = (struct ohci_regs *)devfdt_get_addr(dev);
	int err = 0;

	err = usb_cpu_init();

	if (err)
		return err;

	err = ohci_register(dev, regs);

	if (err)
		return err;

	return 0;
}

static int ohci_npcm8xx_remove(struct udevice *dev)
{
	int ret;

	ret = ohci_deregister(dev);
	if (ret)
		return ret;

	ret = usb_cpu_stop();
	if (ret)
		return ret;

	return 0;
}

static const struct udevice_id ohci_npcm8xx_usb_ids[] = {
	{
		.compatible = "nuvoton,npcm8xx-ohci"
	},
	{ }
};

U_BOOT_DRIVER(usb_ohci) = {
	.name	= "ohci_npcm8xx",
	.id	= UCLASS_USB,
	.of_match = ohci_npcm8xx_usb_ids,
	.probe = ohci_npcm8xx_probe,
	.remove = ohci_npcm8xx_remove,
	.ops	= &ohci_usb_ops,
	.platdata_auto_alloc_size = sizeof(struct usb_platdata),
	.priv_auto_alloc_size = sizeof(struct ohci_npcm8xx_priv),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};
#endif
#endif /* defined(CONFIG_USB_OHCI) && defined(CONFIG_SYS_USB_OHCI_CPU_INIT) */
