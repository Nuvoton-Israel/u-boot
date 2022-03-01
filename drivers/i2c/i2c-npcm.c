// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <asm/io.h>
#include <clk.h>
#include <i2c.h>
#include <dm.h>
#include <asm/arch/cpu.h>
#include <asm/arch/smb.h>
#include <asm/arch/gcr.h>
#include <linux/iopoll.h>

#define I2C_FREQ_100K   100000
/* SCLFRQ min/max field values  */
#define SCLFRQ_MIN		10
#define SCLFRQ_MAX		511

#define NPCM_I2C_TIMEOUT_MS	10

enum {
	I2C_ERR_NACK = 1,
	I2C_ERR_BER,
	I2C_ERR_TIMEOUT,
};

struct npcm_i2c_bus {
	struct udevice *dev;
	struct npcm_i2c_regs *reg;
	int module_num;
	u32 apb_clk;
	u32 freq;
	int started;
};

static void npcm_dump_regs(struct npcm_i2c_bus *bus)
{
	struct npcm_i2c_regs *reg = bus->reg;

	printf("\n");
	printf("SMBST=0x%x\n", readb(&reg->st));
	printf("SMBCST=0x%x\n", readb(&reg->cst));
	printf("SMBCTL1=0x%x\n", readb(&reg->ctl1));
	printf("\n");
}

static int npcm_i2c_check_sda(struct npcm_i2c_bus *bus)
{
	struct npcm_i2c_regs *reg = bus->reg;
	ulong start_time;
	int err = I2C_ERR_TIMEOUT;
	u8 val;

	start_time = get_timer(0);
	/* wait SDAST to be 1 */
	while (get_timer(start_time) < NPCM_I2C_TIMEOUT_MS) {
		val = readb(&reg->st);
		if (val & SMBST_NEGACK) {
			err = I2C_ERR_NACK;
			break;
		}
		if (val & SMBST_BER) {
			err = I2C_ERR_BER;
			break;
		}
		if (val & SMBST_SDAST) {
			err = 0;
			break;
		}
	}

	if (err)
		printf("%s: err %d\n", __func__, err);

	return err;
}

static int npcm_i2c_send_start(struct npcm_i2c_bus *bus)
{
	struct npcm_i2c_regs *reg = bus->reg;
	ulong start_time;
	int err = I2C_ERR_TIMEOUT;

	/* Generate START condition */
	writeb(readb(&reg->ctl1) | SMBCTL1_START, &reg->ctl1);

	start_time = get_timer(0);
	while (get_timer(start_time) < NPCM_I2C_TIMEOUT_MS) {
		if (readb(&reg->st) & SMBST_BER)
			return I2C_ERR_BER;
		if (readb(&reg->st) & SMBST_MASTER) {
			err = 0;
			break;
		}
	}
	bus->started = 1;

	return err;
}

static int npcm_i2c_send_stop(struct npcm_i2c_bus *bus, bool wait)
{
	struct npcm_i2c_regs *reg = bus->reg;
	ulong start_time;
	int err = I2C_ERR_TIMEOUT;

	writeb(readb(&reg->ctl1) | SMBCTL1_STOP, &reg->ctl1);

	/* Clear NEGACK, STASTR and BER bits  */
	writeb(SMBST_STASTR | SMBST_NEGACK | SMBST_BER, &reg->st);

	bus->started = 0;

	if (!wait)
		return 0;

	start_time = get_timer(0);
	while (get_timer(start_time) < NPCM_I2C_TIMEOUT_MS) {
		if ((readb(&reg->ctl1) & SMBCTL1_STOP) == 0) {
			err = 0;
			break;
		}
	}
	if (err) {
		printf("%s: err %d\n", __func__, err);
		npcm_dump_regs(bus);
	}

	return err;
}

static void npcm_i2c_reset(struct npcm_i2c_bus *bus)
{
	struct npcm_i2c_regs *reg = bus->reg;

	printf("%s: module %d\n", __func__, bus->module_num);
	/* disable & enable SMB moudle */
	writeb(readb(&reg->ctl2) & ~SMBCTL2_ENABLE, &reg->ctl2);
	writeb(readb(&reg->ctl2) | SMBCTL2_ENABLE, &reg->ctl2);

	/* clear BB and status */
	writeb(SMBCST_BB, &reg->cst);
	writeb(0xff, &reg->st);

	/* select bank 1 */
	writeb(readb(&reg->ctl3) | SMBCTL3_BNK_SEL, &reg->ctl3);
	/* Clear all fifo bits */
	writeb(SMBFIF_CTS_CLR_FIFO, &reg->bank1.fif_cts);

	/* select bank 0 */
	writeb(readb(&reg->ctl3) & ~SMBCTL3_BNK_SEL, &reg->ctl3);
	/* clear EOB bit */
	writeb(SMBCST3_EO_BUSY, &reg->bank0.cst3);
	/* single byte mode */
	writeb(readb(&reg->bank0.fif_ctl) & ~SMBFIF_CTL_FIFO_EN, &reg->bank0.fif_ctl);

	/* set POLL mode */
	writeb(0, &reg->ctl1);
}

