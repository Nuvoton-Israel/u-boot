/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#ifndef _NPCM_GPIO_H_
#define _NPCM_GPIO_H_

struct npcm_gpio_regs {
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
