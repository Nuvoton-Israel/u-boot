// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022 Nuvoton Technology Corp.
 */

#include <dm.h>
#include <errno.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <dm/pinctrl.h>
#include <linux/bitfield.h>
#include <asm/arch/rst.h>

/* GCR register offsets */
#define WD0RCR		0x38
#define WD1RCR		0x3c
#define WD2RCR		0x40
#define SWRSTC1		0x44
#define SWRSTC2		0x48
#define SWRSTC3		0x4c
#define TIPRSTC		0x50
#define CORSTC		0x5c
#define FLOCKR1		0x74
#define INTCR4		0xc0
#define I2CSEGSEL	0xe0
#define MFSEL1		0x260
#define MFSEL2		0x264
#define MFSEL3		0x268
#define MFSEL4		0x26c
#define MFSEL5		0x270
#define MFSEL6		0x274
#define MFSEL7		0x278

/* GPIO register offsets */
#define GPIO_POL	0x08 /* Polarity */
#define GPIO_DOUT	0x0c /* Data OUT */
#define GPIO_OTYP	0x14 /* Output Type */
#define GPIO_PU		0x1c /* Pull-up */
#define GPIO_PD		0x20 /* Pull-down */
#define GPIO_DBNC	0x24 /* Debounce */
#define GPIO_EVEN	0x40 /* Event Enable */
#define GPIO_EVST	0x4c /* Event Status */
#define GPIO_IEM	0x58 /* Input Enable */
#define GPIO_OSRC	0x5c /* Output Slew-Rate Control */
#define GPIO_ODSC	0x60 /* Output Drive Strength Control */
#define GPIO_OES	0x70 /* Output Enable Set */
#define GPIO_OEC	0x74 /* Output Enable Clear */

#define NPCM8XX_GPIO_PER_BANK	32
#define GPIOX_OFFSET	16

/* The lists contain alternate GPIO pins of the function */
/* Serial Interfaces */
static const uint hsi1a_pins[] = { 43, 63 };
static const uint hsi1b_pins[] = { 44, 62 };
static const uint hsi1c_pins[] = { 45, 46, 47, 61 };
static const uint hsi2a_pins[] = { 48, 49 };
static const uint hsi2b_pins[] = { 50, 51 };
static const uint hsi2c_pins[] = { 52, 53, 54, 55 };
static const uint bmcuart0a_pins[] = { 41, 42 };
static const uint bmcuart0b_pins[] = { 48, 49 };
static const uint bmcuart1_pins[] = { 43, 63 };
static const uint nbu1crts_pins[] = { 44, 62 };
static const uint bu2_pins[] = { 96, 97};
static const uint bu4_pins[] = { 54, 55 };
static const uint bu4b_pins[] = { 98, 99 };
static const uint bu5_pins[] = { 52, 53 };
static const uint bu5b_pins[] = { 100, 101 };
static const uint bu6_pins[] = { 50, 51 };

/* SPI/FIU/FLM */
static const uint spi0cs1_pins[] = { 32 };
static const uint spi1_pins[] = { 175, 176, 177, 203 };
static const uint spi1cs1_pins[] = { 233 };
static const uint spi1d23_pins[] = { 191, 192 };
static const uint spi1cs2_pins[] = { 191 };
static const uint spi1cs3_pins[] = { 192 };
static const uint spi3_pins[] = { 183, 184, 185, 186 };
static const uint spi3cs1_pins[] = { 187 };
static const uint spi3quad_pins[] = { 188, 189 };
static const uint spi3cs2_pins[] = { 188 };
static const uint spi3cs3_pins[] = { 189 };
static const uint spix_pins[] = { 224, 225, 226, 227, 229, 230 };
static const uint spixcs1_pins[] = { 228 };
static const uint pspi_pins[] = { 17, 18, 19 };
static const uint fm2_pins[] = { 224, 225, 226, 227, 228, 229, 230 };
static const uint fm1_pins[] = { 175, 176, 177, 203, 191, 192, 233 };
static const uint fm0_pins[] = { 194, 195, 196, 202, 199, 198, 197 };

/* I2C */
static const uint smb0_pins[]  = { 115, 114 };
static const uint smb0b_pins[] = { 195, 194 };
static const uint smb0c_pins[] = { 202, 196 };
static const uint smb0d_pins[] = { 198, 199 };
static const uint smb0den_pins[] = { 197 };
static const uint smb1_pins[] = { 117, 116 };
static const uint smb1b_pins[] = { 126, 127 };
static const uint smb1c_pins[] = { 124, 125 };
static const uint smb1d_pins[] = { 4, 5 };
static const uint smb2_pins[] = { 119, 118 };
static const uint smb2b_pins[] = { 122, 123 };
static const uint smb2c_pins[] = { 120, 121 };
static const uint smb2d_pins[] = { 6, 7 };
static const uint smb3_pins[] = { 30, 31 };
static const uint smb3b_pins[] = { 39, 40 };
static const uint smb3c_pins[] = { 37, 38 };
static const uint smb3d_pins[] = { 59, 60 };
static const uint smb4_pins[] = { 28, 29 };
static const uint smb4b_pins[] = { 18, 19 };
static const uint smb4c_pins[] = { 20, 21 };
static const uint smb4d_pins[] = { 22, 23 };
static const uint smb5_pins[] = { 26, 27 };
static const uint smb5b_pins[] = { 13, 12 };
static const uint smb5c_pins[] = { 15, 14 };
static const uint smb5d_pins[] = { 94, 93 };
static const uint smb6_pins[] = { 172, 171 };
static const uint smb6b_pins[] = { 2, 3 };
static const uint smb6c_pins[] = { 0, 1 };
static const uint smb6d_pins[] = { 10, 11 };
static const uint smb7_pins[] = { 174, 173 };
static const uint smb7b_pins[] = { 16, 141 };
static const uint smb7c_pins[] = { 24, 25 };
static const uint smb7d_pins[] = { 142, 143 };
static const uint smb8_pins[] = { 129, 128 };
static const uint smb9_pins[] = { 131, 130 };
static const uint smb10_pins[] = { 133, 132 };
static const uint smb11_pins[] = { 135, 134 };
static const uint smb12_pins[] = { 221, 220 };
static const uint smb13_pins[] = { 223, 222 };
static const uint smb14_pins[] = { 22, 23 };
static const uint smb14b_pins[] = { 32, 187 };
static const uint smb15_pins[] = { 20, 21 };
static const uint smb15b_pins[] = { 192, 191 };
static const uint smb16_pins[] = { 10, 11 };
static const uint smb16b_pins[] = { 218, 219 };
static const uint smb17_pins[] = { 3, 2 };
static const uint smb18_pins[] = { 0, 1 };
static const uint smb19_pins[] = { 60, 59 };
static const uint smb20_pins[] = { 234, 235 };
static const uint smb21_pins[] = { 169, 170 };
static const uint smb22_pins[] = { 40, 39 };
static const uint smb23_pins[] = { 38, 37 };
static const uint smb23b_pins[] = { 134, 134 };

/* I3C */
static const uint i3c5_pins[] = { 106, 107 };
static const uint i3c4_pins[] = { 33, 34 };
static const uint i3c3_pins[] = { 246, 247 };
static const uint i3c2_pins[] = { 244, 245 };
static const uint i3c1_pins[] = { 242, 243 };
static const uint i3c0_pins[] = { 240, 241 };

/* Fan/PWM */
static const uint fanin0_pins[] = { 64 };
static const uint fanin1_pins[] = { 65 };
static const uint fanin2_pins[] = { 66 };
static const uint fanin3_pins[] = { 67 };
static const uint fanin4_pins[] = { 68 };
static const uint fanin5_pins[] = { 69 };
static const uint fanin6_pins[] = { 70 };
static const uint fanin7_pins[] = { 71 };
static const uint fanin8_pins[] = { 72 };
static const uint fanin9_pins[] = { 73 };
static const uint fanin10_pins[] = { 74 };
static const uint fanin11_pins[] = { 75 };
static const uint fanin12_pins[] = { 76 };
static const uint fanin13_pins[] = { 77 };
static const uint fanin14_pins[] = { 78 };
static const uint fanin15_pins[] = { 79 };
static const uint faninx_pins[] = { 175, 176, 177, 203 };
static const uint pwm0_pins[] = { 80 };
static const uint pwm1_pins[] = { 81 };
static const uint pwm2_pins[] = { 82 };
static const uint pwm3_pins[] = { 83 };
static const uint pwm4_pins[] = { 144 };
static const uint pwm5_pins[] = { 145 };
static const uint pwm6_pins[] = { 146 };
static const uint pwm7_pins[] = { 147 };
static const uint pwm8_pins[] = { 220 };
static const uint pwm9_pins[] = { 221 };
static const uint pwm10_pins[] = { 234 };
static const uint pwm11_pins[] = { 235 };

/* Network */
static const uint rg1mdio_pins[] = { 108, 109 }; /* SGMII1 */
static const uint rg2_pins[] = { 110, 111, 112, 113, 208, 209, 210, 211, 212,
	213, 214, 215 }; /* RGMII2 */
