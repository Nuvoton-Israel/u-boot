#include <stdlib.h>
#include <command.h>
#include <common.h>
#include <time.h>
#include <dm.h>
#include <rand.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <u-boot/crc.h>
#include <console.h>
#include <display_options.h>

/* SIO base address */
#define SIO_IDX		0x4e
#define SIO_DATA	0x4f

/*
 * GPIO Assign Note:
 * eSPI_CLK and eSPI_IO signals must assign on the same GPIO port.
 */
#define PORT_BASE	0xF0015000	// GPIO5: 160~191
#define PORT_OUT	(PORT_BASE + 0xC)
#define PORT_IN		(PORT_BASE + 0x4)
#define PORT_POL	(PORT_BASE + 0x8)
#define PORT_OES	(PORT_BASE + 0x70)
#define PORT_OEC	(PORT_BASE + 0x74)
#define PORT_IEM	(PORT_BASE + 0x58)
#define PORT_EVTYP	(PORT_BASE + 0x28)
#define PORT_EVBE	(PORT_BASE + 0x2C)
#define PORT_EVEN	(PORT_BASE + 0x40)
#define PORT_EVENS	(PORT_BASE + 0x44)
#define PORT_EVENC	(PORT_BASE + 0x48)
#define PORT_EVST	(PORT_BASE + 0x4C)
#define eSPI_nCS	BIT(1) //161
#define eSPI_CLK	BIT(3) //163
#define eSPI_IO0	BIT(4) //164
#define eSPI_IO1	BIT(5) //165
#define eSPI_IO2	BIT(6) //166
#define eSPI_IO3	BIT(7) //167
#define eSPI_ALERT	BIT(8) //168
/* eSPI_nRST is on GPIO2 (GPIO95 = bit 31 of the GPIO2 port) */
#define RST_BASE	0xF0012000	// GPIO2: 64~95
#define RST_OUT		(RST_BASE + 0xC)
#define RST_OES		(RST_BASE + 0x70)
#define eSPI_nRST	BIT(31) //95
/* Mask of all eSPI-related bits on GPIO5 port, used for clrsetbits_le32
 * to avoid overwriting other pins' bits in RW registers (IEM, EVTYP, EVBE) */
#define ALL_ESPI_PORT_PINS	(eSPI_nCS | eSPI_CLK | eSPI_IO0 | eSPI_IO1 | \
				 eSPI_IO2 | eSPI_IO3 | eSPI_ALERT)

/* eSPI I/O mode */
#define ESPI_IO_MODE_SINGLE	0
#define ESPI_IO_MODE_DUAL	1
#define ESPI_IO_MODE_QUAD	2

// eSPI command
#define CMD_SET_CONFIGURATION	0x22
#define CMD_GET_CONFIGURATION	0x21
#define CMD_PUT_VWIRE		0x04
#define CMD_GET_VWIRE		0x05
#define CMD_GET_GET_STATUS	0x25
#define CMD_RESET		0xFF

// OOB Channel Commands
#define CMD_PUT_OOB		0x06
#define CMD_GET_OOB		0x07

// Flash Channel Commands
#define CMD_PUT_FLASH_C		0x08
#define CMD_GET_FLASH_NP	0x09
#define CMD_PUT_FLASH_NP	0x0A
#define CMD_GET_FLASH_C		0x0B

// Cycle Type
#define CYCLE_FLASH_READ	0x00
#define CYCLE_FLASH_WRITE	0x01
#define CYCLE_FLASH_ERASE	0x02
#define CYCLE_FLASH_RPMC_OP1	0x03
#define CYCLE_FLASH_RPMC_OP2	0x04

// Peripheral Channel Commands
#define CMD_PUT_PC			0x00
#define CMD_GET_PC			0x01
#define CMD_PUT_NP			0x02
#define CMD_GET_NP			0x03
#define CMD_PUT_IORD_SHORT		0x40
#define CMD_PUT_IOWR_SHORT		0x44
#define CMD_PUT_MEMRD32_SHORT_1B	0x48
#define CMD_PUT_MEMWR32_SHORT_1B	0x4C

// eSPI response
#define RESP_DEFER		0x01
#define RESP_ACCEPT		0x08
#define RESP_WAIT		0x0F
#define RESP_FATAL_ERROR	0x03
#define RESP_NO_RESP		0xFF

// Status register
#define STATUS_VW_AVAIL		BIT(6)

#define STATUS_OK		0
#define STATUS_ERR		1
#define MAX_RESP_LEN		8

// KCS defines
#define KCS_STATUS_REG		0xCA3
#define   KCS_STATUS_OBF	BIT(0)
#define   KCS_STATUS_IBF	BIT(1)
#define   KCS_STATUS_CD		BIT(3)
#define   KCS_STATE(x) FIELD_GET(GENMASK(7, 6), (x))
#define   KCS_STATE_WRITE(x) (KCS_STATE(x) == 2)
#define   KCS_STATE_READ(x) (KCS_STATE(x) == 1)
#define   KCS_STATE_IDLE(x) (KCS_STATE(x) == 0)
#define   KCS_STATE_ERR(x) (KCS_STATE(x) == 3)
#define KCS_CMD_REG		0xCA3
#define KCS_DATA_REG		0xCA2
#define KCS_CMD_GET_STATUS	0x60
#define KCS_CMD_ABORT		0x60
#define KCS_CMD_WRITE_START	0x61
#define KCS_CMD_WRITE_END	0x62
#define KCS_CMD_READ		0x68

#define FLASH_R_LENGTH		64
#define OOB_R_LENGTH		80

struct get_pc_req {
	u8 cmd;
	u8 crc;
};

struct get_pc_resp {
	u8 code;
	u8 cycle_type;
	u8 tag_len_11_8;
	u8 len_7_0;
	u8 data[4 + 3]; // 2 byte status, 1 byte crc
} __attribute__((__packed__));

struct get_flash_c_req {
	u8 cmd;
	u8 crc;
};

struct get_flash_c_resp {
	u8 code;
	u8 cycle_type;
	u8 tag_len_11_8;
	u8 len_7_0;
	u8 data[FLASH_R_LENGTH + 3]; // FLASH_R_LENGTH bytes data + 2 byte status; 1 byte crc
} __attribute__((__packed__));

struct put_flash_c_req {
	u8 cmd;
	u8 cycle_type;
	u8 tag_len_11_8;
	u8 len_7_0;
	u8 data[FLASH_R_LENGTH + 1]; // FLASH_R_LENGTH bytes data + 1 byte crc
} __attribute__((__packed__));

struct get_status_req {
	u8 cmd;
	u8 crc;
};

struct get_status_resp {
	u8 code;
	u16 status;
	u8 crc;
} __attribute__((__packed__));

struct get_configuration_req {
	u8 cmd;
	u8 addr[2];
	u8 crc;
};

struct get_configuration_resp {
	u8 code;
	u32 data;
	u16 status;
	u8 crc;
} __attribute__((__packed__));

struct set_configuration_req {
	u8 cmd;
	u8 addr[2];
	u8 data[4];
	u8 crc;
};

struct espi_common_resp {
	u8 code;
	u16 status;
	u8 crc;
} __attribute__((__packed__));

struct get_vwire_req {
	u8 cmd;
	u8 crc;
} __attribute__((__packed__));

struct get_vwire_resp {
	u8 code;
	u8 len;
	u8 index;
	u8 data;
	u16 status;
	u8 crc;
} __attribute__((__packed__));

struct put_vwire_req {
	u8 cmd;
	u8 len;
	u8 index;
	u8 data;
	u8 crc;
} __attribute__((__packed__));

struct put_iowr_req {
	u8 cmd;
	u8 addr[2];
	u8 data_crc[5];
} __attribute__((__packed__));

struct put_iord_req {
	u8 cmd;
	u8 addr[2];
	u8 crc;
} __attribute__((__packed__));

struct put_iord_resp {
	u8 code;
	u32 data;
	u16 status;
	u8 crc;
} __attribute__((__packed__));

struct put_memwr32_req {
	u8 cmd;
	u8 addr[4];
	u8 data_crc[5];
} __attribute__((__packed__));

struct put_memrd32_req {
	u8 cmd;
	u8 addr[4];
	u8 crc;
} __attribute__((__packed__));

struct put_memrd32_resp {
	u8 code;
	u32 data;
	u16 status;
	u8 crc;
} __attribute__((__packed__));

struct get_devid_resp {
	u8 netfn_lun;
	u8 cmd;
	u8 comp_code;
	u8 dev_id;
	u8 dev_rev;
	u8 fw_rev1;
	u8 fw_rev2;
	u8 ipmi_ver;
	u8 dev_support;
	u8 manf_id[3];
	u8 prod_id[2];
	u8 aux_fw_rev[4];
} __attribute__((__packed__));

struct get_selinfo_resp {
	u8 netfn_lun;
	u8 cmd;
	u8 comp_code;
	u8 sel_ver;
	u8 entries_lsb;
	u8 entries_msb;
	u16 free_space;
	u32 add_timestamp;
	u32 erase_timestamp;
	u8 operation_supp;
} __attribute__((__packed__));

struct put_flash_np {
	u8 cmd;
	u8 cycle_type;
	u8 tag_len_11_8;
	u8 len_7_0;
	u8 addr_31_24;
	u8 addr_23_16;
	u8 addr_15_8;
	u8 addr_7_0;
	u8 data[FLASH_R_LENGTH + 1]; // FLASH_R_LENGTH bytes data + 1 byte crc
} __attribute__((__packed__));

struct put_flash_np_rpmc {
	u8 cmd;
	u8 cycle_type;
	u8 tag_len_11_8;
	u8 len_7_0;
	u8 data[FLASH_R_LENGTH + 1]; // FLASH_R_LENGTH bytes data + 1 byte crc
} __attribute__((__packed__));

struct get_flash_np_req {
	u8 cmd;
	u8 crc;
} __attribute__((__packed__));

struct get_flash_np_resp {
	u8 code;
	u8 cycle_type;
	u8 tag_len_11_8;
	u8 len_7_0;
	u8 addr_31_24;
	u8 addr_23_16;
	u8 addr_15_8;
	u8 addr_7_0;
	u8 data[FLASH_R_LENGTH + 3]; // FLASH_R_LENGTH bytes data + 2 byte status; 1 byte crc
} __attribute__((__packed__));

struct get_oob_req {
	u8 cmd;
	u8 crc;
} __attribute__((__packed__));

struct get_oob_resp {
	u8 code;
	u8 cycle_type;
	u8 tag_len_11_8;
	u8 len_7_0;
	u8 data[OOB_R_LENGTH + 3]; // OOB_R_LENGTH bytes data + 2 byte status; 1 byte crc
} __attribute__((__packed__));

struct put_oob_req {
	u8 cmd;
	u8 cycle_type;
	u8 tag_len_11_8;
	u8 len_7_0;
	u8 data[OOB_R_LENGTH + 1]; // OOB_R_LENGTH bytes data + 1 byte crc
} __attribute__((__packed__));

static u32 espi_port_state = 0;
static u32 rst_port_state = 0;  /* GPIO2 state for eSPI_nRST (GPIO95) */
static u8 wait_state = 16;
static bool debug = false;
static u8 io_mode = ESPI_IO_MODE_SINGLE; /* current I/O mode: single/dual/quad */
/*
 * use_alert_pin: select which pin carries the slave ALERT signal.
 *   false (default) = IO1-based alert (eSPI "In-Band ALERT" mode)
 *                     IO1 doubles as data line and alert line; only
 *                     usable when io_mode == SINGLE.
 *   true            = dedicated ALERT# pin (eSPI_ALERT, BIT(8), GPIO168)
 *                     always an input regardless of IO mode; event
 *                     detection works in Single, Dual, and Quad modes.
 * Set via the "alertmode <0|1>" CLI command before (or after) "init".
 * Re-calling espi_init() after changing this variable reconfigures the
 * edge-event register on the selected pin.
 */
static bool use_alert_pin = false;

void inline espi_port_update(u32 set, u32 clear)
{
	espi_port_state |= set;
	espi_port_state &= ~clear;
	writel(espi_port_state, PORT_OUT);
}

void inline espi_port_set(u32 state)
{
	espi_port_state |= state;
	writel(espi_port_state, PORT_OUT);
}

void inline espi_port_clear(u32 state)
{
	espi_port_state &= ~state;
	writel(espi_port_state, PORT_OUT);
}

/*
 * espi_port_replace - clear a mask of bits, then set new values in one write.
 * Unlike espi_port_update (set-then-clear), this does clear-then-set, so bits
 * that appear in both `mask` and `val` are correctly driven HIGH.
 * Use this when atomically switching IO lines (e.g. quad TX nibble change).
 */
void inline espi_port_replace(u32 mask, u32 val)
{
	espi_port_state &= ~mask;   /* clear all bits in the field */
	espi_port_state |= val;     /* set desired bits */
	writel(espi_port_state, PORT_OUT);
}

/*
 * rst_nrst_assert / rst_nrst_deassert
 * Drive eSPI_nRST (GPIO95, GPIO2 bit 31) low/high.
 * These operate on RST_OUT/rst_port_state, which is separate from the
 * GPIO5 espi_port_state used by the espi_port_* helpers above.
 */
void inline rst_nrst_assert(void)
{
	rst_port_state &= ~eSPI_nRST;
	writel(rst_port_state, RST_OUT);
}

void inline rst_nrst_deassert(void)
{
	rst_port_state |= eSPI_nRST;
	writel(rst_port_state, RST_OUT);
}

/*
 * Switch I/O mode. Must be called after espi_init().
 * mode: ESPI_IO_MODE_SINGLE / ESPI_IO_MODE_DUAL / ESPI_IO_MODE_QUAD
 *
 * GPIO direction rules:
 *   Single: IO0=out, IO1=in
 *   Dual:   during CMD phase IO0/IO1=out; during resp phase IO0/IO1=in
 *           (espi_tar_dual handles the switch at TAR boundary)
 *   Quad:   during CMD phase IO0~IO3=out; during resp phase IO0~IO3=in
 *           (espi_tar_quad handles the switch at TAR boundary)
 */
static void espi_set_io_mode(u8 mode)
{
	io_mode = mode;
	switch (mode) {
	case ESPI_IO_MODE_DUAL:
		printf("espi: switch to Dual I/O mode\n");
		/* Clear IO2/IO3 output enable in case we are downgrading from Quad */
		writel(eSPI_IO2 | eSPI_IO3, PORT_OEC);
		/* input->output: IEM must be cleared before OES is set.
		 * IO1 may have been input (single mode); clear it from IEM first. */
		clrsetbits_le32(PORT_IEM, ALL_ESPI_PORT_PINS, use_alert_pin ? eSPI_ALERT : 0);
		/* IO0/IO1 start as output (command phase) */
		writel(eSPI_nCS | eSPI_CLK | eSPI_IO0 | eSPI_IO1, PORT_OES);
		break;
	case ESPI_IO_MODE_QUAD:
		printf("espi: switch to Quad I/O mode\n");
		/* input->output: IEM must be cleared before OES is set.
		 * IO1 may have been input (single mode); clear it from IEM first. */
		clrsetbits_le32(PORT_IEM, ALL_ESPI_PORT_PINS, use_alert_pin ? eSPI_ALERT : 0);
		/* IO0~IO3 start as output (command phase) */
		writel(eSPI_nCS | eSPI_CLK | eSPI_IO0 | eSPI_IO1 | eSPI_IO2 | eSPI_IO3, PORT_OES);
		break;
	case ESPI_IO_MODE_SINGLE:
	default:
		printf("espi: switch to Single I/O mode\n");
		io_mode = ESPI_IO_MODE_SINGLE;
		/* Clear IO1/IO2/IO3 output enable in case we are downgrading from Dual or Quad */
		writel(eSPI_IO1 | eSPI_IO2 | eSPI_IO3, PORT_OEC);
		writel(eSPI_nCS | eSPI_CLK | eSPI_IO0, PORT_OES);
		/* IO1 is always data input in single mode; additionally enable
		 * ALERT# input buffer when use_alert_pin is selected */
		clrsetbits_le32(PORT_IEM, ALL_ESPI_PORT_PINS, eSPI_IO1 | (use_alert_pin ? eSPI_ALERT : 0));
		break;
	}
}

