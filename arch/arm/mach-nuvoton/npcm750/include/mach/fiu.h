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

#ifndef __NPCMX50_FIU_H_
#define __NPCMX50_FIU_H_
#ifndef __ASSEMBLY__

#if defined (CONFIG_TARGET_POLEG)
#include <asm/arch/poleg_fiu.h>
#else
#error "no target board defined!"
#endif

#define FIU_DEVICES_PER_MODULE		    4

enum fiu_moudle_tag {
	FIU_MODULE_0,
	FIU_MODULE_1,
	FIU_MODULE_2,
	FIU_MODULE_3,
	FIU_MODULE_X
};


struct npcm750_fiu_regs {
	unsigned int    drd_cfg;
	unsigned int    dwr_cfg;
	unsigned int    uma_cfg;
	unsigned int    uma_cts;
	unsigned int    uma_cmd;
	unsigned int    uma_addr;
	unsigned int    prt_cfg;
	unsigned char	res1[4];
	unsigned int    uma_dw0;
	unsigned int    uma_dw1;
	unsigned int    uma_dw2;
	unsigned int    uma_dw3;
	unsigned int    uma_dr0;
	unsigned int    uma_dr1;
	unsigned int    uma_dr2;
	unsigned int    uma_dr3;
	unsigned int    prt_cmd0;
	unsigned int    prt_cmd1;
	unsigned int    prt_cmd2;
	unsigned int    prt_cmd3;
	unsigned int    prt_cmd4;
	unsigned int    prt_cmd5;
	unsigned int    prt_cmd6;
	unsigned int    prt_cmd7;
	unsigned int    prt_cmd8;
	unsigned int    prt_cmd9;
	unsigned int    stuff[4];
	unsigned int    fiu_cfg;
};

#endif /* __ASSEMBLY__ */
#endif