static const uint rg2mdio_pins[] = { 216, 217 };
static const uint rg2refck_pins[] = { 250 };
static const uint r1_pins[] = { 178, 179, 180, 181, 182, 193, 201 }; /* RMII1 */
static const uint r1err_pins[] = { 56 };
static const uint r1oen_pins[] = { 56 };
static const uint r1md_pins[] = { 57, 58 };
static const uint r1en_pins[] = {  };
static const uint r2_pins[] = { 84, 85, 86, 87, 88, 89, 200 }; /* RMII2 */
static const uint r2md_pins[] = { 91, 92 };
static const uint r2err_pins[] = { 90 };
static const uint r2oen_pins[] = { 90 };
static const uint r2en_pins[] = {  };
static const uint rmii3_pins[] = { 110, 111, 209, 210, 211, 214, 215 };
static const uint r3rxer_pins[] = { 212 };
static const uint r3oen_pins[] = { 213 };
static const uint r3en_pins[] = {  };

/* MMC */
static const uint mmc_pins[] = { 152, 154, 156, 157, 158, 159 };
static const uint mmc8_pins[] = { 148, 149, 150, 151 };
static const uint mmcwp_pins[] = { 153 };
static const uint mmccd_pins[] = { 155 };
static const uint mmcrst_pins[] = { 155 };

/* JTAG */
static const uint jtag2_pins[] = { 43, 44, 45, 46, 47 };
static const uint j2j3_pins[] = { 44, 62, 45, 46 };
static const uint jm1_pins[] = { 136, 137, 138, 139, 140 };
static const uint jm2_pins[] = { 248 };

/* Serial I/O Expander*/
static const uint iox1_pins[] = { 0, 1, 2, 3 };
static const uint iox2_pins[] = { 4, 5, 6, 7 };
static const uint ioxh_pins[] = { 10, 11, 24, 25 };

/* LPC/eSPI */
static const uint lpc_pins[] = { 95, 161, 163, 164, 165, 166, 167 };
static const uint espi_pins[] = { 95, 161, 163, 164, 165, 166, 167, 168 };
static const uint clkrun_pins[] = { 162 };
static const uint serirq_pins[] = { 168 };
static const uint nprd_smi_pins[] = { 190 };
static const uint scipme_pins[] = { 169 };
static const uint sci_pins[] = { 170 };

/* VGA */
static const uint vgadig_pins[] = { 102, 103, 104, 105 };
static const uint gspi_pins[] = { 12, 13, 14, 15 };

/* PCIe */
static const uint clkreq_pins[] = { 231 };

/* MISC */
static const uint ga20kbc_pins[] = { 94, 93 };
static const uint clkout_pins[] = { 160 };
static const uint wdog1_pins[] = { 218 };
static const uint wdog2_pins[] = { 219 };
static const uint ddr_pins[] = { 110, 111, 112, 113, 208, 209, 210, 211, 212,
	213, 214, 215, 216, 217, 250 };

/* TIP/COP */
static const uint tp_gpio7_pins[] = { 96 };
static const uint tp_gpio6_pins[] = { 97 };
static const uint tp_gpio5_pins[] = { 98 };
static const uint tp_gpio4_pins[] = { 99 };
static const uint tp_gpio3_pins[] = { 100 };
static const uint tp_gpio2_pins[] = { 16 };
static const uint tp_gpio1_pins[] = { 9 };
static const uint tp_gpio0_pins[] = { 8 };
static const uint tp_gpio5b_pins[] = { 58 };
static const uint tp_gpio4b_pins[] = { 57 };
static const uint tp_gpio2b_pins[] = { 101 };
static const uint tp_gpio1b_pins[] = { 92 };
static const uint tp_gpio0b_pins[] = { 91 };
static const uint tp_uart_pins[] = { 50, 51 };
static const uint tp_smb2_pins[] = { 24, 25 };
static const uint tp_smb1_pins[] = { 142, 143 };
static const uint tp_jtag3_pins[] = { 44, 62, 45, 46 };
static const uint cp1gpio2c_pins[] = { 101 };
static const uint cp1gpio3c_pins[] = { 100 };
static const uint cp1gpio0b_pins[] = { 127 };
static const uint cp1gpio1b_pins[] = { 126 };
static const uint cp1gpio2b_pins[] = { 125 };
static const uint cp1gpio3b_pins[] = { 124 };
static const uint cp1gpio4b_pins[] = { 99 };
static const uint cp1gpio5b_pins[] = { 98 };
static const uint cp1gpio6b_pins[] = { 97 };
static const uint cp1gpio7b_pins[] = { 96 };
static const uint cp1gpio0_pins[] = {  };
static const uint cp1gpio1_pins[] = {  };
static const uint cp1gpio2_pins[] = {  };
static const uint cp1gpio3_pins[] = {  };
static const uint cp1gpio4_pins[] = {  };
static const uint cp1gpio5_pins[] = { 17 };
static const uint cp1gpio6_pins[] = { 91 };
static const uint cp1gpio7_pins[] = { 92 };
static const uint cp1utxd_pins[] = { 42 };
static const uint cp1urxd_pins[] = { 41 };

/* General IO */
static const uint gpi35_pins[] = { 35 };
static const uint gpi36_pins[] = { 36 };
static const uint gpio1836_pins[] = { 183, 184, 185, 186 };
static const uint gpio1889_pins[] = { 188, 189 };
static const uint gpio187_pins[] = { 187 };
static const uint lkgpo0_pins[] = { 16 };
static const uint lkgpo1_pins[] = { 8 };
static const uint lkgpo2_pins[] = { 9 };
static const uint hgpio0_pins[] = { 20 };
static const uint hgpio1_pins[] = { 21 };
static const uint hgpio2_pins[] = { 22 };
static const uint hgpio3_pins[] = { 23 };
static const uint hgpio4_pins[] = { 24 };
static const uint hgpio5_pins[] = { 25 };
static const uint hgpio6_pins[] = { 59 };
static const uint hgpio7_pins[] = { 60 };
static char *gpio_func_name = "gpio";
static char **npcm8xx_funcs;
static int npcm8xx_num_funcs;
#define GPIO_FUNC_SEL	0

struct npcm8xx_pinctrl_priv {
	void __iomem *gpio_base;
	struct regmap *gcr_regmap;
	struct regmap *rst_regmap;
};

struct group_config {
	char *name;
	const uint *pins;
	uint npins;
	u32 reg; /* Register of setting func */
	u32 bit;
};

#define GRP(x, _reg, _bit) {			\
	.name = #x,				\
	.pins = x## _pins,			\
	.npins = ARRAY_SIZE(x## _pins),		\
	.reg = _reg,				\
	.bit = _bit,				\
	}