static void espi_init(void)
{
	u32 reg;

	if (IS_ENABLED(CONFIG_ARCH_NPCM8XX)) {
		// clear MFSEL1.26 & MFSEL4.8
		reg = readl(0xF0800260) & ~BIT(26);
		writel(reg, 0xF0800260);
		reg = readl(0xF080026C) & ~BIT(8);
		writel(reg, 0xF080026C);
	} else {
		// set MFSEL1.26 & clear MFSEL4.8
		reg = readl(0xF080000C) | BIT(26);
		writel(reg, 0xF080000C);
		reg = readl(0xF08000B0) & ~BIT(8);
		writel(reg, 0xF08000B0);
	}

	printf("espi: init gpios\n");
	// initial state GPIO5: nCS high, CLK low, IO0 high, IO2/IO3 low
	espi_port_state = readl(PORT_OUT);
	espi_port_update(eSPI_nCS | eSPI_IO0, eSPI_CLK | eSPI_IO2 | eSPI_IO3);
	// initial state GPIO2: nRST high; configure as output
	rst_port_state = readl(RST_OUT);
	rst_nrst_deassert();
	writel(eSPI_nRST, RST_OES);
	/*
	 * After reset, slave always comes up in single mode per eSPI spec.
	 * Reset io_mode to single and apply the matching GPIO direction via
	 * espi_set_io_mode() so PORT_OES/IEM are always consistent with io_mode.
	 */
	io_mode = ESPI_IO_MODE_SINGLE;
	espi_set_io_mode(ESPI_IO_MODE_SINGLE);

	// set event on alert pin (IO1 or ALERT# depending on use_alert_pin)
	u32 alert_bit = use_alert_pin ? eSPI_ALERT : eSPI_IO1;

	writel(alert_bit, PORT_EVENC);
	writel(alert_bit, PORT_EVST);
	writel(alert_bit, PORT_EVENS);
	clrsetbits_le32(PORT_EVTYP, ALL_ESPI_PORT_PINS, alert_bit);
	clrsetbits_le32(PORT_EVBE, ALL_ESPI_PORT_PINS, alert_bit);

	printf("espi reset\n");
	udelay(100);
	// assert eSPI_nRST (GPIO95, GPIO2)
	rst_nrst_assert();
	udelay(100);
	// deassert eSPI_nRST
	rst_nrst_deassert();
	udelay(100);
}

/*
 * The Serial Clock must be low at the assertion edge of the Chip Select# while eSPI
 * Reset# has been de-asserted. The first data is launched from master while the serial
 * clock is still low and sampled on the first rising edge of the clock by slave. Subsequent
 * data is launched on the falling edge of the clock from master and sampled on the
 * rising edge of the clock by slave. The data is launched from slave on the falling edge
 * of the clock. The master could implement a more flexible sampling scheme since it
 * controls the clock.
 */

/* Single mode: 1 bit per clock on IO0 (TX), IO1 (RX). 8 clocks per byte. */
static void espi_sendbyte(u8 data)
{
	int i;
	u32 val = 0;

	if (data & BIT(7))
		espi_port_update(eSPI_IO0, eSPI_CLK); //clk_low + IO0
	else
		espi_port_clear(eSPI_IO0 | eSPI_CLK); //clk_low

	espi_port_set(eSPI_CLK); // clk_high
	for (i = 6; i >= 0; i--) {
		val &= ~eSPI_CLK;
		if (data & BIT(i))
			espi_port_update(eSPI_IO0, eSPI_CLK); //clk_low + IO0
		else
			espi_port_clear(eSPI_IO0 | eSPI_CLK); //clk_low
		espi_port_set(eSPI_CLK); // clk_high
	}
	espi_port_clear(eSPI_CLK); //clk_low
}

/*
 * Dual mode: 2 bits per clock on IO0/IO1 (both output during TX).
 * MSB first: bit[7:6], bit[5:4], bit[3:2], bit[1:0]. 4 clocks per byte.
 * IO line assignment per eSPI spec (IO0=LSB, IO1=MSB):
 *   IO1 = high bit of pair, IO0 = low bit of pair
 */
static void espi_sendbyte_dual(u8 data)
{
	int i;
	u32 io;

	/* Pairs: [7,6], [5,4], [3,2], [1,0] — IO1=high bit, IO0=low bit */
	for (i = 7; i >= 1; i -= 2) {
		io = 0;
		if (data & BIT(i))
			io |= eSPI_IO1;     /* high bit of pair → IO1 (MSB) */
		if (data & BIT(i - 1))
			io |= eSPI_IO0;     /* low bit of pair  → IO0 (LSB) */
		espi_port_replace(eSPI_CLK | eSPI_IO0 | eSPI_IO1, io); /* CLK low, set IO */
		espi_port_set(eSPI_CLK);                               /* CLK high */
	}
	/* CLK left high on exit; caller (espi_send) issues final CLK low */
}

/*
 * Quad mode: 4 bits per clock on IO0~IO3 (all output during TX).
 * High nibble first: bit[7:4], then bit[3:0]. 2 clocks per byte.
 * IO line assignment per eSPI spec (IO0=LSB, IO3=MSB):
 *   High nibble: IO3=bit7, IO2=bit6, IO1=bit5, IO0=bit4
 *   Low  nibble: IO3=bit3, IO2=bit2, IO1=bit1, IO0=bit0
 */
static void espi_sendbyte_quad(u8 data)
{
	u32 io;

	/* High nibble: IO3=bit7, IO2=bit6, IO1=bit5, IO0=bit4 */
	io = 0;
	if (data & BIT(4)) io |= eSPI_IO0;
	if (data & BIT(5)) io |= eSPI_IO1;
	if (data & BIT(6)) io |= eSPI_IO2;
	if (data & BIT(7)) io |= eSPI_IO3;
	espi_port_replace(eSPI_CLK | eSPI_IO0 | eSPI_IO1 | eSPI_IO2 | eSPI_IO3, io); /* CLK low, set IO */
	espi_port_set(eSPI_CLK);                                                       /* CLK high */

	/* Low nibble: IO3=bit3, IO2=bit2, IO1=bit1, IO0=bit0 */
	io = 0;
	if (data & BIT(0)) io |= eSPI_IO0;
	if (data & BIT(1)) io |= eSPI_IO1;
	if (data & BIT(2)) io |= eSPI_IO2;
	if (data & BIT(3)) io |= eSPI_IO3;
	espi_port_replace(eSPI_CLK | eSPI_IO0 | eSPI_IO1 | eSPI_IO2 | eSPI_IO3, io); /* CLK low, set IO */
	espi_port_set(eSPI_CLK);                                                       /* CLK high */
	/* CLK left high on exit; caller (espi_send) issues final CLK low */
}

/* Single mode RX: 1 bit per clock on IO1. 8 clocks per byte. */
static u8 espi_recv_byte_single(void)
{
	// Clock start high -- ended at high.
	int i;
	u32 val;
	u8 in = 0;

	for (i = 7; i >= 0; i--) {
		espi_port_set(eSPI_CLK | eSPI_IO0); // clk_high
		val = readl(PORT_IN);
		if (val & eSPI_IO1)
			in |= BIT(i);
		espi_port_clear(eSPI_CLK);
	}
	return in;
}

/*
 * Dual mode RX: 2 bits per clock on IO0/IO1 (both input during RX).
 * IO line assignment per eSPI spec (IO0=LSB, IO1=MSB):
 *   IO1 = high bit of pair, IO0 = low bit of pair
 */
static u8 espi_recv_byte_dual(void)
{
	int i;
	u32 val;
	u8 in = 0;

	/* Pairs: [7,6], [5,4], [3,2], [1,0] — IO1=high bit, IO0=low bit */
	for (i = 7; i >= 1; i -= 2) {
		espi_port_set(eSPI_CLK);
		val = readl(PORT_IN);
		if (val & eSPI_IO1)
			in |= BIT(i);       /* IO1 (MSB) → high bit of pair */
		if (val & eSPI_IO0)
			in |= BIT(i - 1);   /* IO0 (LSB) → low bit of pair */
		espi_port_clear(eSPI_CLK);
	}
	return in;
}

/*
 * Quad mode RX: 4 bits per clock on IO0~IO3 (all input during RX).
 * IO line assignment per eSPI spec (IO0=LSB, IO3=MSB):
 *   High nibble: IO3=bit7, IO2=bit6, IO1=bit5, IO0=bit4
 *   Low  nibble: IO3=bit3, IO2=bit2, IO1=bit1, IO0=bit0
 */
static u8 espi_recv_byte_quad(void)
{
	u32 val;
	u8 in = 0;

	/* First clock: high nibble, IO3=bit7 IO2=bit6 IO1=bit5 IO0=bit4 */
	espi_port_set(eSPI_CLK);
	val = readl(PORT_IN);
	if (val & eSPI_IO0) in |= BIT(4);
	if (val & eSPI_IO1) in |= BIT(5);
	if (val & eSPI_IO2) in |= BIT(6);
	if (val & eSPI_IO3) in |= BIT(7);
	espi_port_clear(eSPI_CLK);

	/* Second clock: low nibble, IO3=bit3 IO2=bit2 IO1=bit1 IO0=bit0 */
	espi_port_set(eSPI_CLK);
	val = readl(PORT_IN);
	if (val & eSPI_IO0) in |= BIT(0);
	if (val & eSPI_IO1) in |= BIT(1);
	if (val & eSPI_IO2) in |= BIT(2);
	if (val & eSPI_IO3) in |= BIT(3);
	espi_port_clear(eSPI_CLK);

	return in;
}

static void espi_send(u8 *out, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		switch (io_mode) {
		case ESPI_IO_MODE_DUAL:
			espi_sendbyte_dual(out[i]);
			break;
		case ESPI_IO_MODE_QUAD:
			espi_sendbyte_quad(out[i]);
			break;
		default:
			espi_sendbyte(out[i]);
			break;
		}
	}
	/* For dual/quad, sendbyte leaves CLK high after the last clock.
	 * Pull CLK low here once so TAR and subsequent logic see CLK=low. */
	if (io_mode != ESPI_IO_MODE_SINGLE)
		espi_port_clear(eSPI_CLK);
}

/* Dispatch recv according to current io_mode */
static inline u8 espi_recv_byte(void)
{
	switch (io_mode) {
	case ESPI_IO_MODE_DUAL:
		return espi_recv_byte_dual();
	case ESPI_IO_MODE_QUAD:
		return espi_recv_byte_quad();
	default:
		return espi_recv_byte_single();
	}
}

static int espi_wait_response(u8 *code)
{
	int count = wait_state;
	int rc = 0;

	do {
		*code = espi_recv_byte();
		if (*code != RESP_WAIT && *code != RESP_ACCEPT && *code != RESP_DEFER) {
			rc = -EIO;
			break;
		}
		if (count-- == 0 && *code != RESP_ACCEPT && *code != RESP_DEFER) {
			rc = -ETIMEDOUT;
			break;
		}
	} while (*code != RESP_ACCEPT && *code != RESP_DEFER);

	if (count < 0 && *code == RESP_DEFER)
		return -EINPROGRESS;

	return rc;
}

static void espi_read_status(u16 *status)
{
	u8 *in = (u8 *)status;

	*in++ = espi_recv_byte();
	*in = espi_recv_byte();
	espi_recv_byte(); // read crc
}

static u16 espi_read_response(u8 *resp, int len)
{
	u16 status = STATUS_OK;
	int i, rc, cnt;

	rc = espi_wait_response(resp);

	if (rc != 0) {
		if (rc != -EINPROGRESS) {
			espi_read_status(&status);
			//printf("espi response error: code=0x%02x, status=0x%04x\n", *resp, status);
			return status;
		} else {
			// case for PUT_MEMRD32_SHORT DEFER
			cnt = (len % 4) ? (len % 4) : 4;
			resp+=cnt;
			len-=cnt;
		}
	}

	resp++;
	len--;
	for (i = 0; i < len; i++, resp++)
		*resp = espi_recv_byte();

	return status;
}

/*
 * Turn-Around (TAR): master drives all active IO lines high for 1st clock,
 * then tri-states them. Total 2 clocks.
 *
 * Single: drives IO0 high for 1st clock, floats on 2nd (IO1 stays input).
 * Dual:   drives IO0+IO1 high for 1st clock, tri-states + switches to input.
 * Quad:   drives IO0~IO3 high for 1st clock, tri-states + switches to input.
 */
static void espi_tar(void)
{
	switch (io_mode) {
	case ESPI_IO_MODE_DUAL:
		/* 1st TAR clock: drive IO0/IO1 high */
		espi_port_update(eSPI_IO0 | eSPI_IO1, eSPI_CLK);
		espi_port_set(eSPI_CLK);
		espi_port_clear(eSPI_CLK);
		/* 2nd TAR clock: tri-state IO0/IO1, switch to input for response phase.
		 * IO0/IO1 must be in IEM for DATA receive regardless of alert mode.
		 * Additionally enable ALERT# input buffer when use_alert_pin is set.
		 * nRST is on GPIO2 (RST_OES) so it is NOT included here. */
		writel(eSPI_IO0 | eSPI_IO1, PORT_OEC);
		clrsetbits_le32(PORT_IEM, ALL_ESPI_PORT_PINS, eSPI_IO0 | eSPI_IO1 | (use_alert_pin ? eSPI_ALERT : 0));
		espi_port_set(eSPI_CLK);
		espi_port_clear(eSPI_CLK);
		break;
	case ESPI_IO_MODE_QUAD:
		/* 1st TAR clock: drive IO0~IO3 high */
		espi_port_update(eSPI_IO0 | eSPI_IO1 | eSPI_IO2 | eSPI_IO3, eSPI_CLK);
		espi_port_set(eSPI_CLK);
		espi_port_clear(eSPI_CLK);
		/* 2nd TAR clock: tri-state IO0~IO3, switch to input for response phase.
		 * IO0~IO3 must be in IEM for DATA receive regardless of alert mode.
		 * Additionally enable ALERT# input buffer when use_alert_pin is set.
		 * nRST is on GPIO2 (RST_OES) so it is NOT included here. */
		writel(eSPI_IO0 | eSPI_IO1 | eSPI_IO2 | eSPI_IO3, PORT_OEC);
		clrsetbits_le32(PORT_IEM, ALL_ESPI_PORT_PINS, eSPI_IO0 | eSPI_IO1 | eSPI_IO2 | eSPI_IO3 | (use_alert_pin ? eSPI_ALERT : 0));
		espi_port_set(eSPI_CLK);
		espi_port_clear(eSPI_CLK);
		break;
	default: /* single */
		espi_port_set(eSPI_CLK | eSPI_IO0);
		espi_port_clear(eSPI_CLK);
		espi_port_set(eSPI_CLK);
		espi_port_clear(eSPI_CLK);
		break;
	}
}

/*
 * After the response phase, restore IO direction back to output
 * so the next command phase can drive the lines.
 * Only needed for dual/quad where TAR switched lines to input.
 */
static void espi_restore_io_output(void)
{
	switch (io_mode) {
	case ESPI_IO_MODE_DUAL:
		/* ALERT# pin stays as input during command phase when selected.
		 * nRST is on GPIO2 (RST_OES) and stays output regardless. */
		clrsetbits_le32(PORT_IEM, ALL_ESPI_PORT_PINS, use_alert_pin ? eSPI_ALERT : 0);
		writel(eSPI_nCS | eSPI_CLK | eSPI_IO0 | eSPI_IO1, PORT_OES);
		break;
	case ESPI_IO_MODE_QUAD:
		/* ALERT# pin stays as input during command phase when selected.
		 * nRST is on GPIO2 (RST_OES) and stays output regardless. */
		clrsetbits_le32(PORT_IEM, ALL_ESPI_PORT_PINS, use_alert_pin ? eSPI_ALERT : 0);
		writel(eSPI_nCS | eSPI_CLK | eSPI_IO0 | eSPI_IO1 | eSPI_IO2 | eSPI_IO3, PORT_OES);
		break;
	default:
		break;
	}
}

