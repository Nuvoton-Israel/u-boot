/*
 * Copyright (c) 2016 Nuvoton Technology Corp.
 *
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/mach-types.h>
#include <asm/arch/clock.h>
#include <asm/arch/poleg_otp.h>
#include <asm/arch/poleg_info.h>
#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <clk.h>
#include <fuse.h>
#include <spi_flash.h>
#include <spi.h>

DECLARE_GLOBAL_DATA_PTR;


static bool is_security_enabled(void)
{
	u32 val = readl(FUSTRAP);
	if( val & FUSTRAP_O_SECBOOT ){
		printf("Security is enabled\n");
		return true;
	}else{
		printf("Security is NOT enabled\n");
		return false;
	}
}

static int secure_boot_configuration(void)
{
#if defined(CONFIG_SPI_FLASH) && defined(SPI_FLASH_BASE_ADDR)

	const u8 tag[SA_TAG_FLASH_IMAGE_SIZE] = SA_TAG_FLASH_IMAGE_VAL;
	struct spi_flash *flash;
	struct udevice *udev;
	u32 addr, addr_align;
	int rc , i, offset;
	u8 *buf = NULL;

	// OTP can be programmed only in Basic mode
	if (is_security_enabled())
		return 0;

	rc = spi_flash_probe_bus_cs(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
			CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE, &udev);
	if (rc)
		return rc;

	flash = dev_get_uclass_priv(udev);
	if (!flash)
		return -1;

	// fuse images should be a part of the flash image, right after the uboot
	addr = POLEG_UBOOT_END;

	// if found, program the image to the fuse arrays, set the secure boot
	// bit and erase the image from the flash
	if (((u32*)(addr + SA_TAG_FLASH_IMAGE_OFFSET))[0] == ((u32*)tag)[0] &&
		((u32*)(addr + SA_TAG_FLASH_IMAGE_OFFSET))[1] == ((u32*)tag)[1]) {

		u8 fuse_arrays[2 * NPCM750_OTP_ARR_BYTE_SIZE];
		u32 fustrap_orig;

		printf("%s(): fuse array image was found on flash in address 0x%x\n", __func__, addr);

		memcpy(fuse_arrays, (u8*)addr, sizeof(fuse_arrays));

		fustrap_orig = *(u32*)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET);

		//TODO: Here, randomize 4 AES keys + generate their nibble parity + embed to image

		printf("%s(): program fuse key array from address 0x%x\n", __func__, addr + SA_KEYS_FLASH_IMAGE_OFFSET);

		rc = fuse_prog_image(NPCM750_KEY_SA, (u32)(fuse_arrays + SA_KEYS_FLASH_IMAGE_OFFSET));
		if (rc != 0)
			return rc;

		// clear oSecBoot, will be programmed only after everything is
		// programmed successfuly
		fustrap_orig = *(u32*)(addr + SA_FUSE_FLASH_IMAGE_OFFSET);
		*(u32*)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET) &= ~FUSTRAP_O_SECBOOT;

		printf("%s(): program fuse strap array from address 0x%x\n", __func__, addr + SA_FUSE_FLASH_IMAGE_OFFSET);

		rc = fuse_prog_image(NPCM750_FUSE_SA, (u32)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET));
		if (rc != 0)
			return rc;

		// erase the whole sector
		addr_align = addr & ~(u32)(flash->erase_size -1);

		offset = addr - addr_align;
		printf("%s(): erase the sector of addr 0x%x\n", __func__, addr_align);

		addr_align -= SPI_FLASH_BASE_ADDR;

		buf = (u8 *)malloc(flash->erase_size);
		if (buf) {
			spi_flash_read(flash, addr_align, flash->erase_size, buf);
		} else {
			printf("%s(): failed to alloc buffer, skip otp program\n", __func__);
			return -1;
		}

		for (i = 0 ; i < (SA_TAG_FLASH_IMAGE_OFFSET + SA_TAG_FLASH_IMAGE_SIZE) ; i++)
			buf[offset + i] = 0xff;

		rc = spi_flash_erase(flash, addr_align, flash->erase_size);
		if (rc != 0)
			return rc;

		rc = spi_flash_write(flash, addr_align, flash->erase_size, buf);
		if (rc != 0)
			return rc;

		free(buf);

		// programm SECBOOT bit if required
		if (fustrap_orig & FUSTRAP_O_SECBOOT) {
			printf("%s(): program secure boot bit to FUSTRAP\n", __func__);
			rc = fuse_program_data(NPCM750_FUSE_SA, 0, (u8*)&fustrap_orig, sizeof(fustrap_orig));
		} else {
			printf("%s(): secure boot bit is not set in the flash image, secure boot will not be enabled\n", __func__);
		}

		return rc;
	}
	// No fuse image was found in flash, continue with the normal boot flow

#endif

	return 0;
}

int board_uart_init(void)
{
	struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();

	/* Mode3 - BMC UART3 connected to Serial Interface 2 */
	writel(((readl(&gcr->spswc) & ~(0x07)) | 2), &gcr->spswc);

	/* HSI2SEL */
	writel((readl(&gcr->mfsel1) | (1 << MFSEL1_HSI2SEL)), &gcr->mfsel1);

	/* BSPASEL */
	writel((readl(&gcr->mfsel4) & ~(1 << MFSEL4_BSPASEL)), &gcr->mfsel4);
	return 0;
}