static const struct group_config npcm8xx_groups[] = {
	GRP(smb3, MFSEL1, 0),
	GRP(smb4, MFSEL1, 1),
	GRP(smb5, MFSEL1, 2),
	GRP(spi0cs1, MFSEL1, 3),
	GRP(hsi1c, MFSEL1, 4),
	GRP(hsi2c, MFSEL1, 5),
	GRP(smb0, MFSEL1, 6),
	GRP(smb1, MFSEL1, 7),
	GRP(smb2, MFSEL1, 8),
	GRP(bmcuart0a, MFSEL1, 9),
	GRP(hsi1a, MFSEL1, 10),
	GRP(hsi2a, MFSEL1, 11),
	GRP(r1err, MFSEL1, 12),
	GRP(r1md, MFSEL1, 13),
	GRP(r2, MFSEL1, 14),
	GRP(r2err, MFSEL1, 15),
	GRP(r2md, MFSEL1, 16),
	GRP(ga20kbc, MFSEL1, 17),
	GRP(clkout, MFSEL1, 21),
	GRP(sci, MFSEL1, 22),
	GRP(gspi, MFSEL1, 24),
	GRP(lpc, MFSEL1, 26),
	GRP(hsi1b, MFSEL1, 28),
	GRP(hsi2b, MFSEL1, 29),
	GRP(iox1, MFSEL1, 30),
	GRP(serirq, MFSEL1, 31),
	GRP(fanin0, MFSEL2, 0),
	GRP(fanin1, MFSEL2, 1),
	GRP(fanin2, MFSEL2, 2),
	GRP(fanin3, MFSEL2, 3),
	GRP(fanin4, MFSEL2, 4),
	GRP(fanin5, MFSEL2, 5),
	GRP(fanin6, MFSEL2, 6),
	GRP(fanin7, MFSEL2, 7),
	GRP(fanin8, MFSEL2, 8),
	GRP(fanin9, MFSEL2, 9),
	GRP(fanin10, MFSEL2, 10),
	GRP(fanin11, MFSEL2, 11),
	GRP(fanin12, MFSEL2, 12),
	GRP(fanin13, MFSEL2, 13),
	GRP(fanin14, MFSEL2, 14),
	GRP(fanin15, MFSEL2, 15),
	GRP(pwm0, MFSEL2, 16),
	GRP(pwm1, MFSEL2, 17),
	GRP(pwm2, MFSEL2, 18),
	GRP(pwm3, MFSEL2, 19),
	GRP(pwm4, MFSEL2, 20),
	GRP(pwm5, MFSEL2, 21),
	GRP(pwm6, MFSEL2, 22),
	GRP(pwm7, MFSEL2, 23),
	GRP(hgpio0, MFSEL2, 24),
	GRP(hgpio1, MFSEL2, 25),
	GRP(hgpio2, MFSEL2, 26),
	GRP(hgpio3, MFSEL2, 27),
	GRP(hgpio4, MFSEL2, 28),
	GRP(hgpio5, MFSEL2, 29),
	GRP(hgpio6, MFSEL2, 30),
	GRP(hgpio7, MFSEL2, 31),
	GRP(scipme, MFSEL3, 0),
	GRP(smb6, MFSEL3, 1),
	GRP(smb7, MFSEL3, 2),
	GRP(faninx, MFSEL3, 3),
	GRP(spi1, MFSEL3, 4),
	GRP(smb12, MFSEL3, 5),
	GRP(smb13, MFSEL3, 6),
	GRP(smb14, MFSEL3, 7),
	GRP(smb15, MFSEL3, 8),
	GRP(r1,	MFSEL3,	9),
	GRP(mmc, MFSEL3, 10),
	GRP(mmc8, MFSEL3, 11),
	GRP(pspi, MFSEL3, 13),
	GRP(iox2, MFSEL3, 14),
	GRP(clkrun, MFSEL3, 16),
	GRP(ioxh, MFSEL3, 18),
	GRP(wdog1, MFSEL3, 19),
	GRP(wdog2, MFSEL3, 20),
	GRP(i3c5, MFSEL3, 22),
	GRP(bmcuart1, MFSEL3, 24),
	GRP(mmccd, MFSEL3, 25),
	GRP(ddr, MFSEL3, 26),
	GRP(jtag2, MFSEL4, 0),
	GRP(bmcuart0b, MFSEL4, 1),
	GRP(mmcrst, MFSEL4, 6),
	GRP(espi, MFSEL4, 8),
	GRP(clkreq, MFSEL4, 9),
	GRP(smb8, MFSEL4, 11),
	GRP(smb9, MFSEL4, 12),
	GRP(smb10, MFSEL4, 13),
	GRP(smb11, MFSEL4, 14),
	GRP(spi3, MFSEL4, 16),
	GRP(spi3cs1, MFSEL4, 17),
	GRP(spi3cs2, MFSEL4, 18),
	GRP(spi3cs3, MFSEL4, 19),
	GRP(spi3quad, MFSEL4, 20),
	GRP(rg1mdio, MFSEL4, 21),
	GRP(bu2, MFSEL4, 22),
	GRP(rg2mdio, MFSEL4, 23),
	GRP(rg2, MFSEL4, 24),
	GRP(spix, MFSEL4, 27),
	GRP(spixcs1, MFSEL4, 28),
	GRP(spi1cs1, MFSEL5, 0),
	GRP(jm2, MFSEL5, 1),
	GRP(j2j3, MFSEL5, 2),
	GRP(spi1d23, MFSEL5, 3),
	GRP(spi1cs2, MFSEL5, 4),
	GRP(spi1cs3, MFSEL5, 5),
	GRP(bu6, MFSEL5, 6),
	GRP(bu5, MFSEL5, 7),
	GRP(bu4, MFSEL5, 8),
	GRP(r1oen, MFSEL5, 9),
	GRP(r2oen, MFSEL5, 10),
	GRP(rmii3, MFSEL5, 11),
	GRP(bu5b, MFSEL5, 12),
	GRP(bu4b, MFSEL5, 13),
	GRP(r3oen, MFSEL5, 14),
	GRP(jm1, MFSEL5, 15),
	GRP(gpi35, MFSEL5, 16),
	GRP(i3c0, MFSEL5, 17),
	GRP(gpi36, MFSEL5, 18),
	GRP(i3c1, MFSEL5, 19),
	GRP(tp_gpio4b, MFSEL5, 20),
	GRP(i3c2, MFSEL5, 21),
	GRP(tp_gpio5b, MFSEL5, 22),
	GRP(i3c3, MFSEL5, 23),
	GRP(smb16, MFSEL5, 24),
	GRP(smb17, MFSEL5, 25),
	GRP(smb18, MFSEL5, 26),
	GRP(smb19, MFSEL5, 27),
	GRP(smb20, MFSEL5, 28),
	GRP(smb21, MFSEL5, 29),
	GRP(smb22, MFSEL5, 30),
	GRP(smb23, MFSEL5, 31),
	GRP(smb23b, MFSEL6, 0),
	GRP(cp1utxd, MFSEL6, 1),
	GRP(cp1gpio0, MFSEL6, 2),
	GRP(cp1gpio1, MFSEL6, 3),
	GRP(cp1gpio2, MFSEL6, 4),
	GRP(cp1gpio3, MFSEL6, 5),
	GRP(cp1gpio4, MFSEL6, 6),
	GRP(cp1gpio5, MFSEL6, 7),
	GRP(cp1gpio6, MFSEL6, 8),
	GRP(cp1gpio7, MFSEL6, 9),
	GRP(i3c4, MFSEL6, 10),
	GRP(pwm8, MFSEL6, 11),
	GRP(pwm9, MFSEL6, 12),
	GRP(pwm10, MFSEL6, 13),
	GRP(pwm11, MFSEL6, 14),
	GRP(nbu1crts, MFSEL6, 15),
	GRP(fm0, MFSEL6, 16),
	GRP(fm1, MFSEL6, 17),
	GRP(fm2, MFSEL6, 18),
	GRP(gpio1836, MFSEL6, 19),
	GRP(cp1gpio0b, MFSEL6, 20),
	GRP(cp1gpio1b, MFSEL6, 21),
	GRP(cp1gpio2b, MFSEL6, 22),
	GRP(cp1gpio3b, MFSEL6, 23),
	GRP(cp1gpio7b, MFSEL6, 24),
	GRP(cp1gpio6b, MFSEL6, 25),
	GRP(cp1gpio5b, MFSEL6, 26),
	GRP(cp1gpio4b, MFSEL6, 27),
	GRP(cp1gpio3c, MFSEL6, 28),
	GRP(cp1gpio2c, MFSEL6, 29),
	GRP(r3rxer, MFSEL6, 30),
	GRP(cp1urxd, MFSEL6, 31),
	GRP(tp_gpio0, MFSEL7, 0),
	GRP(tp_gpio1, MFSEL7, 1),
	GRP(tp_gpio2, MFSEL7, 2),
	GRP(tp_gpio3, MFSEL7, 3),
	GRP(tp_gpio4, MFSEL7, 4),
	GRP(tp_gpio5, MFSEL7, 5),
	GRP(tp_gpio6, MFSEL7, 6),
	GRP(tp_gpio7, MFSEL7, 7),
	GRP(tp_gpio0b, MFSEL7, 8),
	GRP(tp_gpio1b, MFSEL7, 9),
	GRP(tp_gpio2b, MFSEL7, 10),
	GRP(tp_smb1, MFSEL7, 11),
	GRP(tp_uart, MFSEL7, 12),
	GRP(tp_jtag3, MFSEL7, 13),
	GRP(gpio187, MFSEL7, 24),
	GRP(gpio1889, MFSEL7, 25),
	GRP(smb14b, MFSEL7, 26),
	GRP(smb15b, MFSEL7, 27),
	GRP(tp_smb2, MFSEL7, 28),
	GRP(vgadig, MFSEL7, 29),
	GRP(smb16b, MFSEL7, 30),
	GRP(smb0b, I2CSEGSEL, 0),
	GRP(smb0c, I2CSEGSEL, 1),
	GRP(smb0d, I2CSEGSEL, 2),
	GRP(smb1b, I2CSEGSEL, 5),
	GRP(smb1c, I2CSEGSEL, 6),
	GRP(smb1d, I2CSEGSEL, 7),
	GRP(smb2b, I2CSEGSEL, 8),
	GRP(smb2c, I2CSEGSEL, 9),
	GRP(smb2d, I2CSEGSEL, 10),
	GRP(smb3b, I2CSEGSEL, 11),
	GRP(smb3c, I2CSEGSEL, 12),
	GRP(smb3d, I2CSEGSEL, 13),
	GRP(smb4b, I2CSEGSEL, 14),
	GRP(smb4c, I2CSEGSEL, 15),
	GRP(smb4d, I2CSEGSEL, 16),
	GRP(smb5b, I2CSEGSEL, 19),
	GRP(smb5c, I2CSEGSEL, 20),
	GRP(smb5d, I2CSEGSEL, 21),
	GRP(smb0den, I2CSEGSEL, 22),
	GRP(smb6b, I2CSEGSEL, 24),
	GRP(smb6c, I2CSEGSEL, 25),
	GRP(smb6d, I2CSEGSEL, 26),
	GRP(smb7b, I2CSEGSEL, 27),
	GRP(smb7c, I2CSEGSEL, 28),
	GRP(smb7d, I2CSEGSEL, 29),
	GRP(lkgpo0, FLOCKR1, 0),
	GRP(lkgpo1, FLOCKR1, 4),
	GRP(lkgpo2, FLOCKR1, 8),
	GRP(nprd_smi, FLOCKR1, 20),
	GRP(mmcwp, FLOCKR1, 24),
	GRP(rg2refck, INTCR4, 6),
	GRP(r1en, INTCR4, 12),
	GRP(r2en, INTCR4, 13),
	GRP(r3en, INTCR4, 14),
};

/* Pin flags */
#define SLEW		BIT(0) /* Has Slew Control */
#define GPIO_ALT	BIT(1) /* GPIO function is enabled by setting alternate */
#define DSLO_MASK	GENMASK(11, 8)  /* Drive strength */
#define DSHI_MASK	GENMASK(15, 12)
#define GPIO_IDX_MASK	GENMASK(18, 16)
#define GPIO_IDX(x)	((x) << 16) /* index of alt_func[] for gpio function */
#define DS(lo, hi)	(((lo) << 8) | ((hi) << 12))
#define DSLO(x)		FIELD_GET(DSLO_MASK, x) /* Low DS value */
#define DSHI(x)		FIELD_GET(DSHI_MASK, x) /* High DS value */
#define GPIO_IDX_VAL(x)	FIELD_GET(GPIO_IDX_MASK, x)