static u16 espi_get_status(void)
{
	struct get_status_req req = {0};
	struct get_status_resp resp = {0};

	req.cmd = CMD_GET_GET_STATUS;
	req.crc = crc8(0, (u8 *)&req, sizeof(req) - 1);

	debug("req.cmd 0x%x\n", req.cmd );
	debug("req.crc 0x%x\n", req.crc);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	espi_read_response((u8 *)&resp, sizeof(resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	debug("resp.code 0x%x\n", resp.code);
	debug("resp.status 0x%x\n", resp.status);
	debug("resp.crc %x\n", resp.crc);

	return resp.status;
}

static int espi_get_flash_c(int len, u32 dest)
{
	struct get_flash_c_req req = {0};
	struct get_flash_c_resp resp = {0};
	u8 *addr = (u8 *)(uintptr_t)dest;
	u16 status;

	req.cmd = CMD_GET_FLASH_C;
	req.crc = crc8(0, (u8 *)&req, sizeof(req) - 1);

	debug("req.cmd 0x%x\n", req.cmd );
	debug("req.crc 0x%x\n", req.crc);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);
	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	status = espi_read_response((u8 *)&resp, sizeof(resp) - (FLASH_R_LENGTH - len));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	debug("resp.code 0x%x\n", resp.code);
	debug("resp.cycle_type 0x%x\n", resp.cycle_type);
	debug("resp.status %x\n", resp.data[len] | (resp.data[len + 1] << 8));
	debug("resp.crc %x\n", resp.data[len + 2]);

	if (resp.cycle_type == 0xf) {
		memcpy(addr, resp.data, len);
	} else if (resp.cycle_type != 6){
		printf("flash cycle type error (type=0x%x)\n", resp.cycle_type);
		return STATUS_ERR;
	}

	if (status) {
		printf("espi response error: code=0x%02x, status=0x%04x\n", resp.code, status);
		return STATUS_ERR;
	}

	return STATUS_OK;
}

static int espi_put_flash_c(u8 type, int tag_len_L, int len_H)
{
	struct put_flash_c_req req = {0};
	struct espi_common_resp resp = {0};
	u16 status;

	req.cmd = CMD_PUT_FLASH_C;
	req.tag_len_11_8 = tag_len_L;
	req.len_7_0 = len_H;
	if (type == CYCLE_FLASH_READ) {
		req.cycle_type = 0x0f;
		for (int i=0; i<len_H; i++)
			req.data[i] = i;
		req.data[len_H] = crc8(0, (u8 *)&req, sizeof(req) + len_H - FLASH_R_LENGTH - 1);
	} else if (type == CYCLE_FLASH_WRITE || type == CYCLE_FLASH_ERASE) {
		req.cycle_type = 0x06;
		req.data[0] = crc8(0, (u8 *)&req, sizeof(req) - FLASH_R_LENGTH - 1);
	}

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	if (type == CYCLE_FLASH_READ) {
		espi_send((u8 *)&req, sizeof(req) + len_H - FLASH_R_LENGTH);
	} else if (type == CYCLE_FLASH_WRITE || type == CYCLE_FLASH_ERASE) {
		espi_send((u8 *)&req, sizeof(req) - FLASH_R_LENGTH);
	}
	espi_tar();
	status = espi_read_response((u8 *)&resp, sizeof(resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (debug) {
		printf("resp.code 0x%x\n", resp.code);
		printf("resp.status 0x%x\n", resp.status);
	}

	if (status) {
		printf("\n  put_flash_c response error: code=0x%02x, status=0x%04x\n", resp.code, status);
		return STATUS_ERR;
	}

	return STATUS_OK;
}

static int wait_for_flash_np_free(void)
{
	u16 status;
	int count = 1000000;

	do {
		status = espi_get_status();
		if (status & BIT(9)) {
			debug("FLASH_NP_FREE  count %d\n", count);
			return 0;
		}

		if (count-- == 0) {
			printf("\nwait_for_flash_np_free timeout\n");
			return -ETIMEDOUT;
		}
		//udelay(100);
		udelay(1);
	} while (1);

	return -ETIMEDOUT;
}

static int espi_put_flash_np(u8 type, u32 addr, int len, u32 data_addr, u8 tag)
{
	struct put_flash_np req;
	struct espi_common_resp resp;
	u8 /*tag = 0,*/ crc;
	u16 status;
	int req_len;
	u8 *data = (u8 *)(uintptr_t)data_addr;

	if (wait_for_flash_np_free())
		return STATUS_ERR;

	req.cmd = CMD_PUT_FLASH_NP;
	req.cycle_type = type;
	req.tag_len_11_8 = (tag << 4) | ((len & 0xf00) >> 8);
	req.len_7_0 =  len & 0xff;
	req.addr_7_0= addr & 0xff;
	req.addr_15_8 = (addr & 0x0000FF00) >> 8;
	req.addr_23_16 = (addr & 0x00FF0000) >> 16;
	req.addr_31_24 = (addr & 0xFF000000) >> 24;
	if (type == CYCLE_FLASH_READ || type == CYCLE_FLASH_ERASE) {
		crc = crc8(0, (u8 *)&req, sizeof(req) - FLASH_R_LENGTH - 1);
		req.data[0] = crc;
		req_len = sizeof(req) - FLASH_R_LENGTH;
	} else if (type == CYCLE_FLASH_WRITE) {
		memcpy(req.data, data, len);
		crc = crc8(0, (u8 *)&req, sizeof(req) + len - FLASH_R_LENGTH - 1);
		req.data[len] = crc;
		req_len = sizeof(req) + len - FLASH_R_LENGTH;
	}

	debug("req.cmd 0x%x\n", req.cmd);
	debug("req.cycle_type 0x%x\n", req.cycle_type);
	debug("req.tag_len_11_8 0x%x\n", req.tag_len_11_8);
	debug("req.len_7_0 0x%x\n", req.len_7_0);
	debug("req.addr_7_0 0x%x\n", req.addr_7_0);
	debug("req.addr_15_8 0x%x\n", req.addr_15_8);
	debug("req.addr_23_16 0x%x\n", req.addr_23_16);
	debug("req.addr_31_24 0x%x\n", req.addr_31_24);
	debug("req.crc 0x%x\n", crc);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, req_len);
	espi_tar();
	status = espi_read_response((u8 *)&resp, sizeof(resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	debug("resp.code 0x%x\n", resp.code);
	debug("resp.status 0x%x\n", resp.status);

	if (status) {
		printf("\n  espi_put_flash_np response error: code=0x%02x, status=0x%04x\n", resp.code, status);
		return STATUS_ERR;
	}

	return STATUS_OK;
}

static int espi_put_flash_np_rpmc(u8 type, int len, u32 data_addr, u8 tag)
{
	struct put_flash_np_rpmc req;
	struct espi_common_resp resp;
	u8 /*tag = 0,*/ crc;
	u16 status;
	int req_len;
	u8 *data = (u8 *)(uintptr_t)data_addr;

	if (wait_for_flash_np_free())
		return STATUS_ERR;

	req.cmd = CMD_PUT_FLASH_NP;
	req.cycle_type = type;
	req.tag_len_11_8 = (tag << 4) | ((len & 0xf00) >> 8);
	req.len_7_0 =  len & 0xff;
	if ((type & ~0x60) == CYCLE_FLASH_RPMC_OP1) {
		memcpy(req.data, data, len);
		crc = crc8(0, (u8 *)&req, sizeof(req) + len - FLASH_R_LENGTH - 1);
		req.data[len] = crc;
		req_len = sizeof(req) + len - FLASH_R_LENGTH;
	}
	else if ((type & ~0x60) == CYCLE_FLASH_RPMC_OP2) {
		crc = crc8(0, (u8 *)&req, sizeof(req) - FLASH_R_LENGTH - 1);
		req.data[0] = crc;
		req_len = sizeof(req) - FLASH_R_LENGTH;
	}

	if (debug) {
		printf("req.cmd 0x%x\n", req.cmd);
		printf("req.cycle_type 0x%x\n", req.cycle_type);
		printf("req.tag_len_11_8 0x%x\n", req.tag_len_11_8);
		printf("req.len_7_0 0x%x\n", req.len_7_0);
		printf("req.crc 0x%x\n", crc);
	}

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, req_len);
	espi_tar();
	status = espi_read_response((u8 *)&resp, sizeof(resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (debug) {
		printf("resp.code 0x%x\n", resp.code);
		printf("resp.status 0x%x\n", resp.status);
	}

	if (status) {
		printf("\n espi_put_flash_np_rpmc response error: code=0x%02x, status=0x%04x\n", resp.code, status);
		return STATUS_ERR;
	}

	return STATUS_OK;
}

static int espi_get_flash_np(u8 index, int count, u8 *in)
{
	struct get_flash_np_req req = {0};
	struct get_flash_np_resp *resp = (struct get_flash_np_resp *)in;
	u16 status;

	req.cmd = CMD_GET_FLASH_NP;
	req.crc = crc8(0, &req.cmd, 1);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, sizeof(req));
	espi_tar();

	if (index == CYCLE_FLASH_READ || index == CYCLE_FLASH_ERASE) {
		status = espi_read_response((u8 *)resp, sizeof(*resp) - (FLASH_R_LENGTH));
	} else if (index == CYCLE_FLASH_WRITE) {
		status = espi_read_response((u8 *)resp, sizeof(*resp) + count - (FLASH_R_LENGTH));
	}
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (debug) {
		printf("resp.cycle_type 0x%x\n", resp->cycle_type);
		printf("resp.tag_len_11_8 0x%x\n", resp->tag_len_11_8);
		printf("resp.len_7_0 0x%x\n", resp->len_7_0);
		printf("resp.addr_7_0 0x%x\n", resp->addr_7_0);
		printf("resp.addr_15_8 0x%x\n", resp->addr_15_8);
		printf("resp.addr_23_16 0x%x\n", resp->addr_23_16);
		printf("resp.addr_31_24 0x%x\n", resp->addr_31_24);
		if (index == CYCLE_FLASH_READ || index == CYCLE_FLASH_ERASE) {
			printf("resp.status 0x%x\n", resp->data[0] | (resp->data[1] << 8));
			printf("resp.crc 0x%x\n", resp->data[2]);
		} else if (index == CYCLE_FLASH_WRITE) {
			printf("resp.status 0x%x\n", resp->data[resp->len_7_0] | (resp->data[resp->len_7_0 + 1] << 8));
			printf("resp.crc 0x%x\n", resp->data[resp->len_7_0 + 2]);
		}
	}

	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
				__func__, resp->code, status);
		return STATUS_ERR;
	}

	return STATUS_OK;
}

/*
 * wait_alert_and_dispatch() - wait for ALERT# / IO1 edge then return the
 * channel(s) that have data pending, identified via GET_STATUS.
 *
 * Background: in U-Boot there are no interrupts, threads, or background
 * tasks.  The GPIO edge-event register (PORT_EVST) latches the falling edge
 * of the alert signal, but it is only observed when this code actively polls
 * it.  ALERT# / IO1 alone does not tell the master *which* channel raised
 * the alert; that requires a GET_STATUS command.
 *
 * This helper is intended for slave-initiated events (e.g. target pushing
 * SLV_BOOT_DONE on the VW channel after channel enable), where the master
 * has not just issued a transaction and therefore has no natural place to
 * call wait_alert().
 *
 * Usage pattern:
 *   u16 pending;
 *   if (wait_alert_and_dispatch(&pending) == 0) {
 *       if (pending & BIT(6))  espi_get_vwire(...);   // VW_AVAIL
 *       if (pending & BIT(4))  espi_get_pc(...);       // PC_AVAIL
 *       if (pending & BIT(7))  espi_get_oob(...);      // OOB_AVAIL
 *       if (pending & BIT(12)) espi_get_flash_c(...);  // FLASH_C_AVAIL
 *   }
 *
 * Alternative when ALERT# hardware is not wired (IO1-alert in dual/quad):
 *   Skip wait_alert_and_dispatch() and go straight to GET_STATUS polling
 *   (the existing wait_for_vw_avail() / wait_for_oob_avail() etc.).  Those
 *   functions already do exactly that and are always safe to call.
 *
 * Returns 0 on alert detected, -ETIMEDOUT on timeout.
 * *status_out receives the raw GET_STATUS value (0 if timed out).
 */
static int wait_alert_and_dispatch(u16 *status_out)
{
	u32 val;
	int count = 1000000;
	u32 alert_bit = use_alert_pin ? eSPI_ALERT : eSPI_IO1;

	if (status_out)
		*status_out = 0;

	/*
	 * In IO1-alert + dual/quad mode the edge event will never fire because
	 * IO1 is a data line.  Skip directly to GET_STATUS.
	 */
	if (io_mode != ESPI_IO_MODE_SINGLE && !use_alert_pin)
		goto get_status;

	/* Wait for falling-edge latch on the alert pin */
	do {
		val = readl(PORT_EVST);
		if (val & alert_bit) {
			writel(alert_bit, PORT_EVST); /* clear latch */
			debug("eSPI_ALERT (dispatch)\n");
			break;
		}
		if (count-- == 0) {
			printf("\nwait_alert_and_dispatch: timeout waiting for alert\n");
			return -ETIMEDOUT;
		}
		udelay(1);
	} while (1);

get_status:
	if (status_out)
		*status_out = espi_get_status();
	return 0;
}

static int wait_alert(void)
{
	u32 val;
	int count = 1000000;
	u32 alert_bit = use_alert_pin ? eSPI_ALERT : eSPI_IO1;

	/*
	 * In IO1-alert mode, IO1 is a shared data/alert line.  In dual/quad
	 * mode it is actively driven as a data line so the edge-event register
	 * will never fire.  Fall back to a short delay and let the caller use
	 * GET_STATUS polling instead.
	 *
	 * In ALERT#-pin mode the dedicated pin is always an input regardless
	 * of IO mode, so edge-event detection works correctly in all modes.
	 */
	if (io_mode != ESPI_IO_MODE_SINGLE && !use_alert_pin) {
		udelay(100); /* give slave time to post the completion */
		return 0;
	}

	do {
		val = readl(PORT_EVST);
		if (val & alert_bit) {
			writel(alert_bit, PORT_EVST);
			debug("eSPI_ALERT\n");
			break;
		}
		if (count-- == 0) {
			printf("\nwait_alert timeout\n");
			return -ETIMEDOUT;
		}
		udelay(100);
	} while (1);

	return 0;
}

static int wait_for_flash_c_avail(void)
{
	u16 status;
	int count = 1000000;

	do {
		status = espi_get_status();
		if (status & BIT(12)) {
			debug("FLASH_C_AVAIL  count %d\n", count);
			return 0;
		}

		if (count-- == 0) {
			printf("\nwait_for_flash_c_avail timeout\n");
			return -ETIMEDOUT;
		}
		//udelay(100);
		udelay(1);
	} while (1);

	return -ETIMEDOUT;
}

static int wait_for_flash_np_avail(void)
{
	u16 status;
	int count = 1000000;

	do {
		status = espi_get_status();
		if (status & BIT(13)) {
			debug("FLASH_NP_AVAIL  count %d\n", count);
			return 0;
		}

		if (count-- == 0) {
			printf("\nwait_for_flash_np_avail timeout\n");
			return -ETIMEDOUT;
		}
		//udelay(100);
		udelay(1);
	} while (1);

	return -ETIMEDOUT;
}

static int espi_flash_read(u32 addr, int count, u32 dest, u8 tag)
{
	int i, index = 0, len;
	static ulong time_start;

	time_start = get_timer(0);

	printf("Load address: 0x%x\n", dest);
	if (debug) {
		puts("Loading: *\b");
	}

	index = count /FLASH_R_LENGTH;
	if (count % FLASH_R_LENGTH)
		index++;

	for (i = 0; i < index; i ++) {
		if ((i == index - 1) && (count % FLASH_R_LENGTH))
			len = count % FLASH_R_LENGTH;
		else
			len = FLASH_R_LENGTH;

		if (espi_put_flash_np(CYCLE_FLASH_READ, addr + i * FLASH_R_LENGTH, len, 0, tag))
			return STATUS_ERR;

		if (wait_alert())
			return STATUS_ERR;

		if (wait_for_flash_c_avail())
			return STATUS_ERR;

		if (espi_get_flash_c(len, dest + i * FLASH_R_LENGTH))
			return STATUS_ERR;

		//status = espi_get_status();
		//printf("status = 0x%x\n", status);
		if (debug) {
			putc('#');
			if ((i % FLASH_R_LENGTH) == 0 && i > 0)
				puts("\n\t ");
		}

		//udelay(20);
	}
	time_start = get_timer(time_start);
	if (time_start > 0) {
		if (debug) {
			puts("\n\t ");	/* Line up with "Loading: " */
			print_size(count / time_start * 1000, "/s");
		}
	}

	if (debug) {
		printf("\ndone\n");
	}

	return STATUS_OK;
}

static int espi_flash_write(u32 addr, int count, u32 data_addr, u8 tag)
{
	int i, index = 0, len;
	static ulong time_start;
	//u32 *data = (u32 *)(uintptr_t)data_addr;

	time_start = get_timer(0);

	printf("Save mem address 0x%x to flash address 0x%x\n", data_addr, addr);
	if (debug)
		printf("Saving..\n");

	index = count /FLASH_R_LENGTH;
	if (count % FLASH_R_LENGTH)
		index++;

	for (i = 0; i < index; i ++) {
		if ((i == index - 1) && (count % FLASH_R_LENGTH))
			len = count % FLASH_R_LENGTH;
		else
			len = FLASH_R_LENGTH;


		if (espi_put_flash_np(CYCLE_FLASH_WRITE, addr + i * FLASH_R_LENGTH, len, data_addr, tag))
			return STATUS_ERR;

		if (wait_alert())
			return STATUS_ERR;

		if (wait_for_flash_c_avail())
			return STATUS_ERR;

		if (espi_get_flash_c(0, 0))
			return STATUS_ERR;

		data_addr += len;
		if (debug) {
			putc('#');
			if ((i % FLASH_R_LENGTH) == 0 && i > 0)
				puts("\n\t ");
		}
	}
	time_start = get_timer(time_start);
	if (time_start > 0) {
		if (debug) {
			puts("\n\t ");	/* Line up with "Loading: " */
			print_size(count / time_start * 1000, "/s");
		}
	}

	if (debug) {
		printf("\ndone\n");
	}

	return STATUS_OK;
}

static int espi_flash_erase(u32 addr, u8 tag)
{
	printf("Erase address: 0x%x\n", addr);

	if (wait_for_flash_np_free())
		return STATUS_ERR;

	if (espi_put_flash_np(CYCLE_FLASH_ERASE, addr, 0, 0, tag))
		return STATUS_ERR;

	if (wait_alert())
		return STATUS_ERR;

	if (wait_for_flash_c_avail())
		return STATUS_ERR;

	if (espi_get_flash_c(0, 0))
		return STATUS_ERR;

	if (debug) {
		printf("\ndone\n");
	}

	return STATUS_OK;
}

static int espi_flash_rpmc_op1(u8 flash_dev, int count, u32 data_addr, u8 tag)
{
	printf("Get mem address 0x%x to flash device 0x%x\n", data_addr, flash_dev);

	if (espi_put_flash_np_rpmc((CYCLE_FLASH_RPMC_OP1 | (flash_dev << 5)), count, data_addr, tag))
		return STATUS_ERR;

	if (wait_alert())
		return STATUS_ERR;

	if (wait_for_flash_c_avail())
		return STATUS_ERR;

	if (espi_get_flash_c(0, 0))
		return STATUS_ERR;

	if (debug) {
		printf("\ndone\n");
	}

	return STATUS_OK;
}

static int espi_flash_rpmc_op2(u8 flash_dev, int count, u32 dest, u8 tag)
{
	printf("Read flash device 0x%x to 0x%x\n", flash_dev, dest);

	if (espi_put_flash_np_rpmc((CYCLE_FLASH_RPMC_OP2 | (flash_dev << 5)), count, 0, tag))
		return STATUS_ERR;

	if (wait_alert())
		return STATUS_ERR;

	if (wait_for_flash_c_avail())
		return STATUS_ERR;

	if (espi_get_flash_c(count, dest))
		return STATUS_ERR;

	if (debug) {
		printf("\ndone\n");
	}

	return STATUS_OK;
}

static int espi_caf_flash(u8 index, int count)
{
	u8 data[OOB_R_LENGTH] = {0};

	if (wait_for_flash_np_avail())
		return STATUS_ERR;

	if (espi_get_flash_np(index, count, data))
		return STATUS_ERR;

	/* 1: cycle_type, 2: tag_len_11_8, 3: len_7_0 */
	if (espi_put_flash_c(data[1], data[2], data[3]))
		return STATUS_ERR;

	printf("cycle_type: %d done\n", index);

	return STATUS_OK;
}

static int espi_get_configuration(u16 addr, u8 *in)
{
	struct get_configuration_req req;
	struct get_configuration_resp *resp = (struct get_configuration_resp *)in;
	u16 status;

	req.cmd = CMD_GET_CONFIGURATION;
	req.addr[0] = addr >> 8;
	req.addr[1] = addr & 0xFF;
	req.crc = crc8(0, (u8 *)&req, sizeof(req) - 1);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	status = espi_read_response((u8 *)resp, sizeof(*resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
			__func__, resp->code, status);
		resp->status = status;
		return STATUS_ERR;
	}
	printf("get_config(%02x)=0x%08x\n", addr, resp->data);
	//printf("status= %04x\n", resp->status);

	return STATUS_OK;
}

static int espi_set_configuration(u16 addr, u32 data, u8 *in)
{
	struct set_configuration_req req;
	struct espi_common_resp *resp = (struct espi_common_resp *)in;
	u16 status;

	printf("set_config(%02x)=0x%08x\n", addr, data);
	req.cmd = CMD_SET_CONFIGURATION;
	req.addr[0] = addr >> 8;
	req.addr[1] = addr & 0xFF;
	req.data[0] = data & 0xFF;
	req.data[1] = (data >> 8) & 0xFF;
	req.data[2] = (data >> 16) & 0xFF;
	req.data[3] = (data >> 24) & 0xFF;
	req.crc = crc8(0, (u8 *)&req, sizeof(req) - 1);

	if (addr == 0x8) {
		wait_state = ((data >> 8) & 0xF0) >> 4;
		if (wait_state ==0)
			wait_state = 16;
		//printf("wait_state=0x%x\n", wait_state);
	}

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	status = espi_read_response((u8 *)resp, sizeof(*resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);
	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
			__func__, resp->code, status);
		resp->status = status;
		return STATUS_ERR;
	}

	return STATUS_OK;
}

static int espi_get_vwire(u8 *in)
{
	struct get_vwire_req req;
	struct get_vwire_resp *resp = (struct get_vwire_resp *)in;
	u16 status;

	req.cmd = CMD_GET_VWIRE;
	req.crc = crc8(0, &req.cmd, 1);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	status = espi_read_response((u8 *)resp, sizeof(*resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
			__func__, resp->code, status);
		resp->status = status;
		return STATUS_ERR;
	} else {
		if (debug) {
			printf("get_vwire: ");
			printf("index(%d)=0x%02x\n", resp->index, resp->data);
			//printf("status=%04x\n", resp->status);
		}
	}
	return STATUS_OK;
}

static int espi_put_vwire(u8 index, u8 data, u8 *in)
{
	struct put_vwire_req req;
	struct espi_common_resp *resp = (struct espi_common_resp *)in;
	u16 status;

	if (debug)
		printf("put_vwire: index(%d)=0x%02x\n", index, data);
	req.cmd = CMD_PUT_VWIRE;
	req.len = 0;
	req.index = index;
	req.data = data;
	req.crc = crc8(0, (u8 *)&req, sizeof(req) - 1);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	status = espi_read_response((u8 *)resp, sizeof(*resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);
	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
			__func__, resp->code, status);
		resp->status = status;
		return STATUS_ERR;
	}
	return STATUS_OK;
}

