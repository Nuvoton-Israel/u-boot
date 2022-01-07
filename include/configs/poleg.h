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
//#define CONFIG_LAST_STAGE_INIT
#define CONFIG_ENV_OVERWRITE

#define CONFIG_MACH_TYPE		        MACH_TYPE_NPCMX50

#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_CMDLINE_TAG
#define CONFIG_INITRD_TAG

#define CONFIG_SYS_USB_OHCI_MAX_ROOT_PORTS   1
#define CONFIG_USB_OHCI_NEW

#ifdef  CONFIG_SYS_PROMPT
#undef  CONFIG_SYS_PROMPT
#define CONFIG_SYS_PROMPT               "U-Boot>"
#endif

#define CONFIG_ENV_SIZE                  0x40000
//#define CONFIG_ENV_OFFSET               (0x100000)
//#define CONFIG_ENV_ADDR                 (0x80000000 + CONFIG_ENV_OFFSET)
#define CONFIG_ENV_SECT_SIZE            0x4000

#ifndef CONFIG_SYS_L2CACHE_OFF
#define CONFIG_SYS_L2_PL310		1
#define CONFIG_SYS_PL310_BASE	0xF03FC000       /* L2 - Cache Regs Base (4k Space)*/
#endif
#undef CONFIG_SYS_CACHELINE_SIZE
#define CONFIG_SYS_CACHELINE_SIZE      32

#define CONFIG_SYS_MALLOC_LEN           (CONFIG_ENV_SIZE + 2048*1024)

#define CONFIG_SYS_MAXARGS              32
#define CONFIG_SYS_CBSIZE               256
#define CONFIG_SYS_PBSIZE               (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_PROMPT_HUSH_PS2	    "> "

#define CONFIG_SYS_BOOTMAPSZ            (0x30 << 20)
#define CONFIG_SYS_LOAD_ADDR            0x8000
#define CONFIG_SYS_SDRAM_BASE           0x0
#define CONFIG_SYS_INIT_SP_ADDR         (0x00008000 - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_MONITOR_LEN          (256 << 10) /* Reserve 256 kB for Monitor   */
#define CONFIG_SYS_MONITOR_BASE	        CONFIG_SYS_TEXT_BASE

#define CONFIG_SYS_MEMTEST_START	CONFIG_SYS_SDRAM_BASE
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + 0x08000000)

#define CONFIG_STANDALONE_LOAD_ADDR     0x10000000

#define SPI_FLASH_BASE_ADDR				0x80000000
#define SPI_FLASH_REGION_SIZE			0x08000000	/* 128MB */
#define SPI0_BASE_ADDR					0x80000000
#define SPI0_END_ADDR					0x8FFFFFFF
#define SPI3_BASE_ADDR					0xA0000000
#define SPI3_END_ADDR					0xBFFFFFFF

#ifndef CONFIG_SYS_MEM_TOP_HIDE
/* 16MB Graphics Memory size to hide + 32MB for VCD ECE DVC. */
#define CONFIG_SYS_MEM_TOP_HIDE   ((16 << 20) + (32 << 20))
#endif
#define PHYS_SDRAM_1			        CONFIG_SYS_SDRAM_BASE

#define CONFIG_BAUDRATE                 115200
#define CONFIG_SYS_BAUDRATE_TABLE       {115200, 57600, 38400}

#define CONFIG_SYS_HZ                   1000

/* Default environemnt variables */
#define CONFIG_BOOTCOMMAND "run common_bootargs; run romboot"
#define CONFIG_SERVERIP                 192.168.0.1
#define CONFIG_IPADDR                   192.168.0.2
#define CONFIG_NETMASK                  255.255.255.0
#define CONFIG_ETHADDR                  "00:00:F7:A0:FF:FC"
#define CONFIG_HAS_ETH1
#define CONFIG_ETH1ADDR                 "00:00:F7:A0:FF:FD"
#define CONFIG_HAS_ETH2
#define CONFIG_ETH2ADDR                 "00:00:F7:A0:FF:FE"
#define CONFIG_HAS_ETH3
#define CONFIG_ETH3ADDR                 "00:00:F7:A0:FF:FF"
#define CONFIG_EXTRA_ENV_SETTINGS   "uimage_flash_addr=80200000\0"   \
		"stdin=serial\0"   \
		"stdout=serial\0"   \
		"stderr=serial\0"    \
		"ethact=eth${eth_num}\0"   \
		"romboot=echo Booting Kernel from flash; echo +++ uimage at 0x${uimage_flash_addr}; " \
		"echo Using bootargs: ${bootargs};bootm ${uimage_flash_addr}\0" \
		"autostart=yes\0"   \
		"eth_num=0\0"    \
		"common_bootargs=setenv bootargs earlycon=${earlycon} root=/dev/ram console=${console} mem=${mem} ramdisk_size=48000 basemac=${ethaddr} oops=panic panic=20\0"   \
		"ftp_prog=setenv ethact eth${eth_num}; dhcp; tftp 10000000 image-bmc; cp.b 10000000 80000000 ${filesize}\0"   \
		"ftp_run=setenv ethact eth${eth_num}; dhcp; tftp 10000000 image-bmc; bootm 10200000\0"   \
		"sd_prog=fatload mmc 0 10000000 image-bmc; cp.b 10000000 80000000 ${filesize}\0"  \
		"sd_run=fatload mmc 0 10000000 image-bmc; bootm 10200000\0"   \
		"usb_prog=usb start; fatload usb 0 10000000 image-bmc; cp.b 10000000 80000000 ${filesize}\0"    \
		"usb_run=usb start; fatload usb 0 10000000 image-bmc; bootm 10200000\0"   \
		"\0"

#endif