#define MAX_ALT_FUNCS	5 /* Max alternate functions */
struct pin_info {
	int gpio_num;
	char *name;
	char *alt_func[MAX_ALT_FUNCS];
	int num_funcs;
	u32 flags;
};

static const struct pin_info npcm8xx_pins[] = {
	{0, "GPIO0/IOX1_DI/SMB6C_SDA/SMB18_SDA", {"iox1", "smb6c", "smb18"}, 3, SLEW},
	{1, "GPIO1/IOX1_LD/SMB6C_SCL/SMB18_SCL", {"iox1", "smb6c", "smb18"}, 3, SLEW},
	{2, "GPIO2/IOX1_CK/SMB6B_SDA/SMB17_SDA", {"iox1", "smb6b", "smb17"}, 3, SLEW},
	{3, "GPIO3/IOX1_DO/SMB6B_SCL/SMB17_SCL", {"iox1", "smb6b", "smb17"}, 3, SLEW},
	{4, "GPIO4/IOX2_DI/SMB1D_SDA", {"iox2", "smb1d"}, 2, SLEW},
	{5, "GPIO5/IOX2_LD/SMB1D_SCL", {"iox2", "smb1d"}, 2, SLEW},
	{6, "GPIO6/IOX2_CK/SMB2D_SDA", {"iox2", "smb2d"}, 2, SLEW},
	{7, "GPIO7/IOX2_D0/SMB2D_SCL", {"iox2", "smb2d"}, 2, SLEW},
	{8, "GPIO8/LKGPO1/TP_GPIO0", {"lkgpo1", "tp_gpio0b"}, 2, DS(8, 12)},
	{9, "GPIO9/LKGPO2/TP_GPIO1", {"lkgpo2", "tp_gpio1b"}, 2, DS(8, 12)},
	{10, "GPIO10/IOXH_LD/SMB6D_SCL/SMB16_SCL", {"ioxh", "smb6d", "smb16"}, 3, SLEW},
	{11, "GPIO11/IOXH_CK/SMB6D_SDA/SMB16_SDA", {"ioxh", "smb6d", "smb16"}, 3, SLEW},
	{12, "GPIO12/GSPI_CK/SMB5B_SCL", {"gspi", "smb5d"}, 2, SLEW},
	{13, "GPIO13/GSPI_DO/SMB5B_SDA", {"gspi", "smb5d"}, 2, SLEW},
	{14, "GPIO14/GSPI_DI/SMB5C_SCL", {"gspi", "smb5c"}, 2, SLEW},
	{15, "GPIO15/GSPI_CS/SMB5C_SDA", {"gspi", "smb5c"}, 2, SLEW},
	{16, "GPIO16/SMB7B_SDA/LKGPO0/TP_GPIO2", {"lkgpo0", "smb7b", "tp_gpio2b"}, 3, SLEW},
	{17, "GPIO17/PSPI_DI/CP1_GPIO5", {"pspi", "cp1gpio5"}, 2, SLEW},
	{18, "GPIO18/PSPI_D0/SMB4B_SDA", {"pspi", "smb4b"}, 2, SLEW},
	{19, "GPIO19/PSPI_CK/SMB4B_SCL", {"pspi", "smb4b"}, 2, SLEW},
	{20, "GPIO20/H_GPIO0/SMB4C_SDA/SMB15_SDA", {"hgpio0", "smb15", "smb4c"}, 3, SLEW},
	{21, "GPIO21/H_GPIO1/SMB4C_SCL/SMB15_SCL", {"hgpio1", "smb15", "smb4c"}, 3, SLEW},
	{22, "GPIO22/H_GPIO2/SMB4D_SDA/SMB14_SDA", {"hgpio2", "smb14", "smb4d"}, 3, SLEW},
	{23, "GPIO23/H_GPIO3/SMB4D_SCL/SMB14_SCL", {"hgpio3", "smb14", "smb4d"}, 3, SLEW},
	{24, "GPIO24/IOXH_DO/H_GPIO4/SMB7C_SCL/TP_SMB2_SCL",
	     {"hgpio4", "ioxh", "smb7c", "tp_smb2"}, 4, SLEW},
	{25, "GPIO25/IOXH_DI/H_GPIO4/SMB7C_SDA/TP_SMB2_SDA", {"hgpio5", "ioxh", "smb7c"}, 3, SLEW},
	{26, "GPIO26/SMB5_SDA", {"smb5"}, 1, 0},
	{27, "GPIO27/SMB5_SCL", {"smb5"}, 1, 0},
	{28, "GPIO28/SMB4_SDA", {"smb4"}, 1, 0},
	{29, "GPIO29/SMB4_SCL", {"smb4"}, 1, 0},
	{30, "GPIO30/SMB3_SDA", {"smb3"}, 1, 0},
	{31, "GPIO31/SMB3_SCL", {"smb3"}, 1, 0},
	{32, "GPIO32/SMB14_SCL/SPI0_nCS1", {"smb14b", "spi0cs1"}, 2, SLEW},
	{33, "I3C4_SCL", {"i3c4"}, 1, SLEW},
	{34, "I3C4_SDA", {"i3c4"}, 1, SLEW},
	{35, "GPI35/MCBPCK", {"gpi35"}, 1, GPIO_ALT | GPIO_IDX(0)},
	{36, "GPI36/SYSBPCK", {"gpi36"}, 1, GPIO_ALT | GPIO_IDX(0)},
	{37, "GPIO37/SMB3C_SDA/SMB23_SDA", {"smb3c", "smb23"}, 2, SLEW},
	{38, "GPIO38/SMB3C_SCL/SMB23_SCL", {"smb3c", "smb23"}, 2, SLEW},
	{39, "GPIO39/SMB3B_SDA/SMB22_SDA", {"smb3b", "smb22"}, 2, SLEW},
	{40, "GPIO40/SMB3B_SCL/SMB22_SCL", {"smb3b", "smb22"}, 2, SLEW},
	{41, "GPIO41/BU0_RXD/CP1U_RXD", {"bmcuart0a", "cp1urxd"}, 2, 0},
	{42, "GPIO42/BU0_TXD/CP1U_TXD", {"bmcuart0a", "cp1utxd"}, 2, DS(2, 4)},
	{43, "GPIO43/SI1_RXD/BU1_RXD", {"hsi1a", "bmcuart1"}, 2, 0},
	{44, "GPIO44/SI1_nCTS/BU1_nCTS/CP_TDI/TP_TDI/CP_TP_TDI",
	     {"hsi1b", "nbu1crts", "jtag2", "tp_jtag3", "j2j3"}, 5, 0},
	{45, "GPIO45/SI1_nDCD/CP_TMS_SWIO/TP_TMS_SWIO/CP_TP_TMS_SWIO",
	     {"hsi1c", "jtag2", "j2j3", "tp_jtag3"}, 4, DS(2, 8)},
	{46, "GPIO46/SI1_nDSR/CP_TCK_SWCLK/TP_TCK_SWCLK/CP_TP_TCK_SWCLK",
	     {"hsi1c", "jtag2", "j2j3", "tp_jtag3"}, 4, 0},
	{47, "GPIO47/SI1n_RI1", {"hsi1c",}, 1, DS(2, 8)},
	{48, "GPIO48/SI2_TXD/BU0_TXD/STRAP5", {"hsi2a", "bmcuart0b"}, 2, 0},
	{49, "GPIO49/SI2_RXD/BU0_RXD", {"hsi2a", "bmcuart0b"}, 2, 0},
	{50, "GPIO50/SI2_nCTS/BU6_TXD/TPU_TXD", {"hsi2b", "bu6", "tp_uart"}, 3, 0},
	{51, "GPIO51/SI2_nRTS/BU6_RXD/TPU_RXD", {"hsi2b", "bu6", "tp_uart"}, 3, 0},
	{52, "GPIO52/SI2_nDCD/BU5_RXD", {"hsi2c", "bu5"}, 2, 0},
	{53, "GPIO53/SI2_nDTR_BOUT2/BU5_TXD", {"hsi2c", "bu5"}, 2, 0},
	{54, "GPIO54/SI2_nDSR/BU4_TXD", {"hsi2c", "bu4"}, 2, 0},
	{55, "GPIO55/SI2_RI2/BU4_RXD", {"hsi2c", "bu4"}, 2, 0},
	{56, "GPIO56/R1_RXERR/R1_OEN", {"r1err", "r1oen"}, 2, 0},
	{57, "GPIO57/R1_MDC/TP_GPIO4", {"r1md", "tp_gpio4b"}, 2, DS(2, 4)},
	{58, "GPIO58/R1_MDIO/TP_GPIO5", {"r1md", "tp_gpio5b"}, 2, DS(2, 4)},
	{59, "GPIO59/H_GPIO06/SMB3D_SDA/SMB19_SDA", {"hgpio6", "smb3d", "smb19"}, 3, 0},
	{60, "GPIO60/H_GPIO07/SMB3D_SCL/SMB19_SCL", {"hgpio7", "smb3d", "smb19"}, 3, 0},
	{61, "GPIO61/SI1_nDTR_BOUT", {"hsi1c"}, 1, 0},
	{62, "GPIO62/SI1_nRTS/BU1_nRTS/CP_TDO_SWO/TP_TDO_SWO/CP_TP_TDO_SWO",
	     {"hsi1b", "jtag2", "j2j3", "nbu1crts", "tp_jtag3"}, 5, 0},
	{63, "GPIO63/BU1_TXD1/SI1_TXD", {"hsi1a", "bmcuart1"}, 2, 0},
	{64, "GPIO64/FANIN0", {"fanin0"}, 1, 0},
	{65, "GPIO65/FANIN1", {"fanin1"}, 1, 0},
	{66, "GPIO66/FANIN2", {"fanin2"}, 1, 0},
	{67, "GPIO67/FANIN3", {"fanin3"}, 1, 0},
	{68, "GPIO68/FANIN4", {"fanin4"}, 1, 0},
	{69, "GPIO69/FANIN5", {"fanin5"}, 1, 0},
	{70, "GPIO70/FANIN6", {"fanin6"}, 1, 0},
	{71, "GPIO71/FANIN7", {"fanin7"}, 1, 0},
	{72, "GPIO72/FANIN8", {"fanin8"}, 1, 0},
	{73, "GPIO73/FANIN9", {"fanin9"}, 1, 0},
	{74, "GPIO74/FANIN10", {"fanin10"}, 1, 0},
	{75, "GPIO75/FANIN11", {"fanin11"}, 1, 0},
	{76, "GPIO76/FANIN12", {"fanin12"}, 1, 0},
	{77, "GPIO77/FANIN13", {"fanin13"}, 1, 0},
	{78, "GPIO78/FANIN14", {"fanin14"}, 1, 0},
	{79, "GPIO79/FANIN15", {"fanin15"}, 1, 0},
	{80, "GPIO80/PWM0", {"pwm0"}, 1, DS(4, 8)},
	{81, "GPIO81/PWM1", {"pwm1"}, 1, DS(4, 8)},
	{82, "GPIO82/PWM2", {"pwm2"}, 1, DS(4, 8)},
	{83, "GPIO83/PWM3", {"pwm3"}, 1, DS(4, 8)},
	{84, "GPIO84/R2_TXD0", {"r2"}, 1, DS(4, 8) | SLEW},
	{85, "GPIO85/R2_TXD1", {"r2"}, 1, DS(4, 8) | SLEW},
	{86, "GPIO86/R2_TXEN", {"r2"}, 1, DS(4, 8) | SLEW},
	{87, "GPIO87/R2_RXD0", {"r2"}, 1, 0},
	{88, "GPIO88/R2_RXD1", {"r2"}, 1, 0},
	{89, "GPIO89/R2_CRSDV", {"r2"}, 1, 0},
	{90, "GPIO90/R2_RXERR/R2_OEN", {"r2err", "r2oen"}, 2, 0},
	{91, "GPIO91/R2_MDC/CP1_GPIO6/TP_GPIO0", {"r2md", "cp1gpio6", "tp_gpio0"}, 3, DS(2, 4)},
	{92, "GPIO92/R2_MDIO/CP1_GPIO7/TP_GPIO1", {"r2md", "cp1gpio7", "tp_gpio1"}, 3, DS(2, 4)},
	{93, "GPIO93/GA20/SMB5D_SCL", {"ga20kbc", "smb5d"}, 2, 0},
	{94, "GPIO94/nKBRST/SMB5D_SDA", {"ga20kbc", "smb5d"}, 2, 0},
	{95, "GPIO95/nESPIRST/LPC_nLRESET", {"lpc", "espi"}, 2, 0},
	{96, "GPIO96/CP1_GPIO7/BU2_TXD/TP_GPIO7", {"cp1gpio7b", "bu2", "tp_gpio7"}, 3, SLEW},
	{97, "GPIO97/CP1_GPIO6/BU2_RXD/TP_GPIO6", {"cp1gpio6b", "bu2", "tp_gpio6"}, 3, SLEW},
	{98, "GPIO98/CP1_GPIO5/BU4_TXD/TP_GPIO5", {"bu4b", "cp1gpio5b", "tp_gpio5"}, 3, SLEW},
	{99, "GPIO99/CP1_GPIO4/BU4_RXD/TP_GPIO4", {"bu4b", "cp1gpio4b", "tp_gpio4"}, 3, SLEW},
	{100, "GPIO100/CP1_GPIO3/BU5_TXD/TP_GPIO3", {"bu5b", "cp1gpio3c", "tp_gpio3"}, 3, SLEW},
	{101, "GPIO101/CP1_GPIO2/BU5_RXD/TP_GPIO2", {"bu5b", "cp1gpio2c", "tp_gpio2"}, 3, SLEW},
	{102, "GPIO102/HSYNC", {"vgadig"}, 1, DS(4, 8)},
	{103, "GPIO103/VSYNC", {"vgadig"}, 1, DS(4, 8)},
	{104, "GPIO104/DDC_SCL", {"vgadig"}, 1, 0},
	{105, "GPIO105/DDC_SDA", {"vgadig"}, 1, 0},
	{106, "GPIO106/I3C5_SCL", {"i3c5"}, 1, SLEW},
	{107, "GPIO107/I3C5_SDA", {"i3c5"}, 1, SLEW},
	{108, "GPIO108/SG1_MDC", {"rg1mdio"}, 1, SLEW},
	{109, "GPIO109/SG1_MDIO", {"rg1mdio"}, 1, SLEW},
	{110, "GPIO110/RG2_TXD0/DDRV0/R3_TXD0", {"rg2", "ddr", "rmii3"}, 3, SLEW},
	{111, "GPIO111/RG2_TXD1/DDRV1/R3_TXD1", {"rg2", "ddr", "rmii3"}, 3, SLEW},
	{112, "GPIO112/RG2_TXD2/DDRV2", {"rg2", "ddr"}, 2, SLEW},
	{113, "GPIO113/RG2_TXD3/DDRV3", {"rg2", "ddr"}, 2, SLEW},
	{114, "GPIO114/SMB0_SCL", {"smb0"}, 1, 0},
	{115, "GPIO115/SMB0_SDA", {"smb0"}, 1, 0},
	{116, "GPIO116/SMB1_SCL", {"smb1"}, 1, 0},
	{117, "GPIO117/SMB1_SDA", {"smb1"}, 1, 0},
	{118, "GPIO118/SMB2_SCL", {"smb2"}, 1, 0},
	{119, "GPIO119/SMB2_SDA", {"smb2"}, 1, 0},
	{120, "GPIO120/SMB2C_SDA", {"smb2c"}, 1, SLEW},
	{121, "GPIO121/SMB2C_SCL", {"smb2c"}, 1, SLEW},
	{122, "GPIO122/SMB2B_SDA", {"smb2b"}, 1, SLEW},
	{123, "GPIO123/SMB2B_SCL", {"smb2b"}, 1, SLEW},
	{124, "GPIO124/SMB1C_SDA/CP1_GPIO3", {"smb1c", "cp1gpio3b"}, 2, SLEW},
	{125, "GPIO125/SMB1C_SCL/CP1_GPIO2", {"smb1c", "cp1gpio2b"}, 2, SLEW},
	{126, "GPIO126/SMB1B_SDA/CP1_GPIO1", {"smb1b", "cp1gpio1b"}, 2, SLEW},
	{127, "GPIO127/SMB1B_SCL/CP1_GPIO0", {"smb1b", "cp1gpio0b"}, 2, SLEW},
	{128, "GPIO128/SMB824_SCL", {"smb8"}, 1, 0},
	{129, "GPIO129/SMB824_SDA", {"smb8"}, 1, 0},
	{130, "GPIO130/SMB925_SCL", {"smb9"}, 1, 0},
	{131, "GPIO131/SMB925_SDA", {"smb9"}, 1, 0},
	{132, "GPIO132/SMB1026_SCL", {"smb10"}, 1, 0},
	{133, "GPIO133/SMB1026_SDA", {"smb10"}, 1, 0},
	{134, "GPIO134/SMB11_SCL", {"smb11", "smb23b"}, 2, 0},
	{135, "GPIO135/SMB11_SDA", {"smb11", "smb23b"}, 2, 0},
	{136, "GPIO136/JM1_TCK", {"jm1"}, 1, SLEW},
	{137, "GPIO137/JM1_TDO", {"jm1"}, 1, SLEW},
	{138, "GPIO138/JM1_TMS", {"jm1"}, 1, SLEW},
	{139, "GPIO139/JM1_TDI", {"jm1"}, 1, SLEW},
	{140, "GPIO140/JM1_nTRST", {"jm1"}, 1, SLEW},
	{141, "GPIO141/SMB7B_SCL", {"smb7b"}, 1, 0},
	{142, "GPIO142/SMB7D_SCL/TPSMB1_SCL", {"smb7d", "tp_smb1"}, 2, SLEW},
	{143, "GPIO143/SMB7D_SDA/TPSMB1_SDA", {"smb7d", "tp_smb1"}, 2, SLEW},
	{144, "GPIO144/PWM4", {"pwm4"}, 1, DS(4, 8)},
	{145, "GPIO145/PWM5", {"pwm5"}, 1, DS(4, 8)},
	{146, "GPIO146/PWM6", {"pwm6"}, 1, DS(4, 8)},
	{147, "GPIO147/PWM7", {"pwm7"}, 1, DS(4, 8)},
	{148, "GPIO148/MMC_DT4", {"mmc8"}, 1, DS(8, 12) | SLEW},
	{149, "GPIO149/MMC_DT5", {"mmc8"}, 1, DS(8, 12) | SLEW},
	{150, "GPIO150/MMC_DT6", {"mmc8"}, 1, DS(8, 12) | SLEW},
	{151, "GPIO151/MMC_DT7", {"mmc8"}, 1, DS(8, 12) | SLEW},
	{152, "GPIO152/MMC_CLK", {"mmc"}, 1, DS(8, 12) | SLEW},
	{153, "GPIO153/MMC_WP", {"mmcwp"}, 1, 0},
	{154, "GPIO154/MMC_CMD", {"mmc"}, 1, DS(8, 12) | SLEW},
	{155, "GPIO155/MMC_nCD/MMC_nRSTLK", {"mmccd", "mmcrst"}, 2, 0},
	{156, "GPIO156/MMC_DT0", {"mmc"}, 1, DS(8, 12) | SLEW},
	{157, "GPIO157/MMC_DT1", {"mmc"}, 1, DS(8, 12) | SLEW},
	{158, "GPIO158/MMC_DT2", {"mmc"}, 1, DS(8, 12) | SLEW},
	{159, "GPIO159/MMC_DT3", {"mmc"}, 1, DS(8, 12) | SLEW},
	{160, "GPIO160/CLKOUT/RNGOSCOUT/GFXBYPCK", {"clkout"}, 1, DS(8, 12) | SLEW},
	{161, "GPIO161/ESPI_nCS/LPC_nLFRAME", {"espi", "lpc"}, 2, 0},
	{162, "GPIO162/LPC_nCLKRUN", {"clkrun"}, 1, DS(8, 12)},
	{163, "GPIO163/ESPI_CK/LPC_LCLK", {"espi", "lpc"}, 2, 0},
	{164, "GPIO164/ESPI_IO0/LPC_LAD0", {"espi", "lpc"}, 2, 0},
	{165, "GPIO165/ESPI_IO1/LPC_LAD1", {"espi", "lpc"}, 2, 0},
	{166, "GPIO166/ESPI_IO2/LPC_LAD2", {"espi", "lpc"}, 2, 0},
	{167, "GPIO167/ESPI_IO3/LPC_LAD3", {"espi", "lpc"}, 2, 0},
	{168, "GPIO168/ESPI_nALERT/SERIRQ", {"espi", "serirq"}, 2, 0},
	{169, "GPIO169/nSCIPME/SMB21_SCL", {"scipme", "smb21"}, 2, 0},
	{170, "GPIO170/nSMI/SMB21_SDA", {"sci", "smb21"}, 2, 0},
	{171, "GPIO171/SMB6_SCL", {"smb6"}, 1, 0},
	{172, "GPIO172/SMB6_SDA", {"smb6"}, 1, 0},
	{173, "GPIO173/SMB7_SCL", {"smb7"}, 1, 0},
	{174, "GPIO174/SMB7_SDA", {"smb7"}, 1, 0},
	{175, "GPIO175/SPI1_CK/FANIN19/FM1_CK", {"spi1", "faninx", "fm1"}, 3, DS(8, 12)},
	{176, "GPIO176/SPI1_DO/FANIN18/FM1_DO/STRAP9", {"spi1", "faninx", "fm1"}, 3, DS(8, 12)},
	{177, "GPIO177/SPI1_DI/FANIN17/FM1_D1/STRAP10", {"spi1", "faninx", "fm1"}, 3, DS(8, 12)},
	{178, "GPIO178/R1_TXD0", {"r1"}, 1, DS(8, 12) | SLEW},
	{179, "GPIO179/R1_TXD1", {"r1"}, 1, DS(8, 12) | SLEW},
	{180, "GPIO180/R1_TXEN", {"r1"}, 1, DS(8, 12) | SLEW},
	{181, "GPIO181/R1_RXD0", {"r1"}, 1, 0},
	{182, "GPIO182/R1_RXD1", {"r1"}, 1, 0},
	{183, "GPIO183/SPI3_SEL", {"spi3", "gpio1836"}, 2,
	      DS(8, 12) | SLEW | GPIO_ALT | GPIO_IDX(1)},
	{184, "GPIO184/SPI3_D0/STRAP13", {"spi3", "gpio1836"}, 2,
	      DS(8, 12) | SLEW | GPIO_ALT | GPIO_IDX(1)},
	{185, "GPIO185/SPI3_D1", {"spi3", "gpio1836"}, 2,
	      DS(8, 12) | SLEW | GPIO_ALT | GPIO_IDX(1)},
	{186, "GPIO186/SPI3_nCS0", {"spi3", "gpio1836"}, 2,
	      DS(8, 12) | SLEW | GPIO_ALT | GPIO_IDX(1)},
	{187, "GPIO187/SPI3_nCS1_SMB14_SDA", {"spi3cs1", "smb14b", "gpio187"}, 3,
	      SLEW | GPIO_ALT | GPIO_IDX(2)},
	{188, "GPIO188/SPI3_D2/SPI3_nCS2", {"spi3quad", "spi3cs2", "gpio1889"}, 3,
	      DS(8, 12) | SLEW | GPIO_ALT | GPIO_IDX(2)},
	{189, "GPIO189/SPI3_D3/SPI3_nCS3", {"spi3quad", "spi3cs3", "gpio1889"}, 3,
	      DS(8, 12) | SLEW | GPIO_ALT | GPIO_IDX(2)},
	{190, "GPIO190/nPRD_SMI", {"nprd_smi"}, 1, DS(2, 4)},
	{191, "GPIO191/SPI1_D1/FANIN17/FM1_D1/STRAP10",
	      {"spi1d23", "spi1cs2", "fm1", "smb15"}, 4, SLEW},
	{192, "GPIO192/SPI1_D3/SPI_nCS3/FM1_D3/SMB15_SCL",
	      {"spi1d23", "spi1cs3", "fm1", "smb15"}, 4, SLEW},
	{193, "GPIO193/R1_CRSDV", {"r1"}, 1, 0},
	{194, "GPIO194/SMB0B_SCL/FM0_CK", {"smb0b", "fm0"}, 2, SLEW},
	{195, "GPIO195/SMB0B_SDA/FM0_D0", {"smb0b", "fm0"}, 2, SLEW},
	{196, "GPIO196/SMB0C_SCL/FM0_D1", {"smb0c", "fm0"}, 2, SLEW},
	{197, "GPIO197/SMB0DEN/FM0_D3", {"smb0den", "fm0"}, 2, SLEW},
	{198, "GPIO198/SMB0D_SDA/FM0_D2", {"smb0d", "fm0"}, 2, SLEW},
	{199, "GPIO199/SMB0D_SCL/FM0_CSO", {"smb0d", "fm0"}, 2, SLEW},
	{200, "GPIO200/R2_CK", {"r2"}, 1, 0},
	{201, "GPIO201/R1_CK", {"r1"}, 1, 0},
	{202, "GPIO202/SMB0C_SDA/FM0_CSI", {"smb0c", "fm0"}, 2, SLEW},
	{203, "GPIO203/SPI1_nCS0/FANIN16/FM1_CSI", {"faninx", "spi1", "fm1"}, 3, DS(8, 12)},
	{208, "GPIO208/RG2_TXC/DVCK", {"rg2", "ddr"}, 2, SLEW},
	{209, "GPIO209/RG2_TXCTL/DDRV4/R3_TXEN", {"rg2", "ddr", "rmii3"}, 3, SLEW},
	{210, "GPIO210/RG2_RXD0/DDRV5/R3_RXD0", {"rg2", "ddr", "rmii3"}, 3, DS(8, 12) | SLEW},
	{211, "GPIO211/RG2_RXD1/DDRV6/R3_RXD1", {"rg2", "ddr", "rmii3"}, 3, DS(8, 12) | SLEW},
	{212, "GPIO212/RG2_RXD2/DDRV7/R3_RXD2", {"rg2", "ddr", "r3rxer"}, 3, DS(8, 12) | SLEW},
	{213, "GPIO213/RG2_RXD3/DDRV8/R3_OEN", {"rg2", "ddr", "r3oen"}, 3, DS(8, 12) | SLEW},
	{214, "GPIO214/RG2_RXC/DDRV9/R3_CK", {"rg2", "ddr", "rmii3"}, 3, DS(8, 12) | SLEW},
	{215, "GPIO215/RG2_RXCTL/DDRV10/R3_CRSDV", {"rg2", "ddr", "rmii3"}, 3, DS(8, 12) | SLEW},
	{216, "GPIO216/RG2_MDC/DDRV11", {"rg2mdio", "ddr"}, 2, DS(8, 12) | SLEW},
	{217, "GPIO217/RG2_MDIO/DVHSYNC", {"rg2mdio", "ddr"}, 2, DS(8, 12) | SLEW},
	{218, "GPIO218/nWDO1/SMB16_SCL", {"wdog1", "smb16"}, 2, SLEW},
	{219, "GPIO219/nWDO2/SMB16_SDA", {"wdog2", "smb16"}, 2, SLEW},
	{220, "GPIO220/SMB12_SCL/PWM8", {"smb12", "pwm8"}, 2, SLEW},
	{221, "GPIO221/SMB12_SDA/PWM9", {"smb12", "pwm9"}, 2, SLEW},
	{222, "GPIO222/SMB13_SCL", {"smb13"}, 1, SLEW},
	{223, "GPIO223/SMB13_SDA", {"smb13"}, 1, SLEW},
	{224, "GPIO224/SPIX_CK/FM2_CK", {"spix", "fm2"}, 2, DS(8, 12) | SLEW},
	{225, "GPO225/SPIX_D0/FM2_D0/STRAP1", {"spix", "fm2"}, 2, DS(8, 12) | SLEW},
	{226, "GPO226/SPIX_D1/FM2_D1/STRAP2", {"spix", "fm2"}, 2, DS(8, 12) | SLEW},
	{227, "GPIO227/SPIX_nCS0/FM2_CSI", {"spix", "fm2"}, 2, DS(8, 12) | SLEW},
	{228, "GPIO228/SPIX_nCS1/FM2_CSO", {"spixcs1", "fm2"}, 2, DS(8, 12) | SLEW},
	{229, "GPO229/SPIX_D2/FM2_D2/STRAP3", {"spix", "fm2"}, 2, DS(8, 12) | SLEW},
	{230, "GPO230/SPIX_D3/FM2_D3/STRAP6", {"spix", "fm2"}, 2, DS(8, 12) | SLEW},
	{231, "GPIO231/EP_nCLKREQ", {"clkreq"}, 1, DS(4, 12) | SLEW},
	{233, "GPIO233/SPI1_nCS1/FM1_CSO", {"spi1cs1", "fm1"}, 2, 0},
	{234, "GPIO234/PWM10/SMB20_SCL", {"pwm10", "smb20"}, 2, SLEW},
	{235, "GPIO235/PWM11/SMB20_SDA", {"pwm11", "smb20"}, 2, SLEW},
	{240, "GPIO240/I3C0_SCL", {"i3c0"}, 2, SLEW},
	{241, "GPIO241/I3C0_SDA", {"i3c0"}, 2, SLEW},
	{242, "GPIO242/I3C1_SCL", {"i3c1"}, 2, SLEW},
	{243, "GPIO243/I3C1_SDA", {"i3c1"}, 2, SLEW},
	{244, "GPIO244/I3C2_SCL", {"i3c2"}, 2, SLEW},
	{245, "GPIO245/I3C2_SDA", {"i3c2"}, 2, SLEW},
	{246, "GPIO246/I3C3_SCL", {"i3c3"}, 2, SLEW},
	{247, "GPIO247/I3C3_SDA", {"i3c3"}, 2, SLEW},
	{250, "GPIO250/RG2_REFCK/DVVSYNC", {"ddr", "rg2refck"}, 2, DS(8, 12) | SLEW},
};

