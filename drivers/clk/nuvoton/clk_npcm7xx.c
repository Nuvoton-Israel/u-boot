// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <clk-uclass.h>
#include <div64.h>
#include <dm.h>
#include <asm/io.h>
#include <asm/types.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/log2.h>
#include <linux/kernel.h>
#include <dt-bindings/clock/nuvoton,npcm7xx-clock.h>

/* Register offsets */
#define CLKSEL		0x04
#define CLKDIV1		0x08
#define CLKDIV2		0x2C
#define CLKDIV3		0x58
#define PLLCON0		0x0C
#define PLLCON1		0x10
#define PLLCON2		0x54

/* PLLCON mask */
#define PLLCON_INDV	GENMASK(5, 0)
#define PLLCON_FBDV	GENMASK(27, 16)
#define PLLCON_OTDV1	GENMASK(10, 8)
#define PLLCON_OTDV2	GENMASK(15, 13)

/* CLKSEL mask */
#define CPUCKSEL	GENMASK(1, 0)
#define SDCKSEL		GENMASK(7, 6)
#define UARTCKSEL	GENMASK(9, 8)
#define TIMCKSEL	GENMASK(15, 14)

/* CLKSEL val */
#define CPUCKSEL_PLL0	0
#define CPUCKSEL_PLL1	1
#define CPUCKSEL_REFCLK	2
#define CKSEL_PLL0	0
#define CKSEL_PLL1	1
#define CKSEL_REFCLK	2
#define CKSEL_PLL2	3

/* CLKDIV1 mask */
#define CLK4DIV		GENMASK(27, 26)
#define TIMCKDIV	GENMASK(25, 21)
#define UARTDIV1	GENMASK(20, 16)
#define MMCCKDIV	GENMASK(15, 11)

/* CLKDIV2 mask */
#define APB2CKDIV	GENMASK(27, 26)
#define APB5CKDIV	GENMASK(23, 22)
#define UARTDIV2	GENMASK(15, 11)

/* Flags */
#define FIXED_SRC	BIT(0)	/* Use fixed clk source */
#define DIV_TYPE1	BIT(1)	/* div = clkdiv + 1 */
#define DIV_TYPE2	BIT(2)	/* div = 1 << clkdiv */
#define PRE_DIV2	BIT(3)	/* Pre divider 2 */
#define POST_DIV2	BIT(4)	/* Post divider 2 */

#define NONE		(-1)

struct npcm_clk_priv {
	struct udevice *dev;
	void __iomem *regs;
};

struct npcm_clk {
	int id;
	int parent_id;
	u8 reg_clkdiv;
	u32 div_mask;
	u32 sel_mask;
	u32 sel_val;
	u32 flags;
};

struct npcm_clk_map {
	u32 clkid;
	u8 clksel;
};

#define NPCM7XX_CPU_CLK_MAP_SIZE	2
#define NPCM7XX_CLK_MAP_SIZE	3
/* clksel to clkid mapping */
static const struct npcm_clk_map NPCM7XX_cpu_clksel_map[NPCM7XX_CLK_MAP_SIZE] = {
	{NPCM7XX_CLK_PLL0,	CPUCKSEL_PLL0, },
	{NPCM7XX_CLK_PLL1,	CPUCKSEL_PLL1, },
};

static const struct npcm_clk_map NPCM7XX_clksel_map[NPCM7XX_CLK_MAP_SIZE] = {
	{NPCM7XX_CLK_PLL0,	CKSEL_PLL0, },
	{NPCM7XX_CLK_PLL1,	CKSEL_PLL1, },
	{NPCM7XX_CLK_PLL2DIV2,	CKSEL_PLL2, },
};