static int espi_put_iowr(u16 addr, u8 *data, int count, u8 *in)
{
	struct put_iowr_req req;
	struct espi_common_resp *resp = (struct espi_common_resp *)in;
	u16 status;
	int i;

	if (debug) {
		printf("IO wr: addr(0x%x)=", addr);
		for (i = count - 1; i >= 0; i--)
			printf("%02x", data[i]);
		printf("\n");
	}

	req.cmd = CMD_PUT_IOWR_SHORT | ((count - 1) & 0x3);
	req.addr[0] = addr >> 8;
	req.addr[1] = addr & 0xFF;
	for (i = 0; i < count; i++)
		req.data_crc[i] = data[i];
	req.data_crc[i] = crc8(0, (u8 *)&req, count + 3);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, count + 4);
	espi_tar();
	status = espi_read_response((u8 *)resp, sizeof(*resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);
	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
			__func__, resp->code, status);
		resp->status = status;
		return STATUS_ERR;
	}
	return STATUS_OK;
}

static int espi_put_iord(u16 addr, int count, u8 *in)
{
	struct put_iord_req req;
	struct put_iord_resp *resp = (struct put_iord_resp *)in;
	u16 status;

	req.cmd = CMD_PUT_IORD_SHORT | ((count - 1) & 0x3);
	req.addr[0] = addr >> 8;
	req.addr[1] = addr & 0xFF;
	req.crc = crc8(0, &req.cmd, 3);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	status = espi_read_response((u8 *)resp, count + 4);
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
			__func__, resp->code, status);
		resp->status = status;
		return STATUS_ERR;
	}
	if (debug) {
		printf("IO rd: port[0x%x]=", addr);
		if (count == 1)
			printf("%02x\n", resp->data & 0xFF);
		else if (count == 2)
			printf("%04x\n", resp->data & 0xFFFF);
		else if (count == 4)
			printf("%08x\n", resp->data);
	}
	//printf("status=%02x%02x\n", resp.status[1], resp.status[0]);
	return STATUS_OK;
}

static int espi_get_pc(int len)
{
	struct get_pc_req req = {0};
	struct get_pc_resp resp = {0};
	u16 status;

	req.cmd = CMD_GET_PC;
	req.crc = crc8(0, (u8 *)&req, sizeof(req) - 1);

	debug("req.cmd 0x%x\n", req.cmd);
	debug("req.crc 0x%x\n", req.crc);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);
	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	status = espi_read_response((u8 *)&resp, sizeof(resp) - (4 - len));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (resp.cycle_type == 0xf) {
		debug("resp.code 0x%x\n", resp.code);
		debug("resp.cycle_type 0x%x\n", resp.cycle_type);
		debug("resp.tag_len_11_8 0x%x\n", resp.tag_len_11_8);
		debug("resp.len_7_0 0x%x\n", resp.len_7_0);
		for (int i=0; i<len; i++)
			printf("resp.data%d 0x%x\n", i, resp.data[i]);
		debug("resp.status %x\n", resp.data[len] | (resp.data[len + 1] << 8));
		debug("resp.crc %x\n", resp.data[len + 2]);
	} else if (resp.cycle_type != 6){
		printf("pc cycle type error (type=%d)\n", resp.cycle_type);
		return STATUS_ERR;
	}

	if (status) {
		printf("espi response error: code=0x%02x, status=0x%04x\n", resp.code, status);
		return STATUS_ERR;
	}

	return STATUS_OK;
}

static int wait_for_pc_avail(void)
{
	u16 status;
	int count = 1000000;

	do {
		status = espi_get_status();
		if (status & BIT(4)) {
			debug("PC_AVAIL  count %d\n", count);
			return 0;
		}

		if (count-- == 0) {
			printf("\nwait_for_pc_avail timeout\n");
			return -ETIMEDOUT;
		}
		//udelay(100);
		udelay(1);
	} while (1);

	return -ETIMEDOUT;
}

/**
 * espi_wait_and_get_pc() - Wait for ALERT#, poll until PC_AVAIL, then fetch
 *                          the Peripheral Channel completion via GET_PC.
 *
 * This encapsulates the standard post-DEFER flow on the Peripheral Channel:
 * the slave signals readiness via ALERT# (or IO1 in single-IO mode), the host
 * polls the status register until PC_AVAIL is set, then issues GET_PC to
 * retrieve the completion payload.
 *
 * @count: data length in bytes (1, 2, or 4)
 * Return: STATUS_OK on success, STATUS_ERR on any step failure.
 */
static int espi_wait_and_get_pc(int count)
{
	if (wait_alert())
		return STATUS_ERR;

	if (wait_for_pc_avail())
		return STATUS_ERR;

	if (espi_get_pc(count))
		return STATUS_ERR;

	return STATUS_OK;
}
static int espi_put_memwr32(u32 addr, u8 *data, int count, u8 *in)
{
	struct put_memwr32_req req;
	struct espi_common_resp *resp = (struct espi_common_resp *)in;
	u16 status;
	int i;

	if (debug) {
		printf("MEM wr32: addr[0x%x]=", addr);
		for (i = count - 1; i >= 0; i--)
			printf("%02x", data[i]);
		printf("\n");
	}

	req.cmd = CMD_PUT_MEMWR32_SHORT_1B | ((count - 1) & 0x3);
	req.addr[0]= (addr & 0xFF000000) >> 24;
	req.addr[1] = (addr & 0x00FF0000) >> 16;
	req.addr[2] = (addr & 0x0000FF00) >> 8;
	req.addr[3] = addr & 0xff;
	for (i = 0; i < count; i++)
		req.data_crc[i] = data[i];
	req.data_crc[i] = crc8(0, (u8 *)&req, sizeof(req) - (4 - count) - 1);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, count + 6);
	espi_tar();
	status = espi_read_response((u8 *)resp, sizeof(*resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);
	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
		       __func__, resp->code, status);
		resp->status = status;
		return STATUS_ERR;
	}

	if (wait_alert())
		return STATUS_ERR;

	if (debug) {
		status = espi_get_status();
		printf("%s: memwr32 status=0x%04x\n", __func__, status);
	}

	return STATUS_OK;
}

static int espi_put_memrd32(u32 addr, int count, u8 *in)
{
	struct put_memrd32_req req;
	struct put_memrd32_resp *resp = (struct put_memrd32_resp *)in;
	u16 status;

	req.cmd = CMD_PUT_MEMRD32_SHORT_1B | ((count - 1) & 0x3);
	req.addr[0] = (addr & 0xFF000000) >> 24;
	req.addr[1] = (addr & 0x00FF0000) >> 16;
	req.addr[2] = (addr & 0x0000FF00) >> 8;
	req.addr[3] = addr & 0xff;
	req.crc = crc8(0, (u8 *)&req, sizeof(req) - 1);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	status = espi_read_response((u8 *)resp, count + 4);
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (debug) {
		printf("MEM rd32: addr[0x%x]=", addr);
		if (resp->code != RESP_DEFER) {
			if (count == 1)
				printf("%02x\n", resp->data & 0xFF);
			else if (count == 2)
				printf("%04x\n", resp->data & 0xFFFF);
			else if (count == 4)
				printf("%08x\n", resp->data);
		}
	}

	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
		       __func__, resp->code, status);
		resp->status = status;
		return STATUS_ERR;
	}

	if (resp->code == RESP_DEFER) {
		printf("MEM rd32 DEFER\n");
		if (espi_wait_and_get_pc(count))
			return STATUS_ERR;
	}

	return STATUS_OK;
}