static const struct group_config *npcm8xx_group_get(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(npcm8xx_groups); i++) {
		if (!strcmp(npcm8xx_groups[i].name, name))
			return &npcm8xx_groups[i];
	}

	return NULL;
}

static int npcm8xx_get_pin_selector(int gpio)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(npcm8xx_pins); i++) {
		if (npcm8xx_pins[i].gpio_num == gpio)
			return i;
	}

	return -ENOENT;
}

static int npcm8xx_group_set_func(struct udevice *dev,
				  const struct group_config *group,
				  char *func)
{
	struct npcm8xx_pinctrl_priv *priv = dev_get_priv(dev);

	dev_dbg(dev, "set_func [grp %s][func %s]\n", group->name, func);
	if (strcmp(group->name, func))
		regmap_update_bits(priv->gcr_regmap, group->reg,
				   BIT(group->bit), 0);
	else
		regmap_update_bits(priv->gcr_regmap, group->reg,
				   BIT(group->bit), BIT(group->bit));

	return 0;
}

static int npcm8xx_pinmux_set(struct udevice *dev,
			      unsigned int pin_selector,
			      unsigned int func_selector)
{
	const struct pin_info *pin;
	char *func = npcm8xx_funcs[func_selector];
	const struct group_config *group;
	int i;

	pin = &npcm8xx_pins[pin_selector];
	dev_dbg(dev, "set_mux [pin %s][func %s]\n", pin->name, func);

	for (i = 0; i < pin->num_funcs; i++) {
		group = npcm8xx_group_get(pin->alt_func[i]);
		if (!group)
			break;
		npcm8xx_group_set_func(dev, group, func);
	}

	return 0;
}

