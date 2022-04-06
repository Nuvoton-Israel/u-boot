/*
 *  Copyright (c) 2016 Nuvoton Technology Corp.
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
#include <asm/io.h>
#include <asm/arch/rst.h>
#include <asm/arch/gcr.h>
#include <asm/arch/poleg_info.h>
#include <asm/arch/cpu.h>

void reset_cpu(ulong ignored)
{
	writel(WTCR_WTR | WTCR_WTRE | WTCR_WTE, WTCR0_REG);
	while (1);
}

void reset_misc(void)
{
	printf("clear WDC\n");
	writel(readl(0xf0800060) & ~(1 << 21), 0xf0800060);
}

enum reset_type npcm7xx_reset_reason(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	enum reset_type type = UNKNOWN_TYPE;
    u32 value = readl(&gcr->ressr);

	if (value == 0)
		value = ~readl(&gcr->intcr2);

	if (value & CORST)
		type = CORST_TYPE;
	if (value & WD0RST)
		type = WD0RST_TYPE;
	if (value & WD1RST)
		type = WD1RST_TYPE;
	if (value & WD2RST)
		type = WD2RST_TYPE;
	if (value & PORST)
		type = PORST_TYPE;

	return type;
}