/* NPCM7XX clock table, Fout = ((Fin / PRE_DIV) / div) / POST_DIV */
static struct npcm_clk NPCM7XX_clks[] = {
	/* id, parent id,				DIV reg, mask		SEL mask, val,	flag */
	{NPCM7XX_CLK_PLL0, NPCM7XX_CLK_REFCLK,		PLLCON0, NONE,		NONE, NONE, FIXED_SRC },
	{NPCM7XX_CLK_PLL1, NPCM7XX_CLK_REFCLK,		PLLCON1, NONE,		NONE, NONE, FIXED_SRC },
	{NPCM7XX_CLK_PLL2, NPCM7XX_CLK_REFCLK,		PLLCON2, NONE,		NONE, NONE, FIXED_SRC },
	{NPCM7XX_CLK_PLL2DIV2, NPCM7XX_CLK_REFCLK,	PLLCON2, NONE,		NONE, NONE, FIXED_SRC | POST_DIV2 },
	{NPCM7XX_CLK_AHB, NONE,			CLKDIV1, CLK4DIV,	CPUCKSEL, NONE, DIV_TYPE1 | PRE_DIV2 },
	{NPCM7XX_CLK_APB2, NPCM7XX_CLK_AHB,		CLKDIV2, APB2CKDIV,	NONE, NONE, FIXED_SRC | DIV_TYPE2 },
	{NPCM7XX_CLK_APB5, NPCM7XX_CLK_AHB,		CLKDIV2, APB5CKDIV,	NONE, NONE, FIXED_SRC | DIV_TYPE2 },
	{NPCM7XX_CLK_UART, NPCM7XX_CLK_PLL2DIV2,	CLKDIV1, UARTDIV1,	UARTCKSEL, CKSEL_PLL2, DIV_TYPE1 },
	{NPCM7XX_CLK_SDHC, NPCM7XX_CLK_PLL0,		CLKDIV1, MMCCKDIV,	SDCKSEL, CKSEL_PLL0, DIV_TYPE1 },
	{NPCM7XX_CLK_MMC, NPCM7XX_CLK_PLL0,		CLKDIV1, MMCCKDIV,	SDCKSEL, CKSEL_PLL0, DIV_TYPE1 },
	{NPCM7XX_CLK_TIMER, NPCM7XX_CLK_PLL0,		CLKDIV1, TIMCKDIV,	TIMCKSEL, CKSEL_PLL0, DIV_TYPE1 },
};

static int clksel_to_clkid(u8 clksel, u32 mask)
{
	int i, size;
	const struct npcm_clk_map *map;

	if (mask == CPUCKSEL) {
		map = &NPCM7XX_cpu_clksel_map[0];
		size = NPCM7XX_CPU_CLK_MAP_SIZE;
	} else {
		map = &NPCM7XX_clksel_map[0];
		size = NPCM7XX_CLK_MAP_SIZE;
	}

	for (i = 0; i < size; i++) {
		if (map[i].clksel == clksel)
			return map[i].clkid;
	}
	return -EINVAL;
}

static struct npcm_clk *npcm_clk_get(u32 clk_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(NPCM7XX_clks); i++) {
		if (NPCM7XX_clks[i].id == clk_id)
			return &NPCM7XX_clks[i];
	}

	return NULL;
}

static ulong npcm_clk_get_fin(struct npcm_clk_priv *priv, struct npcm_clk *clk)
{
	struct clk parent;
	ulong parent_rate;
	u32 val, clksel;
	int ret;

	if (clk->flags & FIXED_SRC) {
		parent.id = clk->parent_id;
	} else {
		val = readl(priv->regs + CLKSEL);
		clksel = (val & clk->sel_mask) >> (ffs(clk->sel_mask) - 1);
		parent.id = clksel_to_clkid(clksel, clk->sel_mask);
	}
	ret = clk_request(priv->dev, &parent);
	if (ret)
		return 0;

	parent_rate = clk_get_rate(&parent);

	debug("fin of clk%d = %lu\n", clk->id, parent_rate);
	return parent_rate;
}

static ulong npcm_clk_get_fout(struct npcm_clk_priv *priv, u32 id)
{
	struct npcm_clk *clk;
	ulong parent_rate;
	u32 clkdiv, val, div;

	clk = npcm_clk_get(id);
	if (!clk)
		return 0;
	parent_rate = npcm_clk_get_fin(priv, clk);

	val = readl(priv->regs + clk->reg_clkdiv);
	clkdiv = (val & clk->div_mask) >> (ffs(clk->div_mask) - 1);
	if (clk->flags & DIV_TYPE1)
		div = clkdiv + 1;
	else
		div = 1 << clkdiv;

	if (clk->flags & PRE_DIV2)
		div *= 2;

	debug("fout of clk%d = (%lu / %u)\n", id, parent_rate, div);
	return (parent_rate / div);
}

static ulong npcm_clk_set_fout(struct npcm_clk_priv *priv, u32 id, ulong rate)
{
	struct npcm_clk *clk;
	u32 val, clkdiv, div;
	ulong parent_rate;

	clk = npcm_clk_get(id);
	if (!clk)
		return 0;
	
	/* Select source */
	val = readl(priv->regs + CLKSEL);
	val &= ~clk->sel_mask;
	val |= (clk->sel_val << (ffs(clk->sel_mask) - 1)) & clk->sel_mask;
	writel(val, priv->regs + CLKSEL);

	/* Calcuate div */
	parent_rate = npcm_clk_get_fin(priv, clk);
	div = DIV_ROUND_UP(parent_rate, rate);
	if (clk->flags & DIV_TYPE1)
		clkdiv = div - 1;
	else
		clkdiv = ilog2(div);

	val = readl(priv->regs + clk->reg_clkdiv);
	val &= ~clk->div_mask;
	val |= (clkdiv << (ffs(clk->div_mask) - 1)) & clk->div_mask;
	writel(val, priv->regs + clk->reg_clkdiv);

	debug("%s: rate %lu, new rate (%lu / %u)\n", __func__, rate, parent_rate, div);
	return (parent_rate / div);
}

