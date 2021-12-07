/*
 *  Copyright (c) 2017 Nuvoton Technology Corp.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <common.h>
#include <asm/types.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <asm/arch/gcr.h>
#include <asm/io.h>
#include <dm.h>
#include <clk-uclass.h>
#include <dt-bindings/clock/npcm750_poleg-clock.h>

#define POLEG_VERSION_A1 0x04A92750
struct poleg_clk_priv {
	struct clk_ctl *regs;
};

enum pll_clks {
	PLL_0,
	PLL_1,
	PLL_2,
	PLL_CLKREF,
};

static u32 clk_get_pll_freq(struct clk_ctl *pll_clk, enum pll_clks pll)
{
	struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();
	u32 pllval;
	u32 fin = EXT_CLOCK_FREQUENCY_KHZ; /* 25KHz */
	u32 fout, nr, nf, no;
	fout = nr = nf = no = 0;

	switch (pll) {
	case PLL_0:
		pllval = readl(&pll_clk->pllcon0);
		break;
	case PLL_1:
		pllval = readl(&pll_clk->pllcon1);
		break;
	case PLL_2:
		pllval = readl(&pll_clk->pllcon2);
		break;
	case PLL_CLKREF:
	default:
		return 0;
	}

	/* PLL Input Clock Divider */
	nr = (pllval >> PLLCONn_INDV) & 0x1f;
	/* PLL VCO Output Clock Feedback Divider */
	nf = (pllval >> PLLCONn_FBDV) & 0xfff;
	/* PLL Output Clock Divider 1 */
	no = ((pllval >> PLLCONn_OTDV1) & 0x7) *
		((pllval >> PLLCONn_OTDV2) & 0x7);

	fout = ((10 * fin * nf) / (no * nr));
	if ((pll == PLL_1) && (readl(&gcr->pdid) == POLEG_VERSION_A1))
		fout /= 2;

	return fout * 100;
}

static int poleg_timer_init_clk(struct clk_ctl *timer_clk)
{
	volatile unsigned int i;
	/* enable timer 0 */
	writel(readl(&timer_clk->clken1) | (1 << CLKEN1_TIMER0_4),
		&timer_clk->clken1);

	/* Wait for 200 clock cycles between clkDiv or clkSel change */
	for (i = 200; i > 0; i--);

	/* Setting the Timer divisor to 1 */
	writel(readl(&timer_clk->clkdiv1) & ~(0x1f << CLKDIV1_TIMCKDIV),
		&timer_clk->clkdiv1);

	/* Wait for 200 clock cycles between clkDiv or clkSel change */
	for (i = 200; i > 0; i--);

	/* Sellecting 25MHz clock source */
	writel((readl(&timer_clk->clksel) & ~(3 << CLKSEL_TIMCKSEL))
		| (CLKSEL_TIMCKSEL_CLKREF << CLKSEL_TIMCKSEL),
		&timer_clk->clksel);

	/* Wait 10usec for PLL */
	for (i = 200; i > 0; i--);

	return 0;
}

static int poleg_uart_init_clk(struct clk_ctl *uart_clk)
{
	writel((readl(&uart_clk->clkdiv1) & ~(0x1f << CLKDIV1_UARTDIV))
		| (19 << CLKDIV1_UARTDIV), &uart_clk->clkdiv1);
	writel((readl(&uart_clk->clksel) & ~(3 << CLKSEL_UARTCKSEL))
		| (CLKSEL_UARTCKSEL_PLL2 << CLKSEL_UARTCKSEL),
		&uart_clk->clksel);

	return 0;
}

enum sd_num {
	SD1_DEV,
	SD2_DEV,
};

static u32 poleg_sd_configure_clk(struct clk_ctl *sd_clk,
					ulong rate, enum sd_num sdnum)
{
	u32 pll0_freq, divider, sdhci_clk;
	volatile unsigned int i;

	if (sdnum == SD1_DEV) {
		writel(readl(&sd_clk->clken2) | (1 << CLKEN2_SDHC),
			&sd_clk->clken2);
	} else if (sdnum == SD2_DEV) {
		writel(readl(&sd_clk->clken2) | (1 << CLKEN2_MMC),
			&sd_clk->clken2);
	}

	/* To acquire PLL0 frequency. */
	pll0_freq = clk_get_pll_freq(sd_clk, PLL_0);

	/* Calculate rounded up divider to produce closest to
	   target output clock  */
	divider = (pll0_freq % rate == 0) ? (pll0_freq / rate) :
						(pll0_freq / rate) + 1;

	if (sdnum == SD1_DEV) {
		writel((readl(&sd_clk->clkdiv2) & ~(0xf << CLKDIV2_SD1CKDIV))
			| ((divider / 2) - 1) << CLKDIV2_SD1CKDIV,
			&sd_clk->clkdiv2);
	} else if (sdnum == SD2_DEV) {
		writel((readl(&sd_clk->clkdiv1) & ~(0x1f << CLKDIV1_MMCCKDIV))
			| (divider - 1) << CLKDIV1_MMCCKDIV, &sd_clk->clkdiv1);
	}
	/* Wait to the divider to stabilize (according to spec) */
	for (i = 200; i > 0; i--);

	/* SD clock uses always PLL0 */
	writel((readl(&sd_clk->clksel) & ~(0x3 << CLKSEL_SDCKSEL))
		| (CLKSEL_SDCKSEL_PLL0 << CLKSEL_SDCKSEL),
		&sd_clk->clksel);

	sdhci_clk = pll0_freq / divider;

	return sdhci_clk;
}

