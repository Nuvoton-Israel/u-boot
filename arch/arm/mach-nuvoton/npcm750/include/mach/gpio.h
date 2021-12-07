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
#ifndef __NPCMX50_GPIO_H_
#define __NPCMX50_GPIO_H_

#if defined (CONFIG_TARGET_POLEG)
#include <asm/arch/poleg_gpio.h>
#else
#error "no target board defined!"
#endif

struct npcm750_gpio_regs {
	unsigned int  gpn_tlock1;
	unsigned int  gpn_din;
	unsigned int  gpn_pol;
	unsigned int  gpn_dout;
	unsigned int  gpn_oe;
	unsigned int  gpn_otyp;
	unsigned int  gpn_mp;
	unsigned int  gpn_pu;
	unsigned int  gpn_pd;
	unsigned int  gpn_dbnc;
	unsigned int  gpn_evtyp;
	unsigned int  gpn_evbe;
	unsigned int  gpn_obl0;
	unsigned int  gpn_obl1;
	unsigned int  gpn_obl2;
	unsigned int  gpn_obl3;
	unsigned int  gpn_even;
	unsigned int  gpn_evens;
	unsigned int  gpn_evenc;
	unsigned int  gpn_evst;
	unsigned int  gpn_splck;
	unsigned int  gpn_mplck;
	unsigned int  gpn_iem;
	unsigned int  gpn_osrc;
	unsigned int  gpn_odsc;
	unsigned char res1[4];
	unsigned int  gpn_dos;
	unsigned int  gpn_doc;
	unsigned int  gpn_oes;
	unsigned int  gpn_oec;
	unsigned int  gpn_tlock2;
};

#endif