static int espi_put_oob(void)
{
	struct put_oob_req req = {0};
	struct espi_common_resp resp = {0};
	u16 status;

	req.cmd = CMD_PUT_OOB;
	req.cycle_type = 0x21; /* OOB (Tunneled SMBus) Message */
	req.tag_len_11_8 = 0x00;
	req.len_7_0 = 0x05; /* Response length for PCH get Temperature */

	/* Response data for PCH get Temperature */
	req.data[0] = 0x0e; /* (Dst Slave Addr << 1) | 0 */
	req.data[1] = 0x01; /* Command Code */
	req.data[2] = 0x02; /* Byte Count */
	req.data[3] = 0x03; /* (Src Slave Addr << 1) | 1 */
	req.data[4] = 0x2f; /* Temperature Data */
	req.data[5] = crc8(0, (u8 *)&req, sizeof(req) + 5 - OOB_R_LENGTH - 1);

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);

	espi_send((u8 *)&req, sizeof(req) + 5 - OOB_R_LENGTH);
	espi_tar();
	status = espi_read_response((u8 *)&resp, sizeof(resp));
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (debug) {
		printf("resp.code 0x%x\n", resp.code);
		printf("resp.status 0x%x\n", resp.status);
	}

	if (status) {
		printf("\n  espi_put_oob response error: code=0x%02x, status=0x%04x\n", resp.code, status);
		return STATUS_ERR;
	}

	return STATUS_OK;
}

static int espi_get_oob(int count)
{
	struct get_oob_req req = {0};
	struct get_oob_resp resp = {0};
	u16 status;

	req.cmd = CMD_GET_OOB;
	req.crc = crc8(0, (u8 *)&req, sizeof(req) - 1);

	if (debug) {
		printf("req.cmd 0x%x\n", req.cmd);
		printf("req.crc 0x%x\n", req.crc);
	}

	//CS_low, CLK_low, IO0
	espi_port_update(eSPI_IO0, eSPI_nCS | eSPI_CLK);
	espi_send((u8 *)&req, sizeof(req));
	espi_tar();
	status = espi_read_response((u8 *)&resp, sizeof(resp) + count - OOB_R_LENGTH);
	espi_restore_io_output();
	espi_port_set(eSPI_nCS);

	if (debug) {
		printf("resp.cycle_type 0x%x\n", resp.cycle_type);
		printf("resp.tag_len_11_8 0x%x\n", resp.tag_len_11_8);
		printf("resp.len_7_0 0x%x\n", resp.len_7_0);
	}

	if (status) {
		printf("%s: espi response error: code=0x%02x, status=0x%04x\n",
				__func__, resp.code, status);
		return STATUS_ERR;
	}

	return STATUS_OK;
}

static int wait_for_oob_avail(void)
{
	u16 status;
	int count = 1000000;

	do {
		status = espi_get_status();
		if (status & BIT(7)) {
			debug("OOB_AVAIL  count %d\n", count);
			return 0;
		}

		if (count-- == 0) {
			printf("\nwait_for_oob_avail timeout\n");
			return -ETIMEDOUT;
		}
		//udelay(100);
		udelay(1);
	} while (1);

	return -ETIMEDOUT;
}

static int do_oob_test(int count)
{
	if (wait_for_oob_avail())
		return STATUS_ERR;

	if (espi_get_oob(count))
		return STATUS_ERR;

	if (espi_put_oob())
		return STATUS_ERR;

	return STATUS_OK;
}

static int wait_for_vw_avail(void)
{
	u16 status;
	int count = 1000000;

	do {
		status = espi_get_status();
		if (status & BIT(6)) {
			debug("VW_AVAIL  count %d\n", count);
			return 0;
		}

		if (count-- == 0) {
			printf("\nwait_for_vw_avail timeout\n");
			return -ETIMEDOUT;
		}
		//udelay(100);
		udelay(1);
	} while (1);

	return -ETIMEDOUT;
}

static int espi_iowr8(u16 addr, u8 data)
{
	u8 resp[MAX_RESP_LEN];

	return espi_put_iowr(addr, &data, 1, resp);
}

static int espi_iowr16(u16 addr, u16 data)
{
	u8 resp[MAX_RESP_LEN];

	return espi_put_iowr(addr, (u8 *)&data, 2, resp);
}
static int espi_iowr32(u16 addr, u32 data)
{
	u8 resp[MAX_RESP_LEN];

	return espi_put_iowr(addr, (u8 *)&data, 2, resp);
}
static int espi_iord8(u16 addr, u8 *data)
{
	struct put_iord_resp resp;
	int ret;

	ret = espi_put_iord(addr, 1, (u8 *)&resp);
	*data = (u8)(resp.data & 0xFF);

	return ret;
}
static int espi_iord16(u16 addr, u16 *data)
{
	struct put_iord_resp resp;
	int ret;

	ret = espi_put_iord(addr, 2, (u8 *)&resp);
	*data = (u16)(resp.data & 0xFFFF);

	return ret;
}
static int espi_iord32(u16 addr, u32 *data)
{
	struct put_iord_resp resp;
	int ret;

	ret = espi_put_iord(addr, 4, (u8 *)&resp);
	*data = resp.data;

	return ret;
}

static int do_pc_test(void)
{
	u8 resp[MAX_RESP_LEN];
	u32 data;
	bool test_result = false;
	int count;

	data = 0xCAABCA8B;
	espi_put_memwr32(0x10000000, (u8 *)&data, 4, resp);
	count = 10;
	while (count-- > 0) {
		espi_put_memrd32(0x10000004, 4, resp);
		if (resp[1] == 0x4d && resp[2] == 0x3c && resp[3] == 0x2b && resp[4] == 0x1a) {
			test_result = true;
			break;
		}
		mdelay(1);
	}
	espi_put_memrd32(0x10000004, 4, resp);

	return test_result ? STATUS_OK : STATUS_ERR;
}

static int do_espi_auto_test(void)
{
	u8 resp[MAX_RESP_LEN];
	struct get_configuration_resp *config_resp = (struct get_configuration_resp *)&resp[0];
	int count = 10;
	struct put_iord_resp resp_iord;
	bool test_result = false;
	u8 crc1, crc2;
	u8 pass_cnt = 0; // should be 9 if all tests passed

	espi_set_configuration(0x20, 0x00000001, resp);
	espi_get_configuration(0x20, resp);
	if (config_resp->status & STATUS_VW_AVAIL)
		espi_get_vwire(resp);

	/* wait VW channle ready */
	while (count-- > 0) {
		espi_get_configuration(0x20, resp);
		if (config_resp->data & BIT(1)) {
			printf("VW channel ready\n");
			break;
		}
		mdelay(1);
	}
	/* deassert PLTRST# */
	espi_put_vwire(3, 0x22, resp);
	if (config_resp->status & STATUS_VW_AVAIL)
		espi_get_vwire(resp);

	espi_set_configuration(0x10, 0x00001111, resp);
	/* wait peripheral channle ready */
	count = 10;
	while (count-- > 0) {
		espi_get_configuration(0x10, resp);
		if (config_resp->data & BIT(1)) {
			printf("Peripheral channel ready\n");
			break;
		}
		mdelay(1);
	}

	espi_set_configuration(0x30, 0x111, resp);
	/* wait oob channel ready */
	count = 10;
	while (count-- > 0) {
		espi_get_configuration(0x30, resp);
		if (config_resp->data & BIT(1)) {
			printf("oob channel ready\n");
			break;
		}
		mdelay(1);
	}

	/* bit 11: Target attached flash sharing */
	espi_set_configuration(0x40, 0x00031925, resp);
	/* wait flash channel ready */
	count = 10;
	while (count-- > 0) {
		espi_get_configuration(0x40, resp);
		if (config_resp->data & BIT(1)) {
			printf("flash channel TAF ready\n");
			break;
		}
		mdelay(1);
	}

	if (config_resp->status & BIT(6)) {
		espi_get_vwire(resp);
	}

	test_result = true;
	/* Peripheral Channel */
	printf("=================================================\n");
	printf("peripheral: put_iord_short + put_iowr_short start\n");

	printf("configure PMC1(IO base=0x%04x)\n", KCS_DATA_REG);
	espi_iowr8(SIO_IDX, 7);
	espi_iowr8(SIO_DATA, 0x11);
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 0x0);
	espi_iowr8(SIO_IDX, 0x60);
	espi_iowr8(SIO_DATA, (KCS_DATA_REG >> 8) & 0xFF);
	espi_iowr8(SIO_IDX, 0x61);
	espi_iowr8(SIO_DATA, KCS_DATA_REG & 0xFF);
	espi_iowr8(SIO_IDX, 0x62);
	espi_iowr8(SIO_DATA, (KCS_CMD_REG >> 8) & 0xFF);
	espi_iowr8(SIO_IDX, 0x63);
	espi_iowr8(SIO_DATA, KCS_CMD_REG & 0xFF);
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 1);
	if (espi_put_iord(SIO_DATA, 1, (u8 *)&resp_iord))
		test_result = false;
	if ((u8)(resp_iord.data & 0xFF) != 0x01)
		test_result = false;

	printf("configure SHM(WIN_BASE1=0x10000000)\n");
	espi_iowr8(SIO_IDX, 0x7);
	espi_iowr8(SIO_DATA, 0xF);
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 0x0);
	espi_iowr8(SIO_IDX, 0xF4);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF5);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF6);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF7);
	espi_iowr8(SIO_DATA, 0x10);
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 0x1);
	if (espi_put_iord(SIO_DATA, 1, (u8 *)&resp_iord))
		test_result = false;
	if ((u8)(resp_iord.data & 0xFF) != 0x01)
		test_result = false;

	if (test_result)
		pass_cnt++;
	else
		goto fail_exit;

	printf("peripheral: put_iord_short + put_iowr_short: %s\n", test_result ? "Pass" : "Fail");
	printf("=================================================\n");

	test_result = false;
	/* Peripheral Channel */
	printf("=======================================================\n");
	printf("peripheral: put_memrd32_short + put_memwr32_short start\n");
	if (!do_pc_test())
		test_result = true;

	if (test_result)
		pass_cnt++;
	else
		goto fail_exit;

	printf("peripheral: put_memrd32_short + put_memwr32_short: %s\n", test_result ? "Pass" : "Fail");
	printf("=======================================================\n");

	test_result = true;
	/* VW */
	printf("=============\n");
	printf("vw: IRQ start\n");

	/*
	 * SLV_BOOT_DONE scenario: after VW channel is enabled the target will
	 * push a VW packet on its own initiative (no master command).  In U-Boot
	 * there is no interrupt handler, so we must poll for the alert / status.
	 *
	 * Two equivalent paths:
	 *
	 * Path A – alert-driven dispatch (preferred when ALERT#/IO1 is wired):
	 *   wait_alert_and_dispatch() latches the GPIO edge, then calls
	 *   GET_STATUS once to find out which channel(s) need service.
	 *   Use this path to avoid sending repeated GET_STATUS transactions
	 *   while the bus is otherwise idle.
	 *
	 * Path B – direct GET_STATUS polling (always safe, used here as
	 *   fallback for IO1-alert in dual/quad mode):
	 *   wait_for_vw_avail() spins on GET_STATUS until VW_AVAIL (bit 6)
	 *   is set.  Other wait_for_xxx_avail() helpers work the same way.
	 *
	 * In this autotest we use Path B (wait_for_vw_avail) because the test
	 * must also work in dual/quad + IO1-alert mode where the edge event
	 * cannot fire.  To exercise Path A, call wait_alert_and_dispatch()
	 * and check the returned status word.
	 */
	if (wait_for_vw_avail()) {
		test_result = false;
	} else {
		count = 10;
		while (count > 0) {
			espi_get_vwire(resp);
			if (resp[2] == 0x0 && resp[3] == 0x81)
				break;
			mdelay(500);
			count--;
		}

		if (count == 0)
			test_result = false;

		count = 10;
		while (count > 0) {
			espi_put_iord(KCS_STATUS_REG, 1, (u8 *)&resp_iord);
			if ((u8)(resp_iord.data & 0x01) == 0x1)
				break;
			mdelay(1);
			count--;
		}

		if (count == 0)
			test_result = false;

		espi_put_iord(KCS_DATA_REG, 1, (u8 *)&resp_iord);

		count = 10;
		while (count > 0) {
			espi_get_vwire(resp);
			if (resp[2] == 0x0 && resp[3] == 0x01)
				break;
			mdelay(500);
			count--;
		}

		if (count == 0)
			test_result = false;
	}

	if (test_result)
		pass_cnt++;
	else
		goto fail_exit;

	printf("vw: IRQ: %s\n", test_result ? "Pass" : "Fail");
	printf("=============\n");

	test_result = true;
	/* VW */
	printf("===============================\n");
	printf("vw: get_vwire + put_vwire start\n");

	/* assert HOST_RST_WARN# */
	espi_put_vwire(7, 0x11, resp);
	if (wait_for_vw_avail()) {
		test_result = false;
	} else {
		count = 10;
		while (count > 0) {
			espi_get_vwire(resp);
			if (resp[2] == 0x6 && ((resp[3] & 0x88) == 0x88))
				break;
			mdelay(1);
			count--;
		}

		if (count == 0)
			test_result = false;
	}

	/* deassert HOST_RST_WARN# */
	espi_put_vwire(7, 0x10, resp);
	if (wait_for_vw_avail()) {
		test_result = false;
	} else {
		count = 10;
		while (count > 0) {
			espi_get_vwire(resp);
			if (resp[2] == 0x6 && ((resp[3] & 0x88) == 0x80))
				break;
			mdelay(1);
			count--;
		}

		if (count == 0)
			test_result = false;
	}

	if (test_result)
		pass_cnt++;
	else
		goto fail_exit;

	printf("vw: get_vwire + put_vwire: %s\n", test_result ? "Pass" : "Fail");
	printf("===============================\n");

	test_result = false;
	/* OOB */
	printf("============================\n");
	printf("oob: get_oob + put_oob start\n");

	if (!do_oob_test(4)) // Request length for PCH get Temperature
		test_result = true;

	if (test_result)
		pass_cnt++;
	else
		goto fail_exit;

	printf("oob: get_oob + put_oob: %s\n", test_result ? "Pass" : "Fail");
	printf("============================\n");

	mdelay(2000);

	test_result = true;
	/* Flash */
	printf("==================================\n");
	printf("flash: TAFS read/write/erase start\n");

	espi_flash_read(0x10000, 0x40, 0x10000000, 0x0);
	crc1 = crc8(0, (const unsigned char *)(uintptr_t)0x10000000, 0x40);
	printf("bf erase crc: 0x%x\n", crc1);
	espi_flash_erase(0x10000, 0x0);
	espi_flash_read(0x10000, 0x40, 0x10000000, 0x0);
	crc2 = crc8(0, (const unsigned char *)(uintptr_t)0x10000000, 0x40);
	printf("af erase crc: 0x%x\n", crc2);
	if (crc1 == crc2)
		test_result = false;

	espi_flash_write(0x10000, 0x40, 0x10010000, 0x0);
	crc1 = crc8(0, (const unsigned char *)(uintptr_t)0x10010000, 0x40);
	printf("bf write crc: 0x%x\n", crc1);
	espi_flash_read(0x10000, 0x40, 0x10000000, 0x0);
	crc2 = crc8(0, (const unsigned char *)(uintptr_t)0x10000000, 0x40);
	printf("af write crc: 0x%x\n", crc2);
	if (crc1 != crc2)
		test_result = false;

	if (test_result)
		pass_cnt++;
	else
		goto fail_exit;

	printf("flash: TAFS read/write/erase: %s\n", test_result ? "Pass" : "Fail");
	printf("==================================\n");

	test_result = true;
	printf("===============================\n");
	printf("flash: TAFS tag overwrite start\n");
	// should consider the test for tag overwrite
	if (!espi_flash_read(0x0, 0x40, 0x10000000, 0x1))
		test_result = false;
	if (!espi_flash_erase(0x0, 0x1))
		test_result = false;
	if (!espi_flash_write(0x0, 0x40, 0x10010000, 0x1))
		test_result = false;

	if (test_result)
		pass_cnt++;
	else
		goto fail_exit;

	printf("flash: TAFS tag overwrite: %s\n", test_result ? "Pass" : "Fail");
	printf("===============================\n");

	test_result = true;
	printf("=================================\n");
	printf("flash: TAFS RPMC operations start\n");
	if (espi_flash_rpmc_op1(0x0, 0x40, 0x10000000, 0x0))
		test_result = false;
	if (espi_flash_rpmc_op1(0x1, 0x40, 0x10000000, 0x0))
		test_result = false;
	if (espi_flash_rpmc_op1(0x2, 0x40, 0x10000000, 0x0))
		test_result = false;
	if (espi_flash_rpmc_op1(0x3, 0x40, 0x10000000, 0x0))
		test_result = false;
	if (espi_flash_rpmc_op2(0x0, 0x40, 0x10000000, 0x0))
		test_result = false;
	if (espi_flash_rpmc_op2(0x1, 0x40, 0x10000000, 0x0))
		test_result = false;
	if (espi_flash_rpmc_op2(0x2, 0x40, 0x10000000, 0x0))
		test_result = false;
	if (espi_flash_rpmc_op2(0x3, 0x40, 0x10000000, 0x0))
		test_result = false;

	if (test_result)
		pass_cnt++;
	else
		goto fail_exit;

	printf("flash: TAFS RPMC operations: %s\n", test_result ? "Pass" : "Fail");
	printf("=================================\n");

	test_result = true;
	/* Flash */
	printf("=================\n");
	printf("flash: CAFS start\n");

	/* bit 11: Controller attached flash sharing */
	espi_set_configuration(0x40, 0x00031125, resp);
	/* wait flash channel ready */
	count = 10;
	while (count-- > 0) {
		espi_get_configuration(0x40, resp);
		if (config_resp->data & BIT(1)) {
			printf("flash channel CAF ready\n");
			break;
		}
		mdelay(1);
	}

	if (espi_caf_flash(CYCLE_FLASH_READ, 0x40))
		test_result = false;
	if (espi_caf_flash(CYCLE_FLASH_WRITE, 0x40))
		test_result = false;
	if (espi_caf_flash(CYCLE_FLASH_ERASE, 0x0))
		test_result = false;

	if (test_result)
		pass_cnt++;
	else
		goto fail_exit;

	printf("flash: CAFS: %s\n", test_result ? "Pass" : "Fail");
	printf("=================\n");

