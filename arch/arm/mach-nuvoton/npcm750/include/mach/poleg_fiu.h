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

#ifndef __POLEG_FIU_H_
#define __POLEG_FIU_H_

#define CHUNK_SIZE		    16

enum _spi_trans_status
{
	FIU_TRANS_STATUS_DONE	    = 0,
	FIU_TRANS_STATUS_IN_PROG    = 1
};

/* FIU UMA data size */
enum _spi_uma_data_size
{
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

#endif
