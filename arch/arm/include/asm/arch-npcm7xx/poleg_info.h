/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __POLEG_INFO_H_
#define __POLEG_INFO_H_

#include <spi_flash.h>

#define UBOOT_RAM_IMAGE_ADDR            (0x8000)

#define HEADER_ADDR_OFFSET              (0x140)
#define HEADER_SIZE_OFFSET              (0x144)
#define HEADER_VERSION_OFFSET           (0x148)

#define HEADER_SIZE                     (0x200)

#define POLEG_BB_HEADER_FIELD_ADDR(offset)      ((SPI_FLASH_BASE_ADDR) + (offset))
#define POLEG_BOOTBLOCK_CODE                    ((SPI_FLASH_BASE_ADDR) + (HEADER_SIZE))
#define POLEG_BOOTBLOCK_SIZE                    (*(u32 *)(POLEG_BB_HEADER_FIELD_ADDR(HEADER_SIZE_OFFSET)))
#define POLEG_BOOTBLOCK_END                     (ALIGN(((POLEG_BOOTBLOCK_CODE) + (POLEG_BOOTBLOCK_SIZE)), 0x1000))

#define POLEG_UBOOT_HEADER_FIELD_ADDR(offset)   ((POLEG_BOOTBLOCK_END) + (offset))
#define POLEG_UBOOT_CODE                        ((POLEG_BOOTBLOCK_END) + (HEADER_SIZE))
#define POLEG_UBOOT_SIZE                        (*(u32 *)(POLEG_UBOOT_HEADER_FIELD_ADDR(HEADER_SIZE_OFFSET)))
#define POLEG_UBOOT_END                         (ALIGN(((POLEG_UBOOT_CODE) + (POLEG_UBOOT_SIZE)), 0x1000))

#endif
