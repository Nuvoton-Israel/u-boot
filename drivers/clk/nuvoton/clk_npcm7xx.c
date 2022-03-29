// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <clk-uclass.h>
#include <div64.h>
#include <dm.h>
#include <asm/io.h>
#include <linux/bitfield.h>
#include <linux/log2.h>
#include <dt-bindings/clock/nuvoton,npcm7xx-clock.h>

/* Register offsets */
#define CLKSEL		0x04	/* clock source selection */
#define CLKDIV1		0x08	/* clock divider 1 */
#define CLKDIV2		0x2C	/* clock divider 2 */
#define CLKDIV3		0x58	/* clock divider 3 */
#define PLLCON0		0x0C	/* pll0 control */
#define PLLCON1		0x10	/* pll1 control */
#define PLLCON2		0x54	/* pll2 control */

/* PLLCON bit filed */
#define PLLCON_INDV	GENMASK(5, 0)
#define PLLCON_OTDV1	GENMASK(10, 8)
#define PLLCON_OTDV2	GENMASK(15, 13)
#define PLLCON_FBDV	GENMASK(27, 16)

/* CLKSEL bit filed */
#define CPUCKSEL	GENMASK(1, 0)
#define SDCKSEL		GENMASK(7, 6)
#define UARTCKSEL	GENMASK(9, 8)
#define TIMCKSEL	GENMASK(15, 14)

/* CLKDIV1 bit filed */
#define SPI3CKDIV	GENMASK(10, 6)
#define MMCCKDIV	GENMASK(15, 11)
#define UARTDIV1	GENMASK(20, 16)
#define TIMCKDIV	GENMASK(25, 21)
#define CLK4DIV		GENMASK(27, 26)

/* CLKDIV2 bit filed */
#define APB5CKDIV	GENMASK(23, 22)
#define APB2CKDIV	GENMASK(27, 26)

/* CLKDIV3 bit filed */
#define SPIXCKDIV	GENMASK(5, 1)
#define SPI0CKDIV	GENMASK(10, 6)
#define UARTDIV2	GENMASK(15, 11)
#define SPI1CKDIV	GENMASK(23, 16)

/* Flags */
#define DIV_TYPE1	BIT(0)	/* div = clkdiv + 1 */
#define DIV_TYPE2	BIT(1)	/* div = 1 << clkdiv */
#define PRE_DIV2	BIT(2)	/* Pre divisor = 2 */
#define POST_DIV2	BIT(3)	/* Post divisor = 2 */
#define FIXED_PARENT	BIT(4)	/* clock source is fixed */

struct npcm_clk_priv {
	struct udevice *dev;
	void __iomem *base;
};

/* Parameters of PLL configuration */
struct npcm_clk_pll {
	const int id;
	const int parent_id;
	u32 reg;
	u32 flags;
};

/* Parent clock id to clksel mapping */
struct parent_data {
	int id;
	int clksel;
};

/* Parameters of parent selection */
struct npcm_clk_select {
	const int id;
	const struct parent_data *parents;
	u32 reg;
	u32 mask;
	u8 num_parents;
	u32 flags;
};

/* Parameters of clock divider */
struct npcm_clk_div {
	const int id;
	u32 reg;
	u32 mask;
	u32 flags;
};

/* Parent clock map */
static const struct parent_data pll_parents[] = {
	{NPCM7XX_CLK_PLL0, 0},
	{NPCM7XX_CLK_PLL1, 1},
	{NPCM7XX_CLK_REFCLK, 2},
	{NPCM7XX_CLK_PLL2DIV2, 3}
};

static const struct parent_data cpuck_parents[] = {
	{NPCM7XX_CLK_PLL0, 0},
	{NPCM7XX_CLK_PLL1, 1},
	{NPCM7XX_CLK_REFCLK, 2},
	{NPCM7XX_CLK_PLL2, 7}
};

static const struct parent_data apb_parent[] = {{NPCM7XX_CLK_AHB, 0}};

static struct npcm_clk_pll npcm7xx_clk_plls[] = {
	{NPCM7XX_CLK_PLL0, NPCM7XX_CLK_REFCLK, PLLCON0, 0},
	{NPCM7XX_CLK_PLL1, NPCM7XX_CLK_REFCLK, PLLCON1, 0},
	{NPCM7XX_CLK_PLL2, NPCM7XX_CLK_REFCLK, PLLCON2, 0},
	{NPCM7XX_CLK_PLL2DIV2, NPCM7XX_CLK_REFCLK, PLLCON2, POST_DIV2}
};

