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

#ifndef __NPCMX50_GCR_H_
#define __NPCMX50_GCR_H_

#if defined (CONFIG_TARGET_POLEG)
#include <asm/arch/poleg_gcr.h>
#else
#error "no target board defined!"
#endif

struct npcm750_gcr {
	unsigned int  pdid;
	unsigned int  pwron;
	unsigned char res1[0x4];
	unsigned int  mfsel1;
	unsigned int  mfsel2;
	unsigned int  miscpe;
	unsigned char res2[0x20];
	unsigned int  spswc;
	unsigned int  intcr;
	unsigned int  intsr;
	unsigned char res3[0xc];
	unsigned int  hifcr;
	unsigned int  sd1irv1;
	unsigned int  sd1irv2;
	unsigned char res4[0x4];
	unsigned int  intcr2;
	unsigned int  mfsel3;
	unsigned int  srcnt;
	unsigned int  ressr;
	unsigned int  rlockr1;
	unsigned int  flockr1;
	unsigned int  dscnt;
	unsigned int  mdlr;
	unsigned char res5[0x18];
	unsigned int  davclvlr;
	unsigned int  intcr3;
	unsigned char res6[0xc];
	unsigned int  vsintr;
	unsigned int  mfsel4;
	unsigned int  sd2irv1;
	unsigned int  sd2irv2;
	unsigned char res7[0x8];
	unsigned int  cpbpntr;
	unsigned char res8[0x8];
	unsigned int  cpctl;
	unsigned int  cp2bst;
	unsigned int  b2cpnt;
	unsigned int  cppctl;
	unsigned int  i2csegsel;
	unsigned int  i2csegctl;
	unsigned int  vsrcr;
	unsigned int  mlockr;
	unsigned char res9[0x4c];
	unsigned int  scrpad;
	unsigned int  usb1phyctl;
	unsigned int  usb2phyctl;
};

#endif
