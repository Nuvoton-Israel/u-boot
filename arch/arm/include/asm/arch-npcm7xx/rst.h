/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _NPCM_RST_H_
#define _NPCM_RST_H_

#define WTCR0_REG       0xF000801C
#define WTCR_WTR        BIT(0)
#define WTCR_WTRE       BIT(1)
#define WTCR_WTE        BIT(7)

enum reset_type {
	PORST_TYPE    = 0x01,
	CORST_TYPE    = 0x02,
	WD0RST_TYPE   = 0x03,
	SWR1ST_TYPE   = 0x04,
	SWR2ST_TYPE   = 0x05,
	SWR3ST_TYPE   = 0x06,
	SWR4ST_TYPE   = 0x07,
	WD1RST_TYPE   = 0x08,
	WD2RST_TYPE   = 0x09,
	UNKNOWN_TYPE  = 0x10,
};

#define PORST		BIT(31)
#define CORST		BIT(30)
#define WD0RST		BIT(29)
#define SW1RST		BIT(28)
#define SW2RST		BIT(27)
#define SW3RST		BIT(26)
#define TIPRST		BIT(25)
#define WD1RST		BIT(24)
#define WD2RST		BIT(23)

enum reset_type npcm7xx_reset_reason(void);

#endif