static void npcm_i2c_recovery(struct npcm_i2c_bus *bus, u32 addr)
{
	u8 val;
	int iter = 27;
	struct npcm_i2c_regs *reg = bus->reg;
	int err;

	val = readb(&reg->ctl3);
	/* Skip recovery, bus not stucked */
	if ((val & SMBCTL3_SCL_LVL) && (val & SMBCTL3_SDA_LVL))
		return;

	printf("Performing I2C bus %d recovery...\n", bus->module_num);
	/* SCL/SDA are not releaed, perform recovery */
	while (1) {
		/* toggle SCL line */
		writeb(SMBCST_TGSCL, &reg->cst);

		udelay(20);
		val = readb(&reg->ctl3);
		if (val & SMBCTL3_SDA_LVL)
			break;
		if (iter-- == 0)
			break;
	}

	if (val & SMBCTL3_SDA_LVL) {
		writeb((u8)((addr << 1) & 0xff), &reg->sda);
		err = npcm_i2c_send_start(bus);
		if (!err) {
			udelay(20);
			npcm_i2c_send_stop(bus, false);
			udelay(200);
			printf("I2C bus %d recovery completed\n", bus->module_num);
		} else {
			printf("%s: send START err %d\n", __func__, err);
		}
	} else {
		printf("Fail to recover I2C bus %d\n", bus->module_num);
	}
	npcm_i2c_reset(bus);
}

static int npcm_i2c_send_address(struct npcm_i2c_bus *bus, u8 addr,
				 bool stall)
{
	struct npcm_i2c_regs *reg = bus->reg;
	ulong start_time;
	u8 val;

	/* Stall After Start Enable */
	if (stall)
		writeb(readb(&reg->ctl1) | SMBCTL1_STASTRE, &reg->ctl1);

	writeb(addr, &reg->sda);
	if (stall) {
		start_time = get_timer(0);
		while (get_timer(start_time) < NPCM_I2C_TIMEOUT_MS) {
			if (readb(&reg->st) & SMBST_STASTR)
				break;

			if (readb(&reg->st) & SMBST_BER) {
				writeb(readb(&reg->ctl1) & ~SMBCTL1_STASTRE, &reg->ctl1);
				return I2C_ERR_BER;
			}
		}
	}

	/* check ACK */
	val = readb(&reg->st);
	if (val & SMBST_NEGACK) {
		debug("NACK on addr 0x%x\n", addr >> 1);
		/* After a Stop condition, writing 1 to NEGACK clears it */
		return I2C_ERR_NACK;
	}
	if (val & SMBST_BER)
		return I2C_ERR_BER;

	return 0;
}

static int npcm_i2c_read_bytes(struct npcm_i2c_bus *bus, u8 *data, int len)
{
	struct npcm_i2c_regs *reg = bus->reg;
	u8 val;
	int i;
	int err = 0;

	if (len == 1) {
		/* bus should be stalled before receiving last byte */
		writeb(readb(&reg->ctl1) | SMBCTL1_ACK, &reg->ctl1);

		/* clear STASTRE if it is set */
		if (readb(&reg->ctl1) & SMBCTL1_STASTRE) {
			writeb(SMBST_STASTR, &reg->st);
			writeb(readb(&reg->ctl1) & ~SMBCTL1_STASTRE, &reg->ctl1);
		}
		npcm_i2c_check_sda(bus);
		npcm_i2c_send_stop(bus, false);
		*data = readb(&reg->sda);
		/* this must be done to generate STOP condition */
		writeb(SMBST_NEGACK, &reg->st);
	} else {
		for (i = 0; i < len; i++) {
			/* When NEGACK bit is set to 1 after the transmission of a byte,
			 * SDAST is not set to 1.
			 */
			if (i != (len - 1)) {
				err = npcm_i2c_check_sda(bus);
			} else {
				err = readb_poll_timeout(&reg->ctl1, val,
							 !(val & SMBCTL1_ACK), 100000);
				if (err) {
					printf("wait nack timeout\n");
					err = I2C_ERR_TIMEOUT;
					npcm_dump_regs(bus);
				}
			}
			if (err && err != I2C_ERR_TIMEOUT)
				break;
			if (i == (len - 2)) {
				/* set NACK before last byte */
				writeb(readb(&reg->ctl1) | SMBCTL1_ACK, &reg->ctl1);
			}
			if (i == (len - 1)) {
				/* last byte */
				/* send STOP condition */
				npcm_i2c_send_stop(bus, false);
				*data = readb(&reg->sda);
				writeb(SMBST_NEGACK, &reg->st);
				break;
			}
			*data = readb(&reg->sda);
			data++;
		}
	}

	return err;
}