fail_exit:
	if (pass_cnt == 9)
		printf("All tests passed\n");
	else
		printf("%d/9 tests passed\n", pass_cnt);

	return 0;
}

static u8 kcs_read_status(void)
{
	struct put_iord_resp resp;
	//u8 cmd = KCS_CMD_GET_STATUS;

	//espi_put_iowr(KCS_CMD_REG, &cmd, 1, (u8 *)&resp);
	espi_put_iord(KCS_STATUS_REG, 1, (u8 *)&resp);
	return (resp.data & 0xFF);
}

/* wait for IBF = 0, means host can write data to BMC */
static int kcs_wait_ibf(u8 *status)
{
	*status = 0xC0; //STATE_ERR
	int count = 100;
	u8 data;

	while (count--) {
		data = kcs_read_status();
		if ((data & KCS_STATUS_IBF) == 0) {
			*status = data;
			return 0;
		}
		mdelay(1);
	}

	return -ETIMEDOUT;
}

/* wait for OBF = 1, means host can read data from BMC */
static int kcs_wait_obf(u8 *status)
{
	*status = 0xC0; //STATE_ERR
	int count = 10000;
	u8 data;

	while (count--) {
		data = kcs_read_status();
		if (data & KCS_STATUS_OBF) {
			*status = data;
			return 0;
		}
		mdelay(1);
	}

	return -ETIMEDOUT;
}
/* clear OBF by read data register */
static inline void kcs_clear_obf(void)
{
	struct put_iord_resp resp;

	espi_put_iord(KCS_DATA_REG, 1, (u8 *)&resp);
}

static inline void kcs_write_cmd(u8 cmd)
{
	u8 resp[MAX_RESP_LEN];

	espi_put_iowr(KCS_CMD_REG, &cmd, 1, resp);
}

static void kcs_write_byte(u8 data)
{
	u8 resp[MAX_RESP_LEN];

	espi_put_iowr(KCS_DATA_REG, &data, 1, resp);
}

static u8 kcs_read_byte(void)
{
	struct put_iord_resp resp;

	espi_put_iord(KCS_DATA_REG, 1, (u8 *)&resp);
	return (u8)(resp.data & 0xFF);
}

/*
 * host to BMC
 * 1. write WRITE_START to CMD register
 * 2. write data to DATA register
 * 3. write WRITE_END to CMD register before send last byte
 * 4. write last byte to DATA register
 */
static int kcs_transfer(u8 *req, int req_len, u8 *resp, int resp_len)
{
	u8 status;
	int read_len = 0;
	int i;

	/* WRITE state */
	if (kcs_wait_ibf(&status)) {
		printf("IBF is not 0\n");
		return -EIO;
	}
	kcs_clear_obf();
	kcs_write_cmd(KCS_CMD_WRITE_START);
	kcs_wait_ibf(&status);
	if (!KCS_STATE_WRITE(status)) {
		printf("WRITE_START error\n");
		goto err_exit;
	}
	kcs_clear_obf();
	for (i = 0; i < req_len - 1; i++) {
		kcs_write_byte(req[i]);
		kcs_wait_ibf(&status);
		if (!KCS_STATE_WRITE(status)) {
			printf("write request error, status=%02x\n", status);
			goto err_exit;
		}
		kcs_clear_obf();
	}
	kcs_write_cmd(KCS_CMD_WRITE_END);
	kcs_wait_ibf(&status);
	if (!KCS_STATE_WRITE(status)) {
		printf("WRITE_END error\n");
		goto err_exit;
	}
	kcs_clear_obf();
	kcs_write_byte(req[req_len - 1]);

	/* READ state */
	for (i = 0; i < resp_len; i++) {
		kcs_wait_ibf(&status);
		if (!KCS_STATE_READ(status) && !KCS_STATE_IDLE(status)) {
			printf("READ state error\n");
			goto err_exit;
		}
		if (KCS_STATE_IDLE(status)) {
			kcs_wait_obf(&status);
			kcs_clear_obf();
			break;
		}
		kcs_wait_obf(&status);
		if (KCS_STATE_ERR(status)) {
			printf("read data error\n");
			break;
		}
		*resp++ = kcs_read_byte();
		if (++read_len < resp_len)
			kcs_write_byte(KCS_CMD_READ);
	}
	return read_len;

err_exit:
	kcs_wait_ibf(&status);
	kcs_write_cmd(KCS_CMD_ABORT);
	kcs_wait_ibf(&status);
	kcs_clear_obf();
	return -EIO;

}

static int do_kcs_test(void)
{
	u8 req[2];
	u8 resp[32];
	int read_len;
	int i;
	bool debug_save = debug;

	/* get device id */
	req[0] = 0x18;
	req[1] = 1;
	debug = false;
	read_len = kcs_transfer(req, 2, resp, 18);
	debug = debug_save;

	if (read_len < 0) {
		printf("kcs transfer err\n");
		return 0;
	}
	for (i = 0; i < read_len; i++)
		printf("%02x ", resp[i]);
	printf("\n");

	return 0;
}

static int do_espi_master_start(void)
{
	u8 resp[MAX_RESP_LEN];
	//struct espi_common_resp *common_resp = (struct espi_common_resp *)&resp[0];
	struct get_configuration_resp *config_resp = (struct get_configuration_resp *)&resp[0];
	int count = 10;
	//struct put_iord_resp *iord_resp = (struct put_iord_resp *)&resp[0];
	//struct get_vwire_resp *vwire_resp = (struct get_vwire_resp *)&resp[0];
	//u32 data;
	//int ret;
#if 1
	espi_set_configuration(0x20, 0x00000001, resp);
	espi_get_configuration(0x20, resp);
	if (config_resp->status & STATUS_VW_AVAIL)
		espi_get_vwire(resp);

	/* wait VW channle ready */
	while (count-- > 0) {
		espi_get_configuration(0x20, resp);
		if (config_resp->data & BIT(1)) {
			printf("VW channel ready\n");
			break;
		}
		mdelay(1);
	}
	/* deassert PLTRST# & SUS_STAT# */
	espi_put_vwire(3, 0x33, resp);
	if (config_resp->status & STATUS_VW_AVAIL)
		espi_get_vwire(resp);

	espi_set_configuration(0x10, 0x00001113, resp);
	/* wait peripheral channle ready */
	count = 10;
	while (count-- > 0) {
		espi_get_configuration(0x10, resp);
		if (config_resp->data & BIT(1)) {
			printf("Peripheral channel ready\n");
			break;
		}
		mdelay(1);
	}
#endif

	espi_set_configuration(0x30, 0x111, resp);
	/* wait oob channel ready */
	count = 10;
	while (count-- > 0) {
		espi_get_configuration(0x30, resp);
		if (config_resp->data & BIT(1)) {
			printf("oob channel ready\n");
			break;
		}
		mdelay(1);
	}

	espi_set_configuration(0x40, 0x00031925, resp);
	count = 10;
	while (count-- > 0) {
		espi_get_configuration(0x40, resp);
		if (config_resp->data & BIT(1)) {
			printf("flash channel ready\n");
			break;
		}
		mdelay(1);
	}

#if 1
	printf("configure SP1\n");
	espi_iowr8(SIO_IDX, 7);
	espi_iowr8(SIO_DATA, 3);
	// disable LDN
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 0);
	// set io addr 3f8
	espi_iowr8(SIO_IDX, 0x60);
	espi_iowr8(SIO_DATA, 3);
	espi_iowr8(SIO_IDX, 0x61);
	espi_iowr8(SIO_DATA, 0xf8);
	// interrupt
	espi_iowr8(SIO_IDX, 0x70);
	espi_iowr8(SIO_DATA, 3);
	// enable LDN
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 1);
	// enable access to control register
	//espi_iowr8(SIO_IDX, 0xf0);
	//espi_iowr8(SIO_DATA, 0xba);
	// select bank1, divisor=1, baud=f_input/(16*divisor)
	espi_iowr8(0x3fb, 0x80);
	espi_iowr8(0x3f8, 1);
	espi_iowr8(0x3f9, 0);
	// select bank0, LCR=3(8 bit/1 stop/no parity)
	espi_iowr8(0x3fb, 0x3);
	espi_iowr8(0x3fc, 0);
	espi_iowr8(0x3f9, 3);
	espi_iowr8(0x3fa, 6);

	printf("configure KCS1(IO base=0x%04x)\n", KCS_DATA_REG);
	espi_iowr8(SIO_IDX, 7);
	espi_iowr8(SIO_DATA, 0x11);
	// set io addr 0ca2/0ca3
	espi_iowr8(SIO_IDX, 0x60);
	espi_iowr8(SIO_DATA, (KCS_DATA_REG >> 8) & 0xFF);
	espi_iowr8(SIO_IDX, 0x61);
	espi_iowr8(SIO_DATA, KCS_DATA_REG & 0xFF);
	espi_iowr8(SIO_IDX, 0x62);
	espi_iowr8(SIO_DATA, (KCS_CMD_REG >> 8) & 0xFF);
	espi_iowr8(SIO_IDX, 0x63);
	espi_iowr8(SIO_DATA, KCS_CMD_REG & 0xFF);
	// enable LDN
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 1);

	printf("configure SHM\n");
	// configure SHM
	espi_iowr8(SIO_IDX, 0x7);
	espi_iowr8(SIO_DATA, 0xF);
	// disable LDN
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 0x0);
	// set io addr 700
	espi_iowr8(SIO_IDX, 0x60);
	espi_iowr8(SIO_DATA, 0x7);
	espi_iowr8(SIO_IDX, 0x61);
	espi_iowr8(SIO_DATA, 0x00);
	// Interrupt Number
	espi_iowr8(SIO_IDX, 0x70);
	espi_iowr8(SIO_DATA, 0xF);
	// SHAW1BA_0~3
	espi_iowr8(SIO_IDX, 0xF4);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF5);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF6);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF7);
	espi_iowr8(SIO_DATA, 0x10);
	// SHAW2BA_0~3
	espi_iowr8(SIO_IDX, 0xF8);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF9);
	espi_iowr8(SIO_DATA, 0x10);
	espi_iowr8(SIO_IDX, 0xFA);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xFB);
	espi_iowr8(SIO_DATA, 0x10);
	// enable LDN
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 0x1);
	// set HOFS1L/HOFS1H
	espi_iowr8(0x726, 0x00);
	espi_iowr8(0x727, 0x08);

	printf("configure ESHM\n");
	// configure ESHM
	espi_iowr8(SIO_IDX, 0x7);
	espi_iowr8(SIO_DATA, 0x1D);
	// disable LDN
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 0x0);
	// SHAW3BA_0~3
	espi_iowr8(SIO_IDX, 0xF4);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF5);
	espi_iowr8(SIO_DATA, 0x20);
	espi_iowr8(SIO_IDX, 0xF6);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF7);
	espi_iowr8(SIO_DATA, 0x10);
	// SHAW4BA_0~3
	espi_iowr8(SIO_IDX, 0xF8);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xF9);
	espi_iowr8(SIO_DATA, 0x30);
	espi_iowr8(SIO_IDX, 0xFA);
	espi_iowr8(SIO_DATA, 0x00);
	espi_iowr8(SIO_IDX, 0xFB);
	espi_iowr8(SIO_DATA, 0x10);
	// enable LDN
	espi_iowr8(SIO_IDX, 0x30);
	espi_iowr8(SIO_DATA, 0x1);
#endif
#if 0
	// configure sp2
	espi_iowr8(0x4e, 7);
	espi_iowr8(0x4f, 2);
	//espi_iord8(0x4f, (u8 *)&data);
	// enable access to control register
	espi_iowr8(0x4e, 0xf0);
	espi_iowr8(0x4f, 0xba);
	espi_iowr8(0x4e, 0x30);
	espi_iowr8(0x4f, 1);
	// select bank1, divisor=13, baud=24000000/(16*13)=115200
	espi_iowr8(0x2fb, 0x80);
	espi_iowr8(0x2f8, 0xd);
	espi_iowr8(0x2f9, 0);
	// select bank0, LCR=3(8 bit/1 stop/no parity)
	espi_iowr8(0x2fb, 0x3);
#endif
	return 0;
}

/*
 * Negotiate I/O mode with the slave via General Capabilities register (addr 0x08).
 *
 * eSPI spec General Capabilities register (0x08):
 *   bits[25:24] IO Mode Support (slave reports what it can do):
 *     00 = Single only
 *     01 = Single and Dual
 *     10 = Single and Quad
 *     11 = Single, Dual and Quad
 *   bits[27:26] IO Mode Select (master writes desired mode):
 *     00 = Single
 *     01 = Dual
 *     10 = Quad
 *
 * This function reads the slave capability, selects the highest common mode
 * (or the requested mode if specified), issues SET_CONFIGURATION, then calls
 * espi_set_io_mode() to switch GPIO directions accordingly.
 */