static ulong npcm_clk_get_pll_rate(struct npcm_clk_priv *priv, u32 id)
{
	struct npcm_clk *clk;
	ulong parent_rate;
	ulong fbdv, indv, otdv1, otdv2;
	u32 val;
	u64 ret;

	clk = npcm_clk_get(id);
	if (!clk)
		return 0;

	parent_rate = npcm_clk_get_fin(priv, clk);

	val = readl(priv->regs + clk->reg_clkdiv);
	indv = FIELD_GET(PLLCON_INDV, val);
	fbdv = FIELD_GET(PLLCON_FBDV, val);
	otdv1 = FIELD_GET(PLLCON_OTDV1, val);
	otdv2 = FIELD_GET(PLLCON_OTDV2, val);

	ret = (u64)parent_rate * fbdv;
	do_div(ret, indv * otdv1 * otdv2);
	if (clk->flags & POST_DIV2)
		do_div(ret, 2);

	debug("fout of pll(id %d) = %llu\n", id, ret);
	return ret;
}

static ulong npcm_clk_get_rate(struct clk *clk)
{
	struct npcm_clk_priv *priv = dev_get_priv(clk->dev);
	struct clk refclk;
	int ret;

	debug("%s: id %lu\n", __func__, clk->id);
	switch (clk->id) {
	case NPCM7XX_CLK_TIMER:
	case NPCM7XX_CLK_REFCLK:
		ret = clk_get_by_name(priv->dev, "refclk", &refclk);
		if (!ret)
			return clk_get_rate(&refclk);
		else
			return ret;
	case NPCM7XX_CLK_PLL0:
	case NPCM7XX_CLK_PLL1:
	case NPCM7XX_CLK_PLL2:
	case NPCM7XX_CLK_PLL2DIV2:
		return npcm_clk_get_pll_rate(priv, clk->id);
	case NPCM7XX_CLK_AHB:
	case NPCM7XX_CLK_APB2:
	case NPCM7XX_CLK_APB5:
	case NPCM7XX_CLK_UART:
		return npcm_clk_get_fout(priv, clk->id);
	default:
		return -ENOSYS;
	}
}

static ulong npcm_clk_set_rate(struct clk *clk, ulong rate)
{
	struct npcm_clk_priv *priv = dev_get_priv(clk->dev);
	ulong fout;

	debug("%s: id %lu, rate %lu\n", __func__, clk->id, rate);
	switch (clk->id) {
	case NPCM7XX_CLK_TIMER:
	case NPCM7XX_CLK_UART:
	case NPCM7XX_CLK_MMC:
	case NPCM7XX_CLK_SDHC:
	case NPCM7XX_CLK_REFCLK:
		fout =  npcm_clk_set_fout(priv, clk->id, rate);
		return fout;
	default:
		return -ENOSYS;
	}
}

static int npcm_clk_request(struct clk *clk)
{
	debug("%s: id %lu\n", __func__, clk->id);
	if (clk->id >= NPCM7XX_NUM_CLOCKS)
		return -EINVAL;

	return 0;
}

static struct clk_ops npcm_clk_ops = {
	.get_rate = npcm_clk_get_rate,
	.set_rate = npcm_clk_set_rate,
	.request = npcm_clk_request,
};

static int npcm_clk_probe(struct udevice *dev)
{
	struct npcm_clk_priv *priv = dev_get_priv(dev);

	priv->regs = dev_read_addr_ptr(dev);
	if (!priv->regs)
		return -ENOENT;
	priv->dev = dev;

	return 0;
}

static const struct udevice_id npcm_clk_ids[] = {
	{ .compatible = "nuvoton,npcm845-clk" },
	{ .compatible = "nuvoton,npcm750-clock" },
	{ }
};

U_BOOT_DRIVER(clk_npcm) = {
	.name           = "clk_npcm",
	.id             = UCLASS_CLK,
	.of_match       = npcm_clk_ids,
	.ops            = &npcm_clk_ops,
	.priv_auto	= sizeof(struct npcm_clk_priv),
	.probe          = npcm_clk_probe,
	.flags = DM_FLAG_PRE_RELOC,
};
