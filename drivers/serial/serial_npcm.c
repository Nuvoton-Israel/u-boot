/*
 * Copyright (c) 2016 Nuvoton Technology Corp.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <linux/compiler.h>
#include <asm/io.h>
#include <serial.h>
#include <clk.h>
#include <asm/arch/uart.h>

DECLARE_GLOBAL_DATA_PTR;

/* Information about a serial port */
struct npcm_serial_platdata {
	struct npcm_uart *reg;  /* address of registers in physical memory */
	u8 port_id;     /* uart port number */
	u32 uart_clk;
};

int npcm_serial_init(struct npcm_uart *uart)
{
	u8 val;

	/*
	* Disable all UART interrupt
	*/
	writeb(0, &uart->ier);

	/*
	* Set port for 8 bit, 1 stop, no parity
	*/
	val = LCR_WLS_8b;
	writeb(val, &uart->lcr);

	/*
	* Set the RX FIFO trigger level, reset RX, TX FIFO
	*/
	val = FCR_FME | FCR_RFR | FCR_TFR | FCR_RFITL_4B;
	writeb(val, &uart->fcr);

	return 0;
}

static int npcm_serial_pending(struct udevice *dev, bool input)
{
	struct npcm_serial_platdata *plat = dev->plat_;
	struct npcm_uart *const uart = plat->reg;

	if (input)
		return (readb(&uart->lsr) & LSR_RFDR);
	else
		return !(readb(&uart->lsr) & LSR_THRE);
}

static int npcm_serial_putc(struct udevice *dev, const char ch)
{
	struct npcm_serial_platdata *plat = dev->plat_;
	struct npcm_uart *const uart = plat->reg;

	while (!(readl(&uart->lsr) & LSR_THRE));

	writeb(ch, &uart->thr);

	return 0;
}

static int npcm_serial_getc(struct udevice *dev)
{
	struct npcm_serial_platdata *plat = dev->plat_;
	struct npcm_uart *const uart = plat->reg;

	while (!(readl(&uart->lsr) & LSR_RFDR));

	return (int)(readb(&uart->rbr) & 0xff);
}

static int npcm_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct npcm_serial_platdata *plat = dev->plat_;
	struct npcm_uart *const uart = plat->reg;
	int ret = 0;
	s32 divisor;
	u32 uart_clock;

#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
	uart_clock = plat->uart_clk;

	/* BaudOut = UART Clock  / (16 * [Divisor + 2]) */
	divisor = ((s32)uart_clock / ((s32)baudrate * 16)) - 2;

	divisor = 0;    /* Maximum Baudrate possible  500000000/2/10/32/1000 = 781  */
#else
	/* 24MHz = 960MHz(PLL2) / 2 / (19 + 1) */
	uart_clock = plat->uart_clk;

	/* BaudOut = UART Clock  / (16 * [Divisor + 2]) */
	divisor = ((s32)uart_clock / ((s32)baudrate * 16)) - 2;

	/* since divisor is rounded down check
	   if it is better when rounded up */
	if (((s32)uart_clock / (16 * (divisor + 2)) - baudrate) >
		(baudrate - (s32)uart_clock / (16 * ((divisor + 1) + 2)))) {
		divisor++;
	}

	if (divisor < 0)
		return -1;
#endif

	writeb(readb(&uart->lcr) | LCR_DLAB, &uart->lcr);
	writeb(divisor & 0xff, &uart->dll);
	writeb(divisor >> 8, &uart->dlm);
	writeb(readb(&uart->lcr) & (~LCR_DLAB), &uart->lcr);

	return ret;
}

static int npcm_serial_probe(struct udevice *dev)
{
	struct npcm_serial_platdata *plat = dev->plat_;
	struct npcm_uart *const uart = plat->reg;
	uint clkd[2]; /* clk_id and clk_no, UART clk_no is 1. */
	struct clk clk;
	int ret;

	npcm_serial_init(uart);

	plat->uart_clk = fdtdec_get_uint(gd->fdt_blob, dev_of_offset(dev),
					"clock-frequency", 115200);

	ret = fdtdec_get_int_array(gd->fdt_blob, dev_of_offset(dev),
					"clocks", clkd, 2);
	if (ret)
		return ret;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret < 0) {
		printf("Cannot find clk driver!\n");
		return ret;
	}

	clk.id = clkd[1];

	/* To set UART clock source and divider */
	ret = clk_set_rate(&clk, plat->uart_clk);
	clk_free(&clk);
	if (ret < 0)
		return ret;

	return 0;
}

static int npcm_serial_ofdata_to_platdata(struct udevice *dev)
{
	struct npcm_serial_platdata *plat = dev->plat_;
	void *addr;

	addr = dev_read_addr_ptr(dev);
	if (addr == NULL)
		return -EINVAL;

	plat->reg = (struct npcm_uart *)addr;
	plat->port_id = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					"id", dev->seq_);
	return 0;
}

static const struct dm_serial_ops npcm_serial_ops = {
	.getc = npcm_serial_getc,
	.setbrg = npcm_serial_setbrg,
	.putc = npcm_serial_putc,
	.pending = npcm_serial_pending,
};

static const struct udevice_id npcm_serial_ids[] = {
	{ .compatible = "nuvoton,npcm750-uart" },
	{ .compatible = "nuvoton,npcm845-uart" },
	{ }
};

U_BOOT_DRIVER(serial_npcm) = {
	.name	= "serial_npcm",
	.id	= UCLASS_SERIAL,
	.of_match = npcm_serial_ids,
	.of_to_plat = npcm_serial_ofdata_to_platdata,
	.plat_auto  = sizeof(struct npcm_serial_platdata),
	.probe = npcm_serial_probe,
	.ops	= &npcm_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
};