int board_init(void)
{
#ifdef CONFIG_ETH_DESIGNWARE
    struct clk_ctl *clkctl = (struct clk_ctl *)npcm750_get_base_clk();
    struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();

#if 0
	/* Enable clock for GMAC1/2 module */
    writel((readl(&clkctl->clken2) | (1 << CLKEN2_GMAC1)), &clkctl->clken2);
    writel((readl(&clkctl->clken2) | (1 << CLKEN2_GMAC2)), &clkctl->clken2);
#endif
    /* Enable RGMII for GMAC1/2 module */
	writel((readl(&gcr->mfsel4) | (1 << MFSEL4_RG1SEL)), &gcr->mfsel4);
    writel((readl(&gcr->mfsel4) | (1 << MFSEL4_RG1MSEL)), &gcr->mfsel4);
	writel((readl(&gcr->mfsel4) | (1 << MFSEL4_RG2SEL)), &gcr->mfsel4);
    writel((readl(&gcr->mfsel4) | (1 << MFSEL4_RG2MSEL)), &gcr->mfsel4);

    /* IP Software Reset for GMAC1/2 module */
    writel(readl(&clkctl->ipsrst2) | (1 << IPSRST2_GMAC1), &clkctl->ipsrst2);
    writel(readl(&clkctl->ipsrst2) & ~(1 << IPSRST2_GMAC1), &clkctl->ipsrst2);
    writel(readl(&clkctl->ipsrst2) | (1 << IPSRST2_GMAC2), &clkctl->ipsrst2);
    writel(readl(&clkctl->ipsrst2) & ~(1 << IPSRST2_GMAC2), &clkctl->ipsrst2);
#endif
	gd->bd->bi_arch_number = CONFIG_MACH_TYPE;
	gd->bd->bi_boot_params = (PHYS_SDRAM_1 + 0x100UL);

	return 0;
}

int dram_init(void)
{
	struct npcm750_gcr *gcr = (struct npcm750_gcr *)npcm750_get_base_gcr();

	int RAMsize = (readl(&gcr->intcr3) >> 8) & 0x3;

	switch(RAMsize)
	{
		case 0:
				gd->ram_size = 0x08000000; /* 128 MB. */
				break;
		case 1:
				gd->ram_size = 0x10000000; /* 256 MB. */
				break;
		case 2:
		/* 3 and 4 should be 1 GB and 2 GB but as a workaround
			to correctly load linux we set it as 512 MB         */
		case 3:
		case 4:
				gd->ram_size = 0x20000000; /* 512 MB. */
				break;

		default:
			break;
	}

	return 0;
}

#ifdef CONFIG_BOARD_EARLY_INIT_F
int board_early_init_f(void)
{
	int err;

	err = board_uart_init();
	if (err) {
		debug("UART init failed\n");
		return err;
	}

	return 0;
}
#endif


int board_eth_init(bd_t *bis)
{
	return 0;
}


#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	const char *board_info;

	board_info = fdt_getprop(gd->fdt_blob, 0, "model", NULL);
	printf("Board: %s\n", board_info ? board_info : "unknown");
#ifdef CONFIG_BOARD_TYPES
	board_info = get_board_type();
	if (board_info)
		printf("Type:  %s\n", board_info);
#endif
	return 0;
}
#endif

#ifdef CONFIG_LAST_STAGE_INIT
int last_stage_init(void)
{
	int rc;
	char value[32];
	struct udevice *dev = gd->cur_serial_dev;

	if (gd->ram_size > 0) {
		sprintf(value, "%ldM", (gd->ram_size / 0x100000));
		env_set("mem", value);
	}

	if (dev && (dev->seq >= 0)) {
		void *addr;
		addr = dev_read_addr_ptr(dev);

		if (addr) {
			sprintf(value, "uart8250,mmio32,0x%x", (u32)addr);
			env_set("earlycon", value);
		}
		sprintf(value, "ttyS%d,115200n8", dev->seq);
		env_set("console", value);

	}
	
	rc = secure_boot_configuration();
	if (rc != 0)
		return rc;

	return 0;
}
#endif