static int espi_negotiate_io_mode(u8 requested_mode)
{
	u8 resp[16];
	struct get_configuration_resp *config_resp = (struct get_configuration_resp *)resp;
	u32 cap, new_cap;
	u8 slave_support, selected;
	u8 saved_mode = io_mode;

	/*
	 * The GET_CONFIGURATION and SET_CONFIGURATION packets that perform the
	 * negotiation must always be sent in Single mode, regardless of the
	 * current io_mode state. If io_mode is already non-single (e.g. a
	 * previous failed negotiation), we temporarily revert to single so
	 * the slave can understand the packets.
	 */
	if (io_mode != ESPI_IO_MODE_SINGLE) {
		printf("espi: temporarily reverting to Single for negotiation\n");
		espi_set_io_mode(ESPI_IO_MODE_SINGLE);
	}

	/* Read General Capabilities */
	if (espi_get_configuration(0x08, resp)) {
		espi_set_io_mode(saved_mode);
		return STATUS_ERR;
	}

	cap = config_resp->data;
	/* IO Mode Support: register 0x08 bits[25:24] */
	slave_support = (cap >> 24) & 0x3;
	//printf("espi: slave IO mode support bits[25:24]=0x%x\n", slave_support);

	/* Validate requested mode against slave capability */
	if (requested_mode == ESPI_IO_MODE_QUAD) {
		if (slave_support == 0x2 || slave_support == 0x3)
			selected = ESPI_IO_MODE_QUAD;
		else {
			printf("espi: slave does not support Quad, falling back\n");
			if (slave_support == 0x1 || slave_support == 0x3)
				selected = ESPI_IO_MODE_DUAL;
			else
				selected = ESPI_IO_MODE_SINGLE;
		}
	} else if (requested_mode == ESPI_IO_MODE_DUAL) {
		if (slave_support == 0x1 || slave_support == 0x3)
			selected = ESPI_IO_MODE_DUAL;
		else {
			printf("espi: slave does not support Dual, using Single\n");
			selected = ESPI_IO_MODE_SINGLE;
		}
	} else {
		selected = ESPI_IO_MODE_SINGLE;
	}

	/*
	 * Write IO Mode Select into bits[27:26] and Alert Mode into bit[28].
	 *   bits[27:26]: 00=Single, 01=Dual, 10=Quad
	 *   bit[28]:     0=IO1 In-Band Alert, 1=ALERT# pin Out-of-Band Alert
	 * SET_CONFIGURATION is sent in Single mode (io_mode is still SINGLE here).
	 */
	new_cap = (cap & ~((0x3 << 26) | BIT(28))) | ((u32)(selected & 0x3) << 26);
	if (use_alert_pin)
		new_cap |= BIT(28);
	if (espi_set_configuration(0x08, new_cap, resp)) {
		espi_set_io_mode(saved_mode);
		return STATUS_ERR;
	}

	/* Switch GPIO directions to the negotiated mode */
	espi_set_io_mode(selected);

	return STATUS_OK;
}

/*
 * Reconfigure GPIO edge-event registers and IEM for the currently selected
 * alert pin (eSPI_IO1 or eSPI_ALERT#) WITHOUT resetting the eSPI bus.
 * Safe to call while the bus is active.
 */
static void espi_reconfigure_alert_pin(void)
{
	u32 alert_bit = use_alert_pin ? eSPI_ALERT : eSPI_IO1;

	/* Disable events on both candidate pins and clear any pending status */
	writel(eSPI_IO1 | eSPI_ALERT, PORT_EVENC);
	writel(eSPI_IO1 | eSPI_ALERT, PORT_EVST);
	/* Re-arm edge-event on the selected pin */
	writel(alert_bit, PORT_EVENS);
	clrsetbits_le32(PORT_EVTYP, ALL_ESPI_PORT_PINS, alert_bit);
	clrsetbits_le32(PORT_EVBE, ALL_ESPI_PORT_PINS, alert_bit);
	/* Refresh GPIO input-enable so ALERT# buffer tracks the selection */
	espi_set_io_mode(io_mode);
}

static int do_espi_command(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	u32 addr, index, data, count, space, tag;
	u8 resp[MAX_RESP_LEN];
	int rc = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (strcmp(argv[1], "start") == 0) {
		espi_init();
		return do_espi_master_start();
	}
	if (strcmp(argv[1], "init") == 0) {
		espi_init();
		return 0;
	}
	if (strcmp(argv[1], "kcstest") == 0) {
		do_kcs_test();
		return 0;
	}
	if (strcmp(argv[1], "autotest") == 0) {
		u8 mode = ESPI_IO_MODE_SINGLE;
		u8 alert = 0; /* default: IO1 In-Band */

		if (argc >= 3) {
			mode = (u8)simple_strtoul(argv[2], NULL, 10);
			if (mode > ESPI_IO_MODE_QUAD) {
				printf("espi autotest: invalid iomode %u (0=single,1=dual,2=quad)\n", mode);
				return CMD_RET_USAGE;
			}
		}
		if (argc >= 4) {
			alert = (u8)simple_strtoul(argv[3], NULL, 10);
			if (alert > 1) {
				printf("espi autotest: invalid alertmode %u (0=IO1,1=ALERT#)\n", alert);
				return CMD_RET_USAGE;
			}
		}
		use_alert_pin = (alert != 0);
		printf("espi autotest: iomode=%u alertmode=%u (%s)\n",
		       mode, alert, use_alert_pin ? "ALERT# pin" : "IO1 In-Band");

		espi_init();
		if (espi_negotiate_io_mode(mode) != 0) {
			printf("[autotest] negotiate_io_mode failed\n");
			return -1;
		}

		do_espi_auto_test();
		//espi_init();
		return 0;
	}

	if (espi_port_state == 0)
		espi_init();

	if (!strcmp(argv[1], "getconfig")) {
		if (argc < 3)
			return CMD_RET_USAGE;
		addr=simple_strtoul(argv[2], NULL, 16);
		espi_get_configuration(addr, resp);
	} else if (!strcmp(argv[1], "setconfig")) {
		if (argc < 4)
			return CMD_RET_USAGE;
		addr=simple_strtoul(argv[2], NULL, 16);
		data=simple_strtoul(argv[3], NULL, 16);
		espi_set_configuration(addr, data, resp);
	} else if (!strcmp(argv[1], "getvw")) {
		debug = true;
		espi_get_vwire(resp);
		debug = false;
	} else if (!strcmp(argv[1], "putvw")) {
		if (argc < 4)
			return CMD_RET_USAGE;
		index=simple_strtoul(argv[2], NULL, 16);
		data=simple_strtoul(argv[3], NULL, 16);
		espi_put_vwire(index, data, resp);
	} else if (!strcmp(argv[1], "iowr")) {
		if (argc < 5)
			return CMD_RET_USAGE;
		addr=simple_strtoul(argv[2], NULL, 16);
		data=simple_strtoul(argv[3], NULL, 16);
		count=simple_strtoul(argv[4], NULL, 16);
		if (count < 1 || count > 4)
			return CMD_RET_USAGE;
		espi_put_iowr(addr, (u8 *)&data, count, resp);
	} else if (!strcmp(argv[1], "iord")) {
		if (argc < 4)
			return CMD_RET_USAGE;
		addr=simple_strtoul(argv[2], NULL, 16);
		count=simple_strtoul(argv[3], NULL, 16);
		if (count < 1 || count > 4)
			return CMD_RET_USAGE;
		debug = true;
		espi_put_iord(addr, count, resp);
		debug = false;
	} else if (!strcmp(argv[1], "flash_read")) {
		if (argc < 6)
			return CMD_RET_USAGE;

		addr = hextoul(argv[2], NULL);
		count = hextoul(argv[3], NULL);
		space = hextoul(argv[4], NULL);
		tag = hextoul(argv[5], NULL);
		if (count < 1)
			return CMD_RET_USAGE;
		debug = true;
		espi_flash_read(addr, count, space, tag);
		debug = false;
	} else if (!strcmp(argv[1], "flash_write")) {
		if (argc < 6)
			return CMD_RET_USAGE;

		addr = hextoul(argv[2], NULL);
		count = hextoul(argv[3], NULL);
		space = hextoul(argv[4], NULL);
		tag = hextoul(argv[5], NULL);
		if (count < 1)
			return CMD_RET_USAGE;
		debug = true;
		espi_flash_write(addr, count, space, tag);
		debug = false;
	} else if (!strcmp(argv[1], "flash_erase")) {
		if (argc < 4)
			return CMD_RET_USAGE;

		addr = hextoul(argv[2], NULL);
		tag = hextoul(argv[3], NULL);
		debug = true;
		espi_flash_erase(addr, tag);
		debug = false;
	} else if (!strcmp(argv[1], "flash_rpmc_op1")) {
		if (argc < 6)
			return CMD_RET_USAGE;

		addr = hextoul(argv[2], NULL);
		count = hextoul(argv[3], NULL);
		space = hextoul(argv[4], NULL);
		tag = hextoul(argv[5], NULL);
		espi_flash_rpmc_op1(addr, count, space, tag);
	} else if (!strcmp(argv[1], "flash_rpmc_op2")) {
		if (argc < 6)
			return CMD_RET_USAGE;

		addr = hextoul(argv[2], NULL);
		count = hextoul(argv[3], NULL);
		space = hextoul(argv[4], NULL);
		tag = hextoul(argv[5], NULL);
		espi_flash_rpmc_op2(addr, count, space, tag);
	} else if (!strcmp(argv[1], "caf_flash")) {
		if (argc < 4)
			return CMD_RET_USAGE;

		index = hextoul(argv[2], NULL);
		count = hextoul(argv[3], NULL);
		espi_caf_flash(index, count);
	} else if (!strcmp(argv[1], "memrd32")) {
		if (argc < 4)
			return CMD_RET_USAGE;
		addr=simple_strtoul(argv[2], NULL, 16);
		count=simple_strtoul(argv[3], NULL, 16);
		if (count < 1 || count > 4)
			return CMD_RET_USAGE;
		debug = true;
		espi_put_memrd32(addr, count, resp);
		debug = false;
	} else if (!strcmp(argv[1], "memwr32")) {
		if (argc < 5)
			return CMD_RET_USAGE;
		addr=simple_strtoul(argv[2], NULL, 16);
		data=simple_strtoul(argv[3], NULL, 16);
		count=simple_strtoul(argv[4], NULL, 16);
		if (count < 1 || count > 4)
			return CMD_RET_USAGE;
		debug = true;
		espi_put_memwr32(addr, (u8 *)&data, count, resp);
		debug = false;
	} else if (!strcmp(argv[1], "getpc")) {
		count = (argc >= 3) ? simple_strtoul(argv[2], NULL, 10) : 1;
		if (count != 1 && count != 2 && count != 4) {
			printf("getpc: invalid len %u (must be 1, 2, or 4)\n", count);
			return CMD_RET_USAGE;
		}
		printf("getpc: waiting for ALERT...\n");
		debug = true;
		rc = espi_wait_and_get_pc(count);
		debug = false;
		if (rc)
			return CMD_RET_FAILURE;
	} else if (!strcmp(argv[1], "oobtest")) {
		if (argc < 3)
			return CMD_RET_USAGE;
		count=simple_strtoul(argv[2], NULL, 16);
		do_oob_test(count);
	} else if (!strcmp(argv[1], "pctest")) {
		do_pc_test();
	} else if (!strcmp(argv[1], "iowrtest")) {
		if (argc < 6)
			return CMD_RET_USAGE;
		addr=simple_strtoul(argv[2], NULL, 16);
		data=simple_strtoul(argv[3], NULL, 16);
		count=simple_strtoul(argv[4], NULL, 16);
		tag=simple_strtoul(argv[5], NULL, 16);
		if (count < 1 || count > 4)
			return CMD_RET_USAGE;
		if (tag < 2)
			debug = true;
		for (int i=0; i<tag; i++) {
			espi_put_iowr(addr, (u8 *)&data, count, resp);
			if (count == 1)
				data = (data+1)%0xff;
		}
		debug = false;
	} else if (!strcmp(argv[1], "getvwtest")) {
		int i=0;
		if (argc < 3)
			return CMD_RET_USAGE;
		tag=simple_strtoul(argv[2], NULL, 16);
		if (tag < 2)
			debug = true;
		for (i=0; i<tag; i++) {
			if (wait_for_vw_avail()) {
				printf("getvwtest avail err\n");
			} else {
				int count = 10;
				while (count > 0) {
					espi_get_vwire(resp);
					if (resp[2] == 0x83 && resp[3] == 0x11) {
						break;
					} else if (resp[2] == 0x83 && resp[3] == 0x10) {
						break;
					}
					mdelay(1);
					count--;
				}

				if (count == 0) {
					printf("getvwtest cnt=0\n");
					break;
				}
			}
		}
		if (i==tag)
			printf("getvwtest pass\n");
		else
			printf("getvwtest fail\n");
		debug = false;
	} else if (!strcmp(argv[1], "putvwtest")) {
		if (argc < 5)
			return CMD_RET_USAGE;
		index=simple_strtoul(argv[2], NULL, 16);
		data=simple_strtoul(argv[3], NULL, 16);
		tag=simple_strtoul(argv[4], NULL, 16);
		if (tag < 2)
			debug = true;
		for (int i=0; i<tag; i++) {
			espi_put_vwire(index, data, resp);
			data ^= 0x08;
		}
		debug = false;
		printf("putvwtest finish\n");
	} else if (!strcmp(argv[1], "kcsstress")) {
		int i=0;
		if (argc < 3)
			return CMD_RET_USAGE;
		tag=simple_strtoul(argv[2], NULL, 16);
		for (i=0; i<tag; i++) {
			do_kcs_test();
		}
	} else if (!strcmp(argv[1], "setiomode")) {
		/*
		 * setiomode <mode>
		 *   0 = Single (default)
		 *   1 = Dual
		 *   2 = Quad
		 * Negotiates with slave via SET_CONFIGURATION and switches GPIO dirs.
		 */
		if (argc < 3)
			return CMD_RET_USAGE;
		data = simple_strtoul(argv[2], NULL, 10);
		if (data > ESPI_IO_MODE_QUAD) {
			printf("invalid mode %u (0=single, 1=dual, 2=quad)\n", data);
			return CMD_RET_USAGE;
		}
		rc = espi_negotiate_io_mode((u8)data);
		if (rc)
			printf("setiomode: negotiation failed\n");
	} else if (!strcmp(argv[1], "getiomode")) {
		const char *mode_str[] = {"single", "dual", "quad"};
		printf("current io_mode: %u (%s)\n", io_mode,
		       io_mode <= ESPI_IO_MODE_QUAD ? mode_str[io_mode] : "unknown");
	} else if (!strcmp(argv[1], "testiomode")) {
		if (argc < 3)
			return CMD_RET_USAGE;
		data = simple_strtoul(argv[2], NULL, 10);
		if (data > ESPI_IO_MODE_QUAD) {
			printf("invalid mode %u (0=single, 1=dual, 2=quad)\n", data);
			return CMD_RET_USAGE;
		}

		struct get_configuration_resp *config_resp = (struct get_configuration_resp *)&resp[0];

		// start test
		espi_init();
		if (espi_negotiate_io_mode((u8)data) != 0) {
			printf("[testiomode] negotiate_io_mode failed\n");
			return -1;
		}

		espi_set_configuration(0x20, 0x00000001, resp);

		espi_get_configuration(0x20, resp);
		if (config_resp->status & STATUS_VW_AVAIL)
			espi_get_vwire(resp);

		/* wait VW channle ready */
		count = 10;
		while (count-- > 0) {
			espi_get_configuration(0x20, resp);
			if (config_resp->data & BIT(1)) {
				printf("VW channel ready\n");
				break;
			}
			mdelay(1);
		}
		/* deassert PLTRST# & SUS_STAT# */
		espi_put_vwire(3, 0x33, resp);
		if (config_resp->status & STATUS_VW_AVAIL)
			espi_get_vwire(resp);

		espi_set_configuration(0x10, 0x00001113, resp);
		/* wait peripheral channle ready */
		count = 10;
		while (count-- > 0) {
			espi_get_configuration(0x10, resp);
			if (config_resp->data & BIT(1)) {
				printf("Peripheral channel ready\n");
				break;
			}
			mdelay(1);
		}

		// enable KCS1
		u8 temp = 0x7;
		espi_put_iowr(0x4e, &temp, 0x1, resp);
		temp = 0x11;
		espi_put_iowr(0x4f, &temp, 0x1, resp);
		temp = 0x30;
		espi_put_iowr(0x4e, &temp, 0x1, resp);
		temp = 0x1;
		espi_put_iowr(0x4f, &temp, 0x1, resp);

		return 0;
	} else if (!strcmp(argv[1], "alertmode")) {
		/*
		 * alertmode <0|1>
		 *   0 = IO1-based alert (In-Band ALERT, default)
		 *   1 = Dedicated ALERT# pin (GPIO168, Out-of-Band ALERT)
		 *
		 * Only reconfigures the local GPIO edge-event registers and IEM.
		 * Does NOT send any bus transaction to the slave.  Bit[28] of the
		 * General Capabilities register (0x08) is written to the slave
		 * later by "espi setiomode", which negotiates both the I/O mode
		 * (bits[27:26]) and the alert mode (bit[28]) in a single
		 * SET_CONFIGURATION(0x08) command.
		 *
		 * Intended usage:
		 *   espi init
		 *   espi alertmode <0|1>
		 *   espi setiomode <0|1|2>   <- notifies slave of both settings
		 */
		if (argc < 3)
			return CMD_RET_USAGE;
		data = simple_strtoul(argv[2], NULL, 10);
		use_alert_pin = (data != 0);
		printf("espi: alert mode -> %s (run 'espi setiomode' to apply to slave)\n",
		       use_alert_pin ? "ALERT# pin (GPIO168)" : "IO1 (In-Band)");
		/* Reconfigure GPIO event/IEM only — no bus transaction */
		espi_reconfigure_alert_pin();
	} else if (!strcmp(argv[1], "alertdispatch")) {
		/*
		 * Wait for ALERT#/IO1 edge then print which channel(s) have data.
		 * Useful for observing slave-initiated events (e.g. SLV_BOOT_DONE)
		 * without polling GET_STATUS in a loop from the console.
		 *
		 * Usage: espi alertdispatch
		 *
		 * Status register bits reported:
		 *   BIT(4)  PC_AVAIL       BIT(5)  NP_AVAIL
		 *   BIT(6)  VW_AVAIL       BIT(7)  OOB_AVAIL
		 *   BIT(12) FLASH_C_AVAIL  BIT(13) FLASH_NP_FREE
		 */
		u16 st = 0;
		printf("espi: waiting for alert...\n");
		if (wait_alert_and_dispatch(&st)) {
			printf("espi: alertdispatch timed out\n");
		} else {
			printf("espi: alert received, status=0x%04x\n", st);
			if (st & BIT(4))  printf("  -> PC_AVAIL\n");
			if (st & BIT(5))  printf("  -> NP_AVAIL\n");
			if (st & BIT(6))  printf("  -> VW_AVAIL\n");
			if (st & BIT(7))  printf("  -> OOB_AVAIL\n");
			if (st & BIT(12)) printf("  -> FLASH_C_AVAIL\n");
			if (st & BIT(13)) printf("  -> FLASH_NP_FREE\n");
			if (!(st & (BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(12)|BIT(13))))
				printf("  (no channel bits set in status)\n");
		}
	} else {
		rc = CMD_RET_USAGE;
	}

	return rc;
}

