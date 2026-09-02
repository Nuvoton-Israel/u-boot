#ifndef SHARED_DEFS_H
#define SHARED_DEFS_H

#include <configs/poleg.h>

#define FLASH_BOOT_ALTERNATIVES  2

/*------------------------------------------------------*/
/* Boot module exported definitions                     */
/*------------------------------------------------------*/
#define START_TAG_SIZE           8     // bytes
//                                       0xAA550750        T     O     O     B
#define START_TAG_ARR_BOOTBLOCK {0x50, 0x07, 0x55, 0xAA, 0x54, 0x4F, 0x4F, 0x42}
//                                 U     B     O     O     T     B     L     K
#define START_TAG_ARR_UBOOT     {0x55, 0x42, 0x4F, 0x4F, 0x54, 0x42, 0x4C, 0x4B}

#define FLASH_EXAM_ADDR_1       (SPI0_BASE_ADDR)
#define FLASH_EXAM_ADDR_2       (SPI0_BASE_ADDR + 0x80000)
#define FLASH_EXAM_ADDR_3       (SPI0_BASE_ADDR + SPI_FLASH_REGION_SIZE)

#define KBOLD_ON                "\x1b[1m"
#define KBOLD_OFF               "\x1b[22m"
#define KNRM                    "\x1B[0m" KBOLD_OFF
#define KRED                    "\x1B[31m" KBOLD_ON

#endif