static ulong poleg_get_cpu_freq(struct clk_ctl *cpu_clk)
{
	ulong fout = 0;
	u32 clksel = readl(&cpu_clk->clksel) & (0x3 << CLKSEL_CPUCKSEL);

	if (clksel == CLKSEL_CPUCKSEL_PLL0)
		fout = (ulong)clk_get_pll_freq(cpu_clk, PLL_0);
	else if (clksel == CLKSEL_CPUCKSEL_PLL1)
		fout = (ulong)clk_get_pll_freq(cpu_clk, PLL_1);
	else if (clksel == CLKSEL_CPUCKSEL_CLKREF)
		fout = EXT_CLOCK_FREQUENCY_MHZ; /* FOUT is specified in MHz */
	else
		fout = EXT_CLOCK_FREQUENCY_MHZ; /* FOUT is specified in MHz */

	return fout;
}

static u32 poleg_get_pll0_apb_divisor(struct clk_ctl *regs, u32 apb)
{
	volatile u32 apb_divisor = 1;

	/* AXI divider */
	apb_divisor = apb_divisor * (1 << ((readl(&regs->clkdiv1)
						>> CLKDIV1_CLK2DIV) & 0x1));
	/* AHBn divider */
	apb_divisor = apb_divisor * (1 << ((readl(&regs->clkdiv1)
						>> CLKDIV1_CLK4DIV) & 0x3));

	switch (apb) {
	case APB1: /* APB divisor */
		apb_divisor = apb_divisor *
				(1 << ((readl(&regs->clkdiv2)
					>> CLKDIV2_APB1CKDIV) & 0x3));
		break;
	case APB2: /* APB divisor */
		apb_divisor = apb_divisor *
				(1 << ((readl(&regs->clkdiv2)
					>> CLKDIV2_APB2CKDIV) & 0x3));
		break;
	case APB3: /* APB divisor */
		apb_divisor = apb_divisor *
				(1 << ((readl(&regs->clkdiv2)
					>> CLKDIV2_APB3CKDIV) & 0x3));
		break;
	case APB4: /* APB divisor */
		apb_divisor = apb_divisor *
				(1 << ((readl(&regs->clkdiv2)
					>> CLKDIV2_APB4CKDIV) & 0x3));
		break;
	case APB5: /* APB divisor */
		apb_divisor = apb_divisor *
				(1 << ((readl(&regs->clkdiv2)
					>> CLKDIV2_APB5CKDIV) & 0x3));
		break;
	case SPI0: /* SPI0 divisor */
		apb_divisor = apb_divisor *
				(((readl(&regs->clkdiv3)
					>> CLKDIV3_SPI0CKDV) & 0x1f) + 1);
		break;
	case SPI3: /* SPI0 divider */
		apb_divisor = apb_divisor *
				(((readl(&regs->clkdiv1)
					>> CLKDIV1_AHB3CKDIV) & 0x1f) + 1);
		break;
	default:
		apb_divisor = 0xFFFFFFFF;
		break;

	}

	return apb_divisor;
}

static ulong poleg_get_rate(struct clk *clk)
{
	struct poleg_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case CLK_APB2:
		return poleg_get_cpu_freq(priv->regs) /
			poleg_get_pll0_apb_divisor(priv->regs, APB2);
		break;
	case CLK_APB5:
		return poleg_get_cpu_freq(priv->regs) /
			poleg_get_pll0_apb_divisor(priv->regs, APB5);
		break;
	}

	return 0;
}

static ulong poleg_set_rate(struct clk *clk, ulong rate)
{
	struct poleg_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case CLK_TIMER:
		return poleg_timer_init_clk(priv->regs);
		break;
	case CLK_UART:
		return poleg_uart_init_clk(priv->regs);
		break;
	case CLK_SD:
		return poleg_sd_configure_clk(priv->regs, rate, SD1_DEV);
		break;
	case CLK_EMMC:
		return poleg_sd_configure_clk(priv->regs, rate, SD2_DEV);
		break;
	default:
		return -1;
	}

	return 0;
}

static int poleg_clk_probe(struct udevice *dev)
{
	struct poleg_clk_priv *priv = dev_get_priv(dev);
	void *base;

	base = dev_read_addr_ptr(dev);

	if (base == NULL)
		return -EINVAL;

	priv->regs = (struct clk_ctl *)base;
	return 0;
}

static struct clk_ops poleg_clk_ops = {
	.get_rate = poleg_get_rate,
	.set_rate = poleg_set_rate,
};

static const struct udevice_id poleg_clk_ids[] = {
	{ .compatible = "nuvoton,npcm750-clock" },
	{ }
};

U_BOOT_DRIVER(clk_poleg) = {
	.name           = "clk_poleg",
	.id             = UCLASS_CLK,
	.of_match       = poleg_clk_ids,
	.ops            = &poleg_clk_ops,
	.plat_auto = sizeof(struct poleg_clk_priv),
	.probe          = poleg_clk_probe,
};