static int npcm8xx_pinmux_group_set(struct udevice *dev,
				    unsigned int group_selector,
				    unsigned int func_selector)
{
	const struct group_config *group;
	int pin_selector;
	int i;

	dev_dbg(dev, "set_mux [grp %s][func %s]\n",
		npcm8xx_groups[group_selector].name,
		npcm8xx_funcs[func_selector]);
	group = &npcm8xx_groups[group_selector];

	if (!group->npins) {
		/* No alternate GPIO pins, just set the function */
		npcm8xx_group_set_func(dev, group,
				       npcm8xx_funcs[func_selector]);
		return 0;
	}

	for (i = 0; i < group->npins; i++) {
		pin_selector = npcm8xx_get_pin_selector(group->pins[i]);
		if (pin_selector < 0) {
			dev_dbg(dev, "invalid pin %d\n", group->pins[i]);
			return -EINVAL;
		}
		npcm8xx_pinmux_set(dev, pin_selector, func_selector);
	}

	return 0;
}

static int npcm8xx_get_pins_count(struct udevice *dev)
{
	return ARRAY_SIZE(npcm8xx_pins);
}

static const char *npcm8xx_get_pin_name(struct udevice *dev,
					unsigned int selector)
{
	return npcm8xx_pins[selector].name;
}

