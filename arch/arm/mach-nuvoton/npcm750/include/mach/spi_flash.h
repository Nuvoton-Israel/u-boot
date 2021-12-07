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

#ifndef __NPCMX50_SPI_FLASH_H_
#define __NPCMX50_SPI_FLASH_H_

/* Common SPI flash commands */
#define SPI_WRITE_STATUS_REG_CMD    0x01
#define SPI_PAGE_PRGM_CMD           0x02
#define SPI_READ_DATA_CMD           0x03
#define SPI_WRITE_DISABLE_CMD       0x04
#define SPI_READ_STATUS_REG_CMD     0x05
#define SPI_WRITE_ENABLE_CMD        0x06
#define SPI_4K_SECTOR_ERASE_CMD     0x20
#define SPI_32K_BLOCK_ERASE_CMD	    0x52
#define SPI_READ_JEDEC_ID_CMD       0x9F
#define SPI_CHIP_ERASE_CMD          0xC7
#define SPI_EXTNADDR_WREAR_CMD		0xC5
#define SPI_EXTNADDR_RDEAR_CMD		0xC8
#define SPI_64K_BLOCK_ERASE_CMD		0xD8

#endif
