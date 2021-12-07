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

#ifndef __NPCMX50_PSPI_H_
#define __NPCMX50_PSPI_H_

#if defined (CONFIG_TARGET_POLEG)
#include <asm/arch/poleg_pspi.h>
#else
#error "no target board defined!"
#endif

enum pspi_transfer_mode {
	PSPI_WRITE_RISING_EDGE,
	PSPI_WRITE_FALLING_EDGE,
};

enum pspi_dev {
	PSPI1_DEV = 0,
	PSPI2_DEV = 1,
};

struct npcm750_pspi_regs {
	unsigned short pspi_data;
	unsigned short pspi_ctl1;
	unsigned char pspi_stat;
};

#endif
