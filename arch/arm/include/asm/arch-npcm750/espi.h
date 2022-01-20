/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _NPCM_ESPI_H_
#define _NPCM_ESPI_H_

#define NPCM_ESPI_BA			  0xF009F000
#define ESPICFG					      0x04
#define ESPIHINDP             0x80
#define ESPICFG_CHNSUPP_MASK	0x0F
#define ESPICFG_CHNSUPP_SHFT	24

#define ESPICFG_IOMODE        8
#define ESPICFG_MAXFREQ       10
#define ESPICFG_RSTO          31

#define NPCM_SHM_BA        0XC0001000
#define SMC_CTL               0x01

#define HOSTWAIT              7

#endif