static int npcm_i2c_send_bytes(struct npcm_i2c_bus *bus, u8 *data, int len)
{
	struct npcm_i2c_regs *reg = bus->reg;
	u8 val;
	int i;
	int err = 0;

	val = readb(&reg->st);
	if (val & SMBST_NEGACK)
		return I2C_ERR_NACK;
	else if (val & SMBST_BER)
		return I2C_ERR_BER;

	/* clear STASTRE if it is set */
	if (readb(&reg->ctl1) & SMBCTL1_STASTRE)
		writeb(readb(&reg->ctl1) & ~SMBCTL1_STASTRE, &reg->ctl1);

	for (i = 0; i < len; i++) {
		err = npcm_i2c_check_sda(bus);
		if (err)
			break;
		writeb(*data, &reg->sda);
		data++;
	}
	npcm_i2c_check_sda(bus);

	return err;
}

static int npcm_i2c_read(struct npcm_i2c_bus *bus, u32 addr, u8 *data,
			 u32 len)
{
	struct npcm_i2c_regs *reg = bus->reg;
	int err;
	bool stall;

	if (len <= 0)
		return -EINVAL;

	/* send START condition */
	err = npcm_i2c_send_start(bus);
	if (err) {
		debug("%s: send START err %d\n", __func__, err);
		return err;
	}

	stall = (len == 1) ? true : false;
	/* send address byte */
	err = npcm_i2c_send_address(bus, (u8)(addr << 1) | 0x1, stall);

	if (!err && len)
		npcm_i2c_read_bytes(bus, data, len);

	if (err == I2C_ERR_NACK) {
		/* clear NACK */
		writeb(SMBST_NEGACK, &reg->st);
	}

	if (err)
		debug("%s: err %d\n", __func__, err);

	return err;
}

static int npcm_i2c_write(struct npcm_i2c_bus *bus, u32 addr, u8 *data,
			  u32 len)
{
	struct npcm_i2c_regs *reg = bus->reg;
	int err;
	bool stall;

	/* send START condition */
	err = npcm_i2c_send_start(bus);
	if (err) {
		debug("%s: send START err %d\n", __func__, err);
		return err;
	}

	stall = (len == 0) ? true : false;
	/* send address byte */
	err = npcm_i2c_send_address(bus, (u8)(addr << 1), stall);

	if (!err && len)
		err = npcm_i2c_send_bytes(bus, data, len);

	/* clear STASTRE if it is set */
	if (stall)
		writeb(readb(&reg->ctl1) & ~SMBCTL1_STASTRE, &reg->ctl1);

	if (err)
		debug("%s: err %d\n", __func__, err);

	return err;
}

static int npcm_i2c_xfer(struct udevice *dev,
			 struct i2c_msg *msg, int nmsgs)
{
	struct npcm_i2c_bus *bus = dev_get_priv(dev);
	struct npcm_i2c_regs *reg = bus->reg;
	int ret = 0, err = 0;

	if (nmsgs < 1 || nmsgs > 2) {
		printf("%s: commands not support\n", __func__);
		return -EREMOTEIO;
	}
	/* clear ST register */
	writeb(0xFF, &reg->st);

	for ( ; nmsgs > 0; nmsgs--, msg++) {
		if (msg->flags & I2C_M_RD)
			err = npcm_i2c_read(bus, msg->addr, msg->buf,
					    msg->len);
		else
			err = npcm_i2c_write(bus, msg->addr, msg->buf,
					     msg->len);
		if (err) {
			debug("i2c_xfer: error %d\n", err);
			ret = -EREMOTEIO;
			break;
		}
	}

	if (bus->started)
		npcm_i2c_send_stop(bus, true);

	if (err)
		npcm_i2c_recovery(bus, msg->addr);

	return ret;
}

