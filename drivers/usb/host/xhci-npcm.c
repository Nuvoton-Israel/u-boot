// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <dm.h>
#include <generic-phy.h>
#include <reset.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <linux/delay.h>

#include <usb.h>
#include <usb/xhci.h>
#include <dwc3-uboot.h>

struct npcm_xhci_priv {
	struct xhci_ctrl ctrl;
	struct xhci_hccr *hcd;
	struct phy phy;
};

static int npcm_xhci_setup_phy(struct udevice *dev, struct phy *phy)
{
	int ret;

	if (!phy)
		return 0;

	ret = generic_phy_get_by_index(dev, 0, phy);
	if (ret) {
		if (ret != -ENOENT) {
			dev_err(dev, "failed to get usb phy\n");
			return ret;
		}
	} else {
		ret = generic_phy_init(phy);
		if (ret) {
			dev_err(dev, "failed to init usb phy\n");
			return ret;
		}
	}

	return 0;
}

static struct dwc3_device dwc3_device_data = {
	.maximum_speed = USB_SPEED_SUPER,
	.base = 0x2e00000,
	.dr_mode = USB_DR_MODE_HOST,
	.index = 0,
	.hsphy_mode = USBPHY_INTERFACE_MODE_UTMIW,
};

#define INTEGRATION_REG 0x3300014
static int npcm_xhci_init(struct udevice *dev)
{
	struct npcm_xhci_priv *priv = dev_get_priv(dev);
	struct reset_ctl reset;
	int ret;
	uint32_t reg_val;

	dwc3_device_data.parent_dev = dev;

	// clear xhci0 disable port bits
	reg_val = readl(INTEGRATION_REG);
	reg_val &= ~(1 << 16);
	reg_val &= ~(1 << 17);
	writel(reg_val, INTEGRATION_REG);

	ret = reset_get_by_index(dev, 0, &reset);
	if (ret && ret != -ENOENT && ret != -ENOTSUPP) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}
#if 0
	/* reset controller */
	if (reset_valid(&reset))
		reset_assert(&reset);

	/* setup phy */
	ret = npcm_xhci_setup_phy(dev, &priv->phy);
	if (ret)
		return ret;

	/* release controller from reset */
	if (reset_valid(&reset))
		reset_deassert(&reset);
#endif

	return dwc3_uboot_init(&dwc3_device_data);

	return 0;
}

static int npcm_xhci_probe(struct udevice *dev)
{
	struct npcm_xhci_priv *priv = dev_get_priv(dev);
	struct xhci_hcor *hcor;
	enum usb_init_type type = dev_get_driver_data(dev);
	int ret;

	ret = npcm_xhci_init(dev);
	if (ret)
		return ret;

	priv->hcd = (struct xhci_hccr *)dev_read_addr_ptr(dev);
	debug("USB HCD @0x%p\n", priv->hcd);
	hcor = (struct xhci_hcor *)((uintptr_t)priv->hcd +
			HC_LENGTH(xhci_readl(&priv->hcd->cr_capbase)));

	return xhci_register(dev, priv->hcd, hcor);
}

static int npcm_xhci_remove(struct udevice *dev)
{
	struct npcm_xhci_priv *priv = dev_get_priv(dev);

	generic_phy_exit(&priv->phy);

	return xhci_deregister(dev);
}

#if 0
static int npcm_xhci_of_to_plat(struct udevice *dev)
{
	struct rcar_xhci_plat *plat = dev_get_plat(dev);

	plat->hcd_base = dev_read_addr(dev);
	if (plat->hcd_base == FDT_ADDR_T_NONE) {
		debug("Can't get the XHCI register base address\n");
		return -ENXIO;
	}

	return 0;
}
#endif


static const struct udevice_id npcm_xhci_ids[] = {
	{ .compatible = "nuvoton,npcm945-xhci", .data = USB_INIT_HOST },
	{ }
};

U_BOOT_DRIVER(xhci_npcm) = {
	.name	= "xhci_npcm",
	.id	= UCLASS_USB,
	.of_match = npcm_xhci_ids,
	.probe = npcm_xhci_probe,
	.remove = npcm_xhci_remove,
	.ops	= &xhci_usb_ops,
	//.of_to_plat = npcm_xhci_of_to_plat,
	.priv_auto = sizeof(struct npcm_xhci_priv),
	.plat_auto = sizeof(struct usb_plat),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};