static int do_ipmi_bmc_cmd(char *arg)
{
	u8 req[2];
	int read_len;
	struct get_devid_resp devid;
	//struct get_selinfo_resp selinfo;
	u8 *resp;
	int resp_len;
	u8 netfn_lun;
	u8 cmd;

	if (!strcmp(arg, "info")) {
		netfn_lun = 0x18;
		cmd = 1; // get_device_id
		resp = (u8 *)&devid;
		resp_len = sizeof(devid);
#if 0
	} else if (!strcmp(arg, "sel")) {
		netfn_lun = 0x28;
		cmd = 0x40; // get_sel_info
		resp = (u8 *)&selinfo;
		resp_len = sizeof(selinfo);
	} else if (!strcmp(arg, "sdr")) {
		netfn_lun = 0x28;
		cmd = 0x20; // get_sdr_info
		resp = (u8 *)&selinfo;
		resp_len = sizeof(selinfo);
#endif
	} else {
		return -EINVAL;
	}
	req[0] = netfn_lun;
	req[1] = cmd;
	read_len = kcs_transfer(req, 2, resp, resp_len);

	if (read_len < 0) {
		printf("kcs transfer err\n");
		return 0;
	}
	if (!strcmp(arg, "info")) {
		printf("Device ID\t\t: %d\n", devid.dev_id);
		printf("Device Revision\t\t: %d\n", devid.dev_rev);
		printf("Firmware Revision\t: %x.%x\n", devid.fw_rev1, devid.fw_rev2);
		printf("IPMI Version\t\t: %d.%d\n", devid.ipmi_ver&0x0F, devid.ipmi_ver >> 4);
		printf("Manufacturer ID\t\t: 0x%06x\n", devid.manf_id[2]<<16 | devid.manf_id[1]<<8 | devid.manf_id[0]);
		printf("Porduct ID\t\t: 0x%04x\n", devid.prod_id[1] << 8 | devid.prod_id[0]);
	}
#if 0
	else if (!strcmp(arg, "sel")) {
		printf("Version\t\t\t: %d.%d\n", selinfo.sel_ver&0x0F, selinfo.sel_ver >> 4);
		printf("Entries\t\t\t: %d\n", selinfo.entries_msb << 8 | selinfo.entries_lsb);
		printf("Free Space\t\t: %d\n", selinfo.free_space);
		if (selinfo.add_timestamp == -1)
			printf("Last Add Time\t\t: Not Available\n");
		else
			printf("Last Add Time\t\t: %d\n", selinfo.add_timestamp);
		if (selinfo.add_timestamp == -1)
			printf("Last Del Time\t\t: Not Available\n");
		else
			printf("Last Del Time\t\t: %d\n", selinfo.erase_timestamp);
	}
#endif
	//for (i = 0; i < read_len; i++)
	//	printf("%02x ", resp[i]);
	//printf("\n");

	return 0;
}

static int do_ipmi_sdr(int record)
{
	u8 req[8];
	u8 resp[128];
	int resp_len = 5 + 64; // Fn_Lun + Cmd + Comp_code + Recid_lsb + Recid_msb + record[64]
	int read_len;
	//u8 m, b, r_exp, b_exp;
	//u8 sensor_num;
	u8 sensor_name[17];

	req[0] = 0x28; // storage
	req[1] = 0x23; // getSDR
	req[2] = 0; // reserveation id
	req[3] = 0;
	req[4] = record & 0xFF;
	req[5] = (record >> 8) & 0xFF;
	req[6] = 0;
	req[7] = 0xFF;
	read_len = kcs_transfer(req, 8, resp, resp_len);

	if (read_len < 5) {
		printf("ipmi sdr err\n");
		return 0;
	}
	if (resp[2] != 0) {
		printf("getSDR completion code=0x%x\n", resp[2]);
		return 0;
	}
	// record type
	if (resp[8] != 1) {
		// not full sensor record type
		return 0;
	}
	// sensor type
	if (resp[17] != 1) {
		// not temperature
		return 0;
	}
	#if 0
	sensor_num = resp[12];
	m = resp[29];
	b = resp[31];
	r_exp = (resp[34] >> 4) & 0x0F;
	b_exp = (resp[34]) & 0x0F;

	printf("resp len=%d\n", read_len);
	printf("sensor number=0x%x\n", resp[12]);
	printf("sensor type=0x%x\n", resp[17]);
	printf("sensor base unit=0x%x\n", resp[26]);
	printf("sensor modifier unit=0x%x\n", resp[27]);
	printf("ID string type=0x%02x\n", resp[52]);
	printf("raw reading=%d\n", resp[36]);
	printf("M=%d, B=%d\n", resp[29], resp[31]);
	printf("R exp=%d, B exp=%d\n", (resp[34]>>4)&0x0F, resp[34]&0x0F);
	#endif
	memset(sensor_name, 0, sizeof(sensor_name));
	memcpy(sensor_name, &resp[53], resp[52] & 0x1F);

	// Get Sensor Reading
	req[0] = 0x10; // sensor/event
	req[1] = 0x2D; // get sensor reading
	req[2] = resp[12];
	read_len = kcs_transfer(req, 3, resp, resp_len);

	if (read_len < 0) {
		printf("kcs transfer err\n");
		return 0;
	}
	if (resp[2] != 0) {
		printf("Get Sensor Reading completion code=0x%x\n", resp[2]);
		return 0;
	}
	printf("%s: %d degree C\n", sensor_name, resp[3]);

	return 0;
}

static int do_ipmi_command(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int record;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (strcmp(argv[1], "bmc") == 0) {
		return do_ipmi_bmc_cmd(argv[2]);
	}
	if (strcmp(argv[1], "sdr") == 0) {
		if (argc > 2)
			record = simple_strtoul(argv[2], NULL, 16);
		else
			record = 8; //EVB_Temp
		return do_ipmi_sdr(record);
	}

	return 0;
}

static int do_espi_uart_command(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int i;
	u8 resp[MAX_RESP_LEN];
	u8 data;
	char *nuvoton ;
	if (strcmp(argv[1], "auto") == 0) {

		nuvoton = "Nuvoton Technology Corporation (Nuvoton) was founded to bring innovative semiconductor solutions to the market. Nuvoton was spun-off as a Winbond Electronics affiliate in July 2008 and went public in September 2010 on the Taiwan Stock Exchange (TWSE).Nuvoton focuses on the developments of microcontroller/audio, cloud security, battery monitoring, component, visual sensing and IoT with security ICs and has strong market share in Industrial, Automotive, Communication, Consumer and Computer markets. Nuvoton owns 6-inch wafer fabs equipped with diversified processing technologies to provide professional wafer foundry services. Nuvoton provides products with a high performance/cost ratio for its customers by leveraging flexible technology, advanced design capability, and integration of digital and analog technologies. Nuvoton values long term relationships with its partners and customers and is dedicated to continuous innovation of its products, processes, and services. Nuvoton has established subsidiaries in the USA, China, Israel, India, Singapore, Korea, Japan and Germany to strengthen regional customer support and global management. For more information, please visit http://www.nuvoton.com \n";

		for ( i = 0; i < strlen(nuvoton); i++) {
			sprintf(&data, "%c", nuvoton[i]);
			espi_put_iowr(0x3f8, &data, 1, resp);
			udelay(100);
		}
		return 0;
	}

	if (strcmp(argv[1], "manual") == 0) {
		ulong addr;
		ulong len;
		u8 lsr;
		int count;

		addr = hextoul(argv[2], NULL);
		len = hextoul(argv[3], NULL);

		nuvoton = (char *)addr;

		for ( i = 0; i < len; i++) {
			sprintf(&data, "%c", nuvoton[i]);
			espi_put_iowr(0x3f8, &data, 1, resp);
			count = 1000;
			while (count--) {
				espi_iord8(0x3fd, (u8 *)&lsr);
				/* Check THRE bit */
				if (lsr & BIT(5))
					break;
			}
			if (count == 0) {
				printf("uart write timeout\n");
				break;
			}

			if (ctrlc())
				break;
		}

		return 0;
	}

	return 0;

}

U_BOOT_CMD(
	espi ,	6,	1,	do_espi_command,
	"espi master command",
	"<command> [<argument1>] [<argument2>] [<argument3>]\n"
	"<command> \n"
	" init - init espi gpios and assert nRST\n"
	" start - start espi master auto config\n"
	" kcstest - do the kcs transfer\n"
	" autotest - do the espi auto test\n"
	"  	<argument1> - optional I/O mode: 0=single(default), 1=dual, 2=quad\n"
	"  	<argument2> - optional alert mode: 0=IO1 In-Band(default), 1=ALERT# pin\n"
	" getconfig - get_configuration\n"
	"  	<argument1> - address\n"
	" setconfig - set_configuration.\n"
	"  	<argument1> - address\n"
	"  	<argument2> - data\n"
	" getvw - get_vwire.\n"
	" putvw - put_vwire.\n"
	"  	<argument1> - vw index\n"
	"  	<argument2> - vw data\n"
	" iowr - put_iowr_short.\n"
	"  	<argument1> - address\n"
	"  	<argument2> - data\n"
	"  	<argument3> - data len (bytes)\n"
	" iord - put_iord_short.\n"
	"  	<argument1> - address\n"
	"  	<argument2> - data len (bytes)\n"
	" flash_read - flash_read.\n"
	"  	<argument1> - flash offset\n"
	"  	<argument2> - data len (bytes)\n"
	"  	<argument3> - memory address to receive\n"
	"	<argument4> - tag\n"
	" flash_write - flash_write.\n"
	"  	<argument1> - flash offset\n"
	"  	<argument2> - data len (bytes)\n"
	"  	<argument3> - memory address to write\n"
	"	<argument4> - tag\n"
	" flash_erase - erase flash 4KB.\n"
	"  	<argument1> - flash offset\n"
	"	<argument2> - tag\n"
	" flash_rpmc_op1 - flash rpmc op1. \n"
	"	<argument1> - flash device\n"
	"	<argument2> - data len (bytes)\n"
	"	<argument3> - memory address to write\n"
	"	<argument4> - tag\n"
	" flash_rpmc_op2 - flash rpmc op2. \n"
	"	<argument1> - flash device\n"
	"	<argument2> - data len (bytes)\n"
	"	<argument3> - memory address to receive\n"
	"	<argument4> - tag\n"
	" caf_flash - read/write/erase.\n"
	"	<argument1> - cycle type\n"
	"	<argument2> - data len (bytes)\n"
	" memrd32 - put_memrd32_short.\n"
	"       <argument1> - address\n"
	"       <argument2> - data len (bytes)\n"
	" memwr32 - put_memwr32_short.\n"
	"       <argument1> - address\n"
	"       <argument2> - data\n"
	"       <argument3> - data len (bytes)\n"
	" getpc - wait for ALERT, poll PC_AVAIL, then issue GET_PC.\n"
	"         Used to retrieve completion after a DEFER response.\n"
	"       <argument1> - data len in bytes: 1, 2, or 4 (default 1)\n"
	" oobtest - get_oob and put_oob.\n"
	"       <argument1> - data\n"
	" pctest - put_memrd32_short + put_memwr32_short.\n"
	" iowrtest - put_iowr_short.\n"
	"       <argument2> - data\n"
	"       <argument3> - data len (bytes)\n"
	"       <argument4> - count\n"
	" putvwtest - put_vwire \n"
	"       <argument1> - count\n"
        "       <argument1> - vw index\n"
        "       <argument2> - vw data\n"
        "       <argument3> - count\n"
	" getvwtest - get_vwire \n"
        "       <argument1> - address\n"
	" kcsstress - do the kcs transfer\n"
	"       <argument1> - count\n"
	" setiomode - negotiate and switch I/O mode with slave.\n"
	"       <argument1> - mode: 0=single, 1=dual, 2=quad\n"
	"       Issues GET_CONFIGURATION(0x08) to check slave capability,\n"
	"       then SET_CONFIGURATION to select the mode, and switches GPIOs.\n"
	" getiomode - print current I/O mode (single/dual/quad).\n"
	" testiomode - test IO mode stability\n"
	"		<argument1> - mode: 0=single, 1=dual, 2=quad\n"
	" alertmode - select alert pin and reconfigure GPIO/IEM (no bus transaction)\n"
	"		<argument1> - mode: 0=IO1 In-Band, 1=ALERT# pin\n"
	" alertdispatch - wait for ALERT#/IO1 edge then print pending channel(s)\n"
	"		(uses wait_alert_and_dispatch + GET_STATUS)\n"
);

U_BOOT_CMD(
	ipmitool ,	3,	1,	do_ipmi_command,
	"ipmi command",
	"<command> <argument>\n"
	"<command> \n"
	" bmc - bmc command\n"
	"     info - show bmc info\n"
	" sdr - show sensor reading\n"
);

U_BOOT_CMD(
	espi_uart ,	4,	1,	do_espi_uart_command,
	"host uart demo",
	"<command> [<argument1>] [<argument2>]\n"
	" auto - send default pattern\n"
	" manual\n"
	"  	<argument1> - address\n"
	"  	<argument2> - data len (bytes)\n"
);