static struct npcm_clk_select npcm7xx_clk_selectors[] = {
	{NPCM7XX_CLK_AHB, cpuck_parents, CLKSEL, CPUCKSEL, 4, 0},
	{NPCM7XX_CLK_APB2, apb_parent, 0, 0, 1, FIXED_PARENT},
	{NPCM7XX_CLK_APB5, apb_parent, 0, 0, 1, FIXED_PARENT},
	{NPCM7XX_CLK_SPI0, apb_parent, 0, 0, 1, FIXED_PARENT},
	{NPCM7XX_CLK_SPI3, apb_parent, 0, 0, 1, FIXED_PARENT},
	{NPCM7XX_CLK_SPIX, apb_parent, 0, 0, 1, FIXED_PARENT},
	{NPCM7XX_CLK_UART, pll_parents, CLKSEL, UARTCKSEL, 4, 0},
	{NPCM7XX_CLK_TIMER, pll_parents, CLKSEL, TIMCKSEL, 4, 0},
	{NPCM7XX_CLK_SDHC, pll_parents, CLKSEL, SDCKSEL, 4, 0}
};

static struct npcm_clk_div npcm7xx_clk_dividers[] = {
	{NPCM7XX_CLK_AHB, CLKDIV1, CLK4DIV, DIV_TYPE1 | PRE_DIV2},
	{NPCM7XX_CLK_APB2, CLKDIV2, APB2CKDIV, DIV_TYPE2},
	{NPCM7XX_CLK_APB5, CLKDIV2, APB5CKDIV, DIV_TYPE2},
	{NPCM7XX_CLK_SPI0, CLKDIV3, SPI0CKDIV, DIV_TYPE1},
	{NPCM7XX_CLK_SPI3, CLKDIV1, SPI3CKDIV, DIV_TYPE1},
	{NPCM7XX_CLK_SPIX, CLKDIV3, SPIXCKDIV, DIV_TYPE1},
	{NPCM7XX_CLK_UART, CLKDIV1, UARTDIV1, DIV_TYPE1},
	{NPCM7XX_CLK_TIMER, CLKDIV1, TIMCKDIV, DIV_TYPE2},
	{NPCM7XX_CLK_SDHC, CLKDIV1, MMCCKDIV, DIV_TYPE1}
};

static int clkid_to_clksel(struct npcm_clk_select *selector, int id)
{
	int i;

	for (i = 0; i < selector->num_parents; i++) {
		if (selector->parents[i].id == id)
			return selector->parents[i].clksel;
	}

	return -EINVAL;
}

static int clksel_to_clkid(struct npcm_clk_select *selector, int clksel)
{
	int i;

	for (i = 0; i < selector->num_parents; i++) {
		if (selector->parents[i].clksel == clksel)
			return selector->parents[i].id;
	}

	return -EINVAL;
}

static struct npcm_clk_pll *npcm_clk_pll_get(int clk_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(npcm7xx_clk_plls); i++) {
		if (npcm7xx_clk_plls[i].id == clk_id)
			return &npcm7xx_clk_plls[i];
	}

	return NULL;
}

static struct npcm_clk_select *npcm_clk_selector_get(int clk_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(npcm7xx_clk_selectors); i++) {
		if (npcm7xx_clk_selectors[i].id == clk_id)
			return &npcm7xx_clk_selectors[i];
	}

	return NULL;
}

static struct npcm_clk_div *npcm_clk_divider_get(int clk_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(npcm7xx_clk_dividers); i++) {
		if (npcm7xx_clk_dividers[i].id == clk_id)
			return &npcm7xx_clk_dividers[i];
	}

	return NULL;
}

static ulong npcm_clk_get_fin(struct npcm_clk_priv *priv, int id)
{
	struct npcm_clk_select *selector;
	struct clk parent;
	ulong parent_rate;
	u32 val, clksel;
	int ret;

	selector = npcm_clk_selector_get(id);
	if (!selector)
		return 0;

	if (selector->flags & FIXED_PARENT) {
		clksel = 0;
	} else {
		val = readl(priv->base + selector->reg);
		clksel = (val & selector->mask) >> (ffs(selector->mask) - 1);
	}
	parent.id = clksel_to_clkid(selector, clksel);

	ret = clk_request(priv->dev, &parent);
	if (ret)
		return 0;

	parent_rate = clk_get_rate(&parent);

	debug("fin of clk%d = %lu\n", id, parent_rate);
	return parent_rate;
}

static u32 npcm_clk_get_div(struct npcm_clk_priv *priv, int id)
{
	struct npcm_clk_div *divider;
	u32 val, div;

	divider = npcm_clk_divider_get(id);
	if (!divider)
		return 0;

	val = readl(priv->base + divider->reg);
	div = (val & divider->mask) >> (ffs(divider->mask) - 1);
	if (divider->flags & DIV_TYPE1)
		div = div + 1;
	else
		div = 1 << div;

	if (divider->flags & PRE_DIV2)
		div = div << 1;

	return div;
}

static u32 npcm_clk_set_div(struct npcm_clk_priv *priv, int id, u32 div)
{
	struct npcm_clk_div *divider;
	u32 val, clkdiv;

	divider = npcm_clk_divider_get(id);
	if (!divider)
		return -EINVAL;

	if (divider->flags & PRE_DIV2)
		div = div >> 1;

	if (divider->flags & DIV_TYPE1)
		clkdiv = div - 1;
	else
		clkdiv = ilog2(div);

	val = readl(priv->base + divider->reg);
	val &= ~divider->mask;
	val |= (clkdiv << (ffs(divider->mask) - 1)) & divider->mask;
	writel(val, priv->base + divider->reg);

	return 0;
}

