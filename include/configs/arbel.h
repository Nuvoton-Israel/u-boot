/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#ifndef __CONFIG_ARBEL_H
#define __CONFIG_ARBEL_H

#define CONFIG_GICV2
#define GICD_BASE			(0xDFFF9000)
#define GICC_BASE			(0xDFFFA000)
#define CONFIG_SYS_USB_OHCI_MAX_ROOT_PORTS   1
#define CONFIG_USB_OHCI_NEW
#define CONFIG_SYS_MALLOC_LEN           (CONFIG_ENV_SIZE + 2048*1024)
#define CONFIG_SYS_MAXARGS              32
#define CONFIG_SYS_CBSIZE               256
#define CONFIG_SYS_PBSIZE               (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_BOOTM_LEN            (20 << 20)
#define CONFIG_SYS_BOOTMAPSZ            (192 << 20)
#define CONFIG_SYS_LOAD_ADDR            CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_SDRAM_BASE           0x0
#define CONFIG_SYS_INIT_SP_ADDR         CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_MONITOR_LEN          (256 << 10)
#define CONFIG_SYS_MONITOR_BASE	        CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_HZ                   1000
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 14400, 19200, 38400, 57600, 115200, 230400, \
					  380400, 460800, 921600 }
#define CONFIG_BITBANGMII_MULTI

/* NPCM specific defines */
#define SPI_FLASH_BASE_ADDR		0x80000000
#define SPI_FLASH_REGION_SIZE		0x08000000	/* 128MB */
#define SPI1_FLASH_REGION_SIZE		0x01000000	/* 16MB */
#define SPI0_BASE_ADDR			0x80000000
#define SPI0_END_ADDR			0x8FFFFFFF
#define SPI1_BASE_ADDR			0x90000000
#define SPI1_END_ADDR			0x93FFFFFF
#define SPI3_BASE_ADDR			0xA0000000
#define SPI3_END_ADDR			0xBFFFFFFF

/* Default environemnt variables */
#define CONFIG_BOOTCOMMAND "run common_bootargs; run romboot"
#define CONFIG_EXTRA_ENV_SETTINGS   "uimage_flash_addr=80400000\0"   \
		"stdin=serial\0"   \
		"stdout=serial\0"   \
		"stderr=serial\0"    \
		"ethact=gmac1\0"   \
		"autostart=no\0"   \
		"ethaddr=00:00:F7:A0:00:FC\0"    \
		"eth1addr=00:00:F7:A0:00:FD\0"   \
		"eth2addr=00:00:F7:A0:00:FE\0"    \
		"eth3addr=00:00:F7:A0:00:FF\0"    \
		"gatewayip=192.168.0.17\0"    \
		"serverip=192.168.0.17\0"    \
		"ipaddr=192.168.0.15\0"    \
		"gmacloopb=0\0"    \
		"eth_num=0\0"    \
		"romboot=echo Booting Kernel from flash at 0x${uimage_flash_addr}; " \
		"echo Using bootargs: ${bootargs};bootm ${uimage_flash_addr}\0" \
		"earlycon=uart8250,mmio32,0xf0000000\0" \
		"console=ttyS0,115200n8\0" \
		"common_bootargs=setenv bootargs earlycon=${earlycon} root=/dev/ram " \
		"console=${console} mem=${mem} ramdisk_size=48000 oops=panic panic=20\0" \
		"ftp_prog=setenv ethact eth${eth_num}; dhcp; tftp 10000000 image-bmc; cp.b 10000000 80000000 ${filesize}\0"   \
		"ftp_run=setenv ethact eth${eth_num}; dhcp; tftp 10000000 image-bmc; bootm 10200000\0"   \
		"usb_prog=usb start; fatload usb 0 10000000 image-bmc; cp.b 10000000 80000000 ${filesize}\0"    \
		"usb_run=usb start; fatload usb 0 10000000 image-bmc; bootm 10200000\0"   \
		"\0"

#endif
