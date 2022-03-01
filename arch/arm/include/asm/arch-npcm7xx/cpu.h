/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#ifndef __NPCMX50_CPU_H_
#define __NPCMX50_CPU_H_

#define NPCM750_GCR_BA			0xF0800000
#define NPCM750_CLK_BA			0xF0801000
#define NPCM750_GPIO_BA			0xF0010000

#ifndef __ASSEMBLY__

#define NPCM750_BASE(device, base) \
static inline unsigned long __attribute__((no_instrument_function)) \
	npcm_get_base_##device(void) \
{ \
	return NPCM750_##base; \
}

NPCM750_BASE(gcr, GCR_BA)
NPCM750_BASE(clk, CLK_BA)
NPCM750_BASE(gpio, GPIO_BA)

#endif

#endif
