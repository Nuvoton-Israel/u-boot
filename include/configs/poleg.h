/*
 * Copyright (c) 2016 Nuvoton Technology Corp.
 *
 * Configuration settings for the NUVOTON POLEG board.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_POLEG_H
#define __CONFIG_POLEG_H

#undef  CONFIG_USE_IRQ

#define CONFIG_ARCH_CPU_INIT
#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_SKIP_LOWLEVEL_INIT

#define CONFIG_MACH_TYPE		        MACH_TYPE_NPCMX50

#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_CMDLINE_TAG
#define CONFIG_INITRD_TAG

#ifdef  CONFIG_SYS_PROMPT
#undef  CONFIG_SYS_PROMPT
#define CONFIG_SYS_PROMPT               "U-Boot>"
#endif

#define CONFIG_ENV_SIZE                  0x10000
#define CONFIG_ENV_OFFSET               (0x100000)
#define CONFIG_ENV_ADDR                 (0x80000000 + CONFIG_ENV_OFFSET)
#define CONFIG_ENV_SECT_SIZE            0x1000

#define CONFIG_SYS_MALLOC_LEN           (CONFIG_ENV_SIZE + 1024*1024)

#define CONFIG_SYS_MAXARGS              16
#define CONFIG_SYS_CBSIZE               256
#define CONFIG_SYS_PBSIZE               (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_PROMPT_HUSH_PS2	    "> "

#define CONFIG_SYS_BOOTMAPSZ            (20 << 20)
#define CONFIG_SYS_LOAD_ADDR            0x8000
#define CONFIG_SYS_SDRAM_BASE           0x0
#define CONFIG_SYS_INIT_SP_ADDR         (0x00008000 - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_MONITOR_LEN          (256 << 10) /* Reserve 256 kB for Monitor   */
#define CONFIG_SYS_MONITOR_BASE	        CONFIG_SYS_TEXT_BASE

#define CONFIG_STANDALONE_LOAD_ADDR     0x10000000

#define SPI_FLASH_BASE_ADDR				0x80000000
#define SPI_FLASH_SIZE					0x4000000

/* 16MB Graphics Memory size to hide + 32MB for VCD ECE DVC. */
#define CONFIG_SYS_MEM_TOP_HIDE   ((16 << 20) + (32 << 20))
#define PHYS_SDRAM_1			        CONFIG_SYS_SDRAM_BASE

#define CONFIG_BAUDRATE                 115200
#define CONFIG_SYS_BAUDRATE_TABLE       {115200, 57600, 38400}

#define CONFIG_SYS_HZ                   1000

#if 0
#ifdef CONFIG_SYS_USE_SPIFLASH
#define CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_ENV_SECT_SIZE            0x1000
#define CONFIG_SYS_MAX_FLASH_BANKS      1
#else
#define CONFIG_ENV_IS_IN_FLASH
#define CONFIG_SYS_MAX_FLASH_BANKS      1
#define CONFIG_SYS_MAX_FLASH_SECT       4096
#endif

/* ETH configuration */
#define CONFIG_IPADDR                   192.168.234.129
#define CONFIG_NETMASK                  255.255.0.0
#define CONFIG_GATEWAYIP                192.168.234.130
#define CONFIG_SERVERIP                 192.168.234.130
#define CONFIG_HOSTNAME                 "poleg"
#define CONFIG_ETH_NPCM750
#define CONFIG_MII
#define CONFIG_PHYLIB
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_NET_RANDOM_ETHADDR
#define CONFIG_PHY_BROADCOM
#define CONFIG_NETCONSOLE

/* ETH GMAC configuration */
#define CONFIG_TFTP_BLOCKSIZE 1024
#define CONFIG_PHY_GIGE
#define CONFIG_ETH_DESIGNWARE
#define CONFIG_ETH_GMAC_NPCM750

/* SPI */
#define CONFIG_ENV_SPI_CS               0
#define CONFIG_SYS_MAX_FLASH_BANKS      1
#endif

#define CONFIG_GENERIC_MMC

#endif