static ulong npcm_clk_get_fout(struct npcm_clk_priv *priv, int id)
{
	ulong parent_rate;
	u32 div;

	parent_rate = npcm_clk_get_fin(priv, id);
	if (!parent_rate)
		return -EINVAL;

	div = npcm_clk_get_div(priv, id);
	if (!div)
		return -EINVAL;

	debug("fout of clk%d = (%lu / %u)\n", id, parent_rate, div);
	return (parent_rate / div);
}

static ulong npcm_clk_set_fout(struct npcm_clk_priv *priv, int id, ulong rate)
{
	u32 div;
	ulong parent_rate;
	int ret;

	parent_rate = npcm_clk_get_fin(priv, id);
	if (!parent_rate)
		return -EINVAL;

	div = DIV_ROUND_UP(parent_rate, rate);
	ret = npcm_clk_set_div(priv, id, div);
	if (ret)
		return ret;

	debug("%s: rate %lu, new rate (%lu / %u)\n", __func__, rate, parent_rate, div);
	return (parent_rate / div);
}

static ulong npcm_clk_get_pll_fout(struct npcm_clk_priv *priv, int id)
{
	struct npcm_clk_pll *pll;
	struct clk parent;
	ulong parent_rate;
	ulong fbdv, indv, otdv1, otdv2;
	u32 val;
	u64 ret;

	pll = npcm_clk_pll_get(id);
	if (!pll)
		return -ENODEV;

	parent.id = pll->parent_id;
	ret = clk_request(priv->dev, &parent);
	if (ret)
		return ret;

	parent_rate = clk_get_rate(&parent);

	val = readl(priv->base + pll->reg);
	indv = FIELD_GET(PLLCON_INDV, val);
	fbdv = FIELD_GET(PLLCON_FBDV, val);
	otdv1 = FIELD_GET(PLLCON_OTDV1, val);
	otdv2 = FIELD_GET(PLLCON_OTDV2, val);

	ret = (u64)parent_rate * fbdv;
	do_div(ret, indv * otdv1 * otdv2);
	if (pll->flags & POST_DIV2)
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
		return npcm_clk_get_pll_fout(priv, clk->id);
	case NPCM7XX_CLK_AHB:
	case NPCM7XX_CLK_APB2:
	case NPCM7XX_CLK_APB5:
	case NPCM7XX_CLK_SDHC:
	case NPCM7XX_CLK_UART:
	case NPCM7XX_CLK_SPI0:
	case NPCM7XX_CLK_SPI3:
	case NPCM7XX_CLK_SPIX:
		return npcm_clk_get_fout(priv, clk->id);
	default:
		return -ENOSYS;
	}
}

static ulong npcm_clk_set_rate(struct clk *clk, ulong rate)
{
	struct npcm_clk_priv *priv = dev_get_priv(clk->dev);

	debug("%s: id %lu, rate %lu\n", __func__, clk->id, rate);
	switch (clk->id) {
	case NPCM7XX_CLK_TIMER:
	case NPCM7XX_CLK_SDHC:
	case NPCM7XX_CLK_UART:
	case NPCM7XX_CLK_SPI0:
	case NPCM7XX_CLK_SPI3:
	case NPCM7XX_CLK_SPIX:
		return  npcm_clk_set_fout(priv, clk->id, rate);
	default:
		return -ENOSYS;
	}
}

static int npcm_clk_set_parent(struct clk *clk, struct clk *parent)
{
	struct npcm_clk_priv *priv = dev_get_priv(clk->dev);
	struct npcm_clk_select *selector;
	int clksel;
	u32 val;

	debug("%s: id %lu, parent %lu\n", __func__, clk->id, parent->id);
	selector = npcm_clk_selector_get(clk->id);
	if (!selector)
		return -EINVAL;

	clksel = clkid_to_clksel(selector, parent->id);
	if (clksel < 0)
		return -EINVAL;

	val = readl(priv->base + selector->reg);
	val &= ~selector->mask;
	val |= clksel << (ffs(selector->mask) - 1);
	writel(val, priv->base + selector->reg);

	return 0;
}

static int npcm_clk_request(struct clk *clk)
{
	if (clk->id >= NPCM7XX_NUM_CLOCKS)
		return -EINVAL;

	return 0;
}

static struct clk_ops npcm_clk_ops = {
	.get_rate = npcm_clk_get_rate,
	.set_rate = npcm_clk_set_rate,
	.set_parent = npcm_clk_set_parent,
	.request = npcm_clk_request,
};

static int npcm_clk_probe(struct udevice *dev)
{
	struct npcm_clk_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base)
		return -EINVAL;
	priv->dev = dev;

	return 0;
}

static const struct udevice_id npcm_clk_ids[] = {
	{ .compatible = "nuvoton,npcm750-clk" },
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