static int npcm8xx_get_groups_count(struct udevice *dev)
{
	return ARRAY_SIZE(npcm8xx_groups);
}

static const char *npcm8xx_get_group_name(struct udevice *dev,
					  unsigned int selector)
{
	return npcm8xx_groups[selector].name;
}

static int npcm8xx_get_functions_count(struct udevice *dev)
{
	return npcm8xx_num_funcs;
}

static const char *npcm8xx_get_function_name(struct udevice *dev,
					     unsigned int selector)
{
	return npcm8xx_funcs[selector];
}

#if CONFIG_IS_ENABLED(PINCONF)
#define PIN_CONFIG_PERSIST_STATE (PIN_CONFIG_END + 1)
#define PIN_CONFIG_POLARITY_STATE (PIN_CONFIG_END + 2)
#define PIN_CONFIG_EVENT_CLEAR (PIN_CONFIG_END + 3)

static const struct pinconf_param npcm8xx_conf_params[] = {
	{ "bias-disable", PIN_CONFIG_BIAS_DISABLE, 0 },
	{ "bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 1 },
	{ "bias-pull-down", PIN_CONFIG_BIAS_PULL_DOWN, 1 },
	{ "input-enable", PIN_CONFIG_INPUT_ENABLE, 1 },
	{ "output-enable", PIN_CONFIG_OUTPUT_ENABLE, 1 },
	{ "output-high", PIN_CONFIG_OUTPUT, 1, },
	{ "output-low", PIN_CONFIG_OUTPUT, 0, },
	{ "drive-open-drain", PIN_CONFIG_DRIVE_OPEN_DRAIN, 1 },
	{ "drive-push-pull", PIN_CONFIG_DRIVE_PUSH_PULL, 1 },
	{ "persist-enable", PIN_CONFIG_PERSIST_STATE, 1 },
	{ "persist-disable", PIN_CONFIG_PERSIST_STATE, 0 },
	{ "input-debounce", PIN_CONFIG_INPUT_DEBOUNCE, 0 },
	{ "active-high", PIN_CONFIG_POLARITY_STATE, 0 },
	{ "active-low", PIN_CONFIG_POLARITY_STATE, 1 },
	{ "drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 0 },
	{ "slew-rate", PIN_CONFIG_SLEW_RATE, 0},
	{ "event-clear", PIN_CONFIG_EVENT_CLEAR, 0},
};

/* Support for retaining the state after soft reset */
static int npcm8xx_gpio_reset_persist(struct udevice *dev, uint bank,
				      uint enable)
{
	struct npcm8xx_pinctrl_priv *priv = dev_get_priv(dev);
	u8 offset = bank + GPIOX_OFFSET;

	dev_dbg(dev, "set gpio persist, bank %d, enable %d\n", bank, enable);

	if (enable) {
		regmap_update_bits(priv->rst_regmap, WD0RCR, BIT(offset), 0);
		regmap_update_bits(priv->rst_regmap, WD1RCR, BIT(offset), 0);
		regmap_update_bits(priv->rst_regmap, WD2RCR, BIT(offset), 0);
		regmap_update_bits(priv->rst_regmap, CORSTC, BIT(offset), 0);
		regmap_update_bits(priv->rst_regmap, SWRSTC1, BIT(offset), 0);
		regmap_update_bits(priv->rst_regmap, SWRSTC2, BIT(offset), 0);
		regmap_update_bits(priv->rst_regmap, SWRSTC3, BIT(offset), 0);
		regmap_update_bits(priv->rst_regmap, TIPRSTC, BIT(offset), 0);
	} else {
		regmap_update_bits(priv->rst_regmap, WD0RCR, BIT(offset),
				   BIT(offset));
		regmap_update_bits(priv->rst_regmap, WD1RCR, BIT(offset),
				   BIT(offset));
		regmap_update_bits(priv->rst_regmap, WD2RCR, BIT(offset),
				   BIT(offset));
		regmap_update_bits(priv->rst_regmap, CORSTC, BIT(offset),
				   BIT(offset));
		regmap_update_bits(priv->rst_regmap, SWRSTC1, BIT(offset),
				   BIT(offset));
		regmap_update_bits(priv->rst_regmap, SWRSTC2, BIT(offset),
				   BIT(offset));
		regmap_update_bits(priv->rst_regmap, SWRSTC3, BIT(offset),
				   BIT(offset));
		regmap_update_bits(priv->rst_regmap, TIPRSTC, BIT(offset),
				   BIT(offset));
	}

	return 0;
}

static bool is_gpio_persist(struct udevice *dev, uint bank)
{
	struct npcm8xx_pinctrl_priv *priv = dev_get_priv(dev);
	u8 offset = bank + GPIOX_OFFSET;
	u32 val;
	int status;

	status = npcm_get_reset_status();
	dev_dbg(dev, "reset status: 0x%x\n", status);

	if (status & CORST)
		regmap_read(priv->rst_regmap, CORSTC, &val);
	else if (status & WD0RST)
		regmap_read(priv->rst_regmap, WD0RCR, &val);
	else if (status & WD1RST)
		regmap_read(priv->rst_regmap, WD1RCR, &val);
	else if (status & WD2RST)
		regmap_read(priv->rst_regmap, WD2RCR, &val);
	else if (status & SW1RST)
		regmap_read(priv->rst_regmap, SWRSTC1, &val);
	else if (status & SW2RST)
		regmap_read(priv->rst_regmap, SWRSTC2, &val);
	else if (status & SW3RST)
		regmap_read(priv->rst_regmap, SWRSTC3, &val);
	else if (status & TIPRST)
		regmap_read(priv->rst_regmap, TIPRSTC, &val);
	else
		return false;

	return !(val & BIT(offset));
}

static void npcm8xx_set_gpio_func(struct udevice *dev, unsigned int selector)
{
	const struct pin_info *pin = &npcm8xx_pins[selector];
	const struct group_config *group;
	char *func = gpio_func_name;
	int i;

	/* gpio is enabled by setting alternate function */
	if (pin->flags & GPIO_ALT)
		func = pin->alt_func[GPIO_IDX_VAL(pin->flags)];

	for (i = 0; i < pin->num_funcs; i++) {
		group = npcm8xx_group_get(pin->alt_func[i]);
		if (!group)
			break;
		npcm8xx_group_set_func(dev, group, func);
	}
}

static int npcm8xx_pinconf_set(struct udevice *dev, unsigned int selector,
			       unsigned int param, unsigned int arg)
{
	struct npcm8xx_pinctrl_priv *priv = dev_get_priv(dev);
	uint pin = npcm8xx_pins[selector].gpio_num;
	uint bank = pin / NPCM8XX_GPIO_PER_BANK;
	uint gpio = (pin % NPCM8XX_GPIO_PER_BANK);
	void __iomem *base = priv->gpio_base + (0x1000 * bank);
	u32 flags = npcm8xx_pins[selector].flags;
	int ret = 0;

	dev_dbg(dev, "set_conf [pin %d][param 0x%x, arg 0x%x]\n",
		pin, param, arg);

	/* Configure pin as gpio function */
	if (param != PIN_CONFIG_SLEW_RATE)
		npcm8xx_set_gpio_func(dev, selector);

	if (is_gpio_persist(dev, bank) &&
	    param != PIN_CONFIG_EVENT_CLEAR) {
		dev_dbg(dev, "retain the state\n");
		return 0;
	}

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		dev_dbg(dev, "set pin %d bias disable\n", pin);
		clrbits_le32(base + GPIO_PU, BIT(gpio));
		clrbits_le32(base + GPIO_PD, BIT(gpio));
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		dev_dbg(dev, "set pin %d bias pull down\n", pin);
		clrbits_le32(base + GPIO_PU, BIT(gpio));
		setbits_le32(base + GPIO_PD, BIT(gpio));
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		dev_dbg(dev, "set pin %d bias pull up\n", pin);
		setbits_le32(base + GPIO_PU, BIT(gpio));
		clrbits_le32(base + GPIO_PD, BIT(gpio));
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		dev_dbg(dev, "set pin %d input enable\n", pin);
		setbits_le32(base + GPIO_OEC, BIT(gpio));
		setbits_le32(base + GPIO_IEM, BIT(gpio));
		break;
	case PIN_CONFIG_OUTPUT_ENABLE:
		dev_dbg(dev, "set pin %d output enable\n", pin);
		clrbits_le32(base + GPIO_IEM, BIT(gpio));
		setbits_le32(base + GPIO_OES, BIT(gpio));
	case PIN_CONFIG_OUTPUT:
		dev_dbg(dev, "set pin %d output %d\n", pin, arg);
		clrbits_le32(base + GPIO_IEM, BIT(gpio));
		setbits_le32(base + GPIO_OES, BIT(gpio));
		if (arg)
			setbits_le32(base + GPIO_DOUT, BIT(gpio));
		else
			clrbits_le32(base + GPIO_DOUT, BIT(gpio));
		break;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		dev_dbg(dev, "set pin %d push pull\n", pin);
		clrbits_le32(base + GPIO_OTYP, BIT(gpio));
		break;
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		dev_dbg(dev, "set pin %d open drain\n", pin);
		setbits_le32(base + GPIO_OTYP, BIT(gpio));
		break;
	case PIN_CONFIG_INPUT_DEBOUNCE:
		dev_dbg(dev, "set pin %d input debounce\n", pin);
		setbits_le32(base + GPIO_DBNC, BIT(gpio));
		break;
	case PIN_CONFIG_POLARITY_STATE:
		dev_dbg(dev, "set pin %d active %d\n", pin, arg);
		if (arg)
			setbits_le32(base + GPIO_POL, BIT(gpio));
		else
			clrbits_le32(base + GPIO_POL, BIT(gpio));
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		dev_dbg(dev, "set pin %d driver strength %d\n", pin, arg);
		if (DSLO(flags) == arg)
			clrbits_le32(base + GPIO_ODSC, BIT(gpio));
		else if (DSHI(flags) == arg)
			setbits_le32(base + GPIO_ODSC, BIT(gpio));
		else
			ret = -EOPNOTSUPP;
		break;
	case PIN_CONFIG_SLEW_RATE:
		dev_dbg(dev, "set pin %d slew rate %d\n", pin, arg);
		if (!(flags & SLEW)) {
			ret = -EOPNOTSUPP;
			break;
		}
		if (arg)
			setbits_le32(base + GPIO_OSRC, BIT(gpio));
		else
			clrbits_le32(base + GPIO_OSRC, BIT(gpio));
		break;
	case PIN_CONFIG_EVENT_CLEAR:
		dev_dbg(dev, "set pin %d event clear\n", pin);
		clrbits_le32(base + GPIO_EVEN, BIT(gpio));
		setbits_le32(base + GPIO_EVST, BIT(gpio));
		break;
	case  PIN_CONFIG_PERSIST_STATE:
		npcm8xx_gpio_reset_persist(dev, bank, arg);
		break;

	default:
		ret = -EOPNOTSUPP;
	}

	return ret;
}
#endif

static struct pinctrl_ops npcm8xx_pinctrl_ops = {
	.set_state	= pinctrl_generic_set_state,
	.get_pins_count = npcm8xx_get_pins_count,
	.get_pin_name = npcm8xx_get_pin_name,
	.get_groups_count = npcm8xx_get_groups_count,
	.get_group_name = npcm8xx_get_group_name,
	.get_functions_count = npcm8xx_get_functions_count,
	.get_function_name = npcm8xx_get_function_name,
	.pinmux_set = npcm8xx_pinmux_set,
	.pinmux_group_set = npcm8xx_pinmux_group_set,
#if CONFIG_IS_ENABLED(PINCONF)
	.pinconf_num_params = ARRAY_SIZE(npcm8xx_conf_params),
	.pinconf_params = npcm8xx_conf_params,
	.pinconf_set = npcm8xx_pinconf_set,
	.pinconf_group_set = npcm8xx_pinconf_set,
#endif
};

static int npcm8xx_pinctrl_probe(struct udevice *dev)
{
	struct npcm8xx_pinctrl_priv *priv = dev_get_priv(dev);
	int i;

	priv->gpio_base = dev_read_addr_ptr(dev);
	if (!priv->gpio_base)
		return -EINVAL;

	priv->gcr_regmap = syscon_regmap_lookup_by_phandle(dev, "syscon-gcr");
	if (IS_ERR(priv->gcr_regmap))
		return -EINVAL;

	priv->rst_regmap = syscon_regmap_lookup_by_phandle(dev, "syscon-rst");
	if (IS_ERR(priv->rst_regmap))
		return -EINVAL;

	/* initialize function names, all group functions + gpio function */
	npcm8xx_num_funcs = ARRAY_SIZE(npcm8xx_groups) + 1;
	npcm8xx_funcs = malloc(npcm8xx_num_funcs * sizeof(char *));
	if (!npcm8xx_funcs)
		return -ENOMEM;
	npcm8xx_funcs[0] = gpio_func_name;
	for (i = 1; i < npcm8xx_num_funcs; i++)
		npcm8xx_funcs[i] = npcm8xx_groups[i - 1].name;

	return 0;
}

static const struct udevice_id npcm8xx_pinctrl_ids[] = {
	{ .compatible = "nuvoton,npcm845-pinctrl" },
	{ }
};

U_BOOT_DRIVER(pinctrl_npcm8xx) = {
	.name = "nuvoton_npcm8xx_pinctrl",
	.id = UCLASS_PINCTRL,
	.of_match = npcm8xx_pinctrl_ids,
	.priv_auto = sizeof(struct npcm8xx_pinctrl_priv),
	.ops = &npcm8xx_pinctrl_ops,
	.probe = npcm8xx_pinctrl_probe,
};