static int npcm_i2c_init_clk(struct npcm_i2c_bus *bus, u32 bus_freq)
{
	struct npcm_i2c_regs *reg = bus->reg;
	u16  sclfrq	= 0;
	u8   hldt		= 7;
	u32  freq;
	u8 val;

	freq = bus->apb_clk;

	if (bus_freq <= I2C_FREQ_100K) {
		/* Set frequency: */
		/* SCLFRQ = T(SCL)/4/T(CLK) = FREQ(CLK)/4/FREQ(SCL)
		 *  = FREQ(CLK) / ( FREQ(SCL)*4 )
		 */
		sclfrq = (u16)((freq / ((u32)bus_freq * 4)));

		/* Check whether requested frequency can be achieved in current CLK */
		if (sclfrq < SCLFRQ_MIN || sclfrq > SCLFRQ_MAX)
			return -EINVAL;

		if (freq >= 40000000)
			hldt = 17;
		else if (freq >= 12500000)
			hldt = 15;
		else
			hldt = 7;
	} else {
		printf("Support standard mode only\n");
		return -EINVAL;
	}

	val = readb(&reg->ctl2) & 0x1;
	val |= (sclfrq & 0x7F) << 1;
	writeb(val, &reg->ctl2);

	/* clear 400K_MODE bit */
	val = readb(&reg->ctl3) & 0xc;
	val |= (sclfrq >> 7) & 0x3;
	writeb(val, &reg->ctl3);

	writeb(hldt, &reg->bank0.ctl4);

	return 0;
}

static int npcm_i2c_set_bus_speed(struct udevice *dev,
				  unsigned int speed)
{
	int ret;
	struct npcm_i2c_bus *bus = dev_get_priv(dev);

	ret = npcm_i2c_init_clk(bus, bus->freq);

	return ret;
}

static int npcm_i2c_probe(struct udevice *dev)
{
	struct npcm_i2c_bus *bus = dev_get_priv(dev);
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	struct npcm_i2c_regs *reg;
	struct clk clk;
	int ret;
	u32 val;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret) {
		printf("%s: ret %d\n", __func__, ret);
		return ret;
	}
	bus->apb_clk = clk_get_rate(&clk);
	if (!bus->apb_clk) {
		printf("%s: fail to get rate\n", __func__);
		return -EINVAL;
	}
	clk_free(&clk);

	bus->module_num = dev->seq_;
	bus->reg = (struct npcm_i2c_regs *)dev_read_addr_ptr(dev);
	bus->freq = dev_read_u32_default(dev, "clock-frequency", 100000);
	bus->started = 0;
	reg = bus->reg;

	if (npcm_i2c_init_clk(bus, bus->freq) != 0) {
		printf("%s: init_clk failed\n", __func__);
		return -EINVAL;
	}
	if (IS_ENABLED(CONFIG_ARCH_NPCM8XX))
		writel(I2CSEGCTL_INIT_VAL, &gcr->i2csegctl);
	if (IS_ENABLED(CONFIG_ARCH_NPCM7xx)) {
		if(bus->module_num == 0) {
			val = readl(&gcr->i2csegsel) & ~(3 << I2CSEGSEL_S0DECFG);
                	writel(val, &gcr->i2csegsel);
                	val = readl(&gcr->i2csegctl) | (1 << I2CSEGCTL_S0DWE) | (1 << I2CSEGCTL_S0DEN);
                	writel(val, &gcr->i2csegctl);
		}
		else if(bus->module_num == 4) {
			val = readl(&gcr->i2csegsel) & ~(3 << I2CSEGSEL_S4DECFG);
                	writel(val, &gcr->i2csegsel);
                	val = readl(&gcr->i2csegctl) | (1 << I2CSEGCTL_S4DWE) | (1 << I2CSEGCTL_S4DEN);
                	writel(val, &gcr->i2csegctl);
		}
	}
	/* enable SMB moudle */
	writeb(readb(&reg->ctl2) | SMBCTL2_ENABLE, &reg->ctl2);

	/* select bank 0 */
	writeb(readb(&reg->ctl3) & ~SMBCTL3_BNK_SEL, &reg->ctl3);

	/* single byte mode */
	writeb(readb(&reg->bank0.fif_ctl) & ~SMBFIF_CTL_FIFO_EN, &reg->bank0.fif_ctl);

	/* set POLL mode */
	writeb(0, &reg->ctl1);

	printf("I2C bus%d ready. speed=%d, base=0x%x, apb=%u\n",
	       bus->module_num, bus->freq, (u32)(uintptr_t)bus->reg, bus->apb_clk);

	return 0;
}

static const struct dm_i2c_ops nuvoton_i2c_ops = {
	.xfer		    = npcm_i2c_xfer,
	.set_bus_speed	= npcm_i2c_set_bus_speed,
};

static const struct udevice_id nuvoton_i2c_of_match[] = {
	{ .compatible = "nuvoton,npcm845-i2c" },
	{ .compatible = "nuvoton,npcm750-i2c" },
	{}
};

U_BOOT_DRIVER(npcm_i2c_bus) = {
	.name = "npcm-i2c",
	.id = UCLASS_I2C,
	.of_match = nuvoton_i2c_of_match,
	.probe = npcm_i2c_probe,
	.priv_auto = sizeof(struct npcm_i2c_bus),
	.ops = &nuvoton_i2c_ops,
};

