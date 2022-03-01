/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#ifndef _NPCM_FIU_H_
#define _NPCM_FIU_H_

#define CHUNK_SIZE		    16

/* FIU UMA data size */
enum _spi_uma_data_size {
	FIU_UMA_DATA_SIZE_0	    = 0,
	FIU_UMA_DATA_SIZE_1	    = 1,
	FIU_UMA_DATA_SIZE_2	    = 2,
	FIU_UMA_DATA_SIZE_3	    = 3,
	FIU_UMA_DATA_SIZE_4	    = 4,
	FIU_UMA_DATA_SIZE_5	    = 5,
	FIU_UMA_DATA_SIZE_6	    = 6,
	FIU_UMA_DATA_SIZE_7	    = 7,
	FIU_UMA_DATA_SIZE_8	    = 8,
	FIU_UMA_DATA_SIZE_9	    = 9,
	FIU_UMA_DATA_SIZE_10	    = 10,
	FIU_UMA_DATA_SIZE_11	    = 11,
	FIU_UMA_DATA_SIZE_12	    = 12,
	FIU_UMA_DATA_SIZE_13	    = 13,
	FIU_UMA_DATA_SIZE_14	    = 14,
	FIU_UMA_DATA_SIZE_15	    = 15,
	FIU_UMA_DATA_SIZE_16	    = 16
};

/* FIU UMA Configuration Register (FIU_UMA_CFG) */
#define FIU_UMA_CFG_LCK                 31
#define FIU_UMA_CFG_CMMLCK              30
#define FIU_UMA_CFG_RDATSIZ             24
#define FIU_UMA_CFG_DBSIZ               21
#define FIU_UMA_CFG_WDATSIZ             16
#define FIU_UMA_CFG_ADDSIZ              11
#define FIU_UMA_CFG_CMDSIZ              10
#define FIU_UMA_CFG_RDBPCK              8
#define FIU_UMA_CFG_DBPCK               6
#define FIU_UMA_CFG_WDBPCK              4
#define FIU_UMA_CFG_ADBPCK              2
#define FIU_UMA_CFG_CMBPCK              0

/* FIU UMA Control and Status Register (FIU_UMA_CTS) */
#define FIU_UMA_CTS_RDYIE		25
#define FIU_UMA_CTS_RDYST		24
#define FIU_UMA_CTS_SW_CS		16
#define FIU_UMA_CTS_DEV_NUM		8
#define FIU_UMA_CTS_EXEC_DONE		0

/* FIU UMA Command Register (FIU_UMA_CMD) */
#define FIU_UMA_CMD_DUM3		24
#define FIU_UMA_CMD_DUM2		16
#define FIU_UMA_CMD_DUM1		8
#define FIU_UMA_CMD_CMD			0

#define FIU_DEVICES_PER_MODULE		    4

struct npcm_fiu_regs {
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

#endif
