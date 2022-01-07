// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <dm.h>
#include <reset-uclass.h>
#include <asm/io.h>
#include <dm/device_compat.h>

/* Register Offsets */
#define NPCM_IPSRST1		0x20
#define NPCM_IPSRST2		0x24
#define NPCM_IPSRST3		0x34
#define NPCM_IPSRST4		0x74

#define NPCM_RC_RESETS_PER_REG	32
#define NPCM_MASK_RESETS	GENMASK(4, 0)

struct npcm_reset_priv {
	void __iomem *base;
	struct reset_ops ops;
};

static int npcm_rc_setclear_reset(struct reset_ctl *reset_ctl, bool set)
{
	struct npcm_reset_priv *priv = dev_get_priv(reset_ctl->dev);
	u32 rst_bit = BIT(reset_ctl->id & NPCM_MASK_RESETS);
	u32 ctrl_offset = reset_ctl->id >> 8;
	u32 stat;

	stat = readl(priv->base + ctrl_offset);
	if (set)
		writel(stat | rst_bit, priv->base + ctrl_offset);
	else
		writel(stat & ~rst_bit, priv->base + ctrl_offset);

	return 0;
}

static int npcm_reset_request(struct reset_ctl *reset_ctl)
{
	return 0;
}

static int npcm_reset_free(struct reset_ctl *reset_ctl)
{
	return 0;
}

static int npcm_reset_assert(struct reset_ctl *reset_ctl)
{
	debug("%s: reset_ctl->id: %lx\n", __func__, reset_ctl->id);
	return npcm_rc_setclear_reset(reset_ctl, true);
}

static int npcm_reset_deassert(struct reset_ctl *reset_ctl)
{
	debug("%s: reset_ctl->id: %lx\n", __func__, reset_ctl->id);
	return npcm_rc_setclear_reset(reset_ctl, false);
}

static int npcm_reset_xlate(struct reset_ctl *reset_ctl,
			    struct ofnode_phandle_args *args)
{
	unsigned int offset, bit;

	if (args->args_count != 2) {
		dev_err(reset_ctl->dev, "Invaild args_count: %d\n", args->args_count);
		return -EINVAL;
	}

	offset = args->args[0];
	if (offset != NPCM_IPSRST1 && offset != NPCM_IPSRST2 &&
	    offset != NPCM_IPSRST3 && offset != NPCM_IPSRST4) {
		dev_err(reset_ctl->dev, "Error reset register (0x%x)\n", offset);
		return -EINVAL;
	}
	bit = args->args[1];
	if (bit >= NPCM_RC_RESETS_PER_REG) {
		dev_err(reset_ctl->dev, "Error reset number (%d)\n", bit);
		return -EINVAL;
	}
	reset_ctl->id = (offset << 8) | bit;

	return 0;
}

static int npcm_reset_probe(struct udevice *dev)
{
	struct npcm_reset_priv *priv = dev_get_priv(dev);

	priv->base = dev_remap_addr(dev);
	if (!priv->base)
		return -ENOMEM;

	return 0;
}

static const struct udevice_id npcm_reset_ids[] = {
	{ .compatible = "nuvoton,npcm845-reset" },
	{ .compatible = "nuvoton,npcm750-reset" },
	{ }
};

struct reset_ops npcm_reset_ops = {
	.request = npcm_reset_request,
	.rfree = npcm_reset_free,
	.rst_assert = npcm_reset_assert,
	.rst_deassert = npcm_reset_deassert,
	.of_xlate = npcm_reset_xlate,
};

U_BOOT_DRIVER(npcm_reset) = {
	.name = "npcm_reset",
	.id = UCLASS_RESET,
	.of_match = npcm_reset_ids,
	.probe = npcm_reset_probe,
	.ops = &npcm_reset_ops,
	.priv_auto = sizeof(struct npcm_reset_priv),
};
