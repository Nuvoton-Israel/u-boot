/*
 * NUVOTON Poleg timer driver
 *
 * Copyright (C) 2017, NUVOTON, Incorporated
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <timer.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <clk.h>
#include <asm/arch/timer.h>

DECLARE_GLOBAL_DATA_PTR;

#define POLEG_CLOCK_RATE			1000000		// 1MHz
#define HW_TIMER_INIT_VAL			0xFFFFFF

struct poleg_timer_priv {
	struct poleg_gptimer_regs *regs;
};

static struct poleg_timer_priv *timer_priv;

/* get HW timer data */
static u32 get_timer_data(void)
{
	u32 count;
	struct poleg_timer_priv *priv = timer_priv;

	if (!priv)
		return 0;

	//writel(readl(&priv->regs->tcsr0) & ~TCSR_EN, &priv->regs->tcsr0);
	count = readl(&priv->regs->tdr0) & 0x00ffffff;
	//writel(readl(&priv->regs->tcsr0) | TCSR_EN, &priv->regs->tcsr0);

	return count;
}

/* delay x useconds */
void __udelay(unsigned long usec)
{
	unsigned long last, now, diff = 0;

	if (!gd->timer) {
		int i, ret;
		ret = dm_timer_init();
		if (ret) {
			for (i = 0 ; i < usec; i++);
			return;
		}
	}
	last = get_timer_data();

	while (diff < usec) {
		now = get_timer_data();
		if (now <= last)
			diff += (last - now);
		else
			diff += (HW_TIMER_INIT_VAL + 1 - now) + last;
		last = now;
	}
}

/* get timer count */
static int poleg_timer_get_count(struct udevice *dev, u64 *count)
{
	unsigned long now, diff;
	int ret;

	if (!gd->timer) {
		ret = dm_timer_init();
		if (ret)
			return ret;
	}
	now = get_timer_data();

	if (now <= gd->arch.lastinc)
		diff = gd->arch.lastinc - now;
	else
		diff = (HW_TIMER_INIT_VAL + 1 - now) + gd->arch.lastinc;

	gd->arch.timer_reset_value += diff;
	gd->arch.lastinc = now;

	*count = gd->arch.timer_reset_value;
	return 0;
}

static int timer_clock_init(struct udevice *dev)
{
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct clk clk;
	uint clkd[2]; /* clk_id and clk_no, timer clk_no is 1. */
	int ret;

	ret = fdtdec_get_int_array(gd->fdt_blob, dev_of_offset(dev),
			"clocks", clkd, 2);
	if (ret)
		return ret;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret < 0) {
		printf("Cannot find clk driver\n");
		return ret;
	}

	clk.id = clkd[1];

	/* To set timer clock source and divider */
	ret = clk_set_rate(&clk, uc_priv->clock_rate);
	clk_free(&clk);
	if (ret < 0)
		return ret;

	return 0;
}

static int poleg_timer_probe(struct udevice *dev)
{
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct poleg_timer_priv *priv = dev_get_priv(dev);
	int ret;

	timer_priv = priv;
	uc_priv->clock_rate = POLEG_CLOCK_RATE;

	ret = timer_clock_init(dev);
	if (ret)
		return ret;

	/* clear tcsr */
	writel(0, &priv->regs->tcsr0);
	/* set timer initial value */
	writel(HW_TIMER_INIT_VAL, &priv->regs->ticr0);

	/* configure timer and start */
	/* periodic mode
	 * input clock freq = 25Mhz
	 * prescale = 25
	 * clock rate = 25Mhz/25 = 1Mhz
	 */
	writel(TCSR_EN | TCSR_MODE_PERIODIC | TCSR_PRESCALE_25, &priv->regs->tcsr0);

	gd->arch.timer_reset_value = 0;
	gd->arch.lastinc = get_timer_data();
	gd->arch.tbl = 0;

	return 0;
}

static int poleg_timer_ofdata_to_platdata(struct udevice *dev)
{
	struct poleg_timer_priv *priv = dev_get_priv(dev);

	priv->regs = map_physmem((phys_addr_t)dev_read_addr_ptr(dev),
				 sizeof(struct poleg_gptimer_regs), MAP_NOCACHE);

	return 0;
}


static const struct timer_ops poleg_timer_ops = {
	.get_count = poleg_timer_get_count,
};

static const struct udevice_id poleg_timer_ids[] = {
	{ .compatible = "nuvoton,poleg-timer" },
	{}
};

U_BOOT_DRIVER(poleg_timer) = {
	.name	= "poleg_timer",
	.id	= UCLASS_TIMER,
	.of_match = poleg_timer_ids,
	.ofdata_to_platdata = poleg_timer_ofdata_to_platdata,
	.priv_auto_alloc_size = sizeof(struct poleg_timer_priv),
	.probe = poleg_timer_probe,
	.ops	= &poleg_timer_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
