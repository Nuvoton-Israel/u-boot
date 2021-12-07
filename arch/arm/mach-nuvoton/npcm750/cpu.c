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
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/armv7.h>

int print_cpuinfo (void)
{
	struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();
	unsigned int id, mdlr;

	mdlr = readl(&gcr->mdlr);

	printf("CPU: ");

	switch(mdlr) {
	case POLEG_NPCM750:
		printf("NPCM750 ");
		break;
	case POLEG_NPCM730:
		printf("NPCM730 ");
		break;
	case POLEG_NPCM710:
		printf("NPCM710 ");
		break;
	default:
		printf("NPCM7XX ");
		break;
	}

	id = readl(&gcr->pdid);
	switch(id) {
	case POLEG_Z1:
		printf("Z1 is no supported! @ ");
		break;
	case POLEG_A1:
		printf("A1 @ ");
		break;
	default:
		printf("Unknown\n");
		break;
	}

	return 0;
}

void s_init(void)
{
	/* Invalidate L2 cache in lowlevel_init */
	v7_outer_cache_inval_all();
}

