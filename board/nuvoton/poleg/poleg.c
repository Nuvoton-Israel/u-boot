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
#include <asm/arch/otp.h>
#include <asm/arch/poleg_info.h>
#include <asm/arch/espi.h>
#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <clk.h>
#include <fuse.h>
#include <spi_flash.h>
#include <spi.h>
#include <env.h>

DECLARE_GLOBAL_DATA_PTR;

static int board_sd_clk_init(const char *name)
{
	struct udevice *clk_dev;
	struct clk clk;
	int node, err;
	const char *path;
	uint clkd[2]; /* clk_id and clk_no */
	int clk_offset;
	int rate;

	node = fdt_path_offset(gd->fdt_blob, "/aliases");
	if (node < 0)
		return -FDT_ERR_NOTFOUND;

	path = fdt_getprop(gd->fdt_blob, node, name, NULL);
	if (!path) {
		printf("no alias for mmc0\n");
		return -FDT_ERR_NOTFOUND;
	}

	node = fdt_path_offset(gd->fdt_blob, path);
	err = fdtdec_get_int_array(gd->fdt_blob, node, "clocks", clkd,
			2);
	if (err)
		return -FDT_ERR_NOTFOUND;

	rate = fdtdec_get_int(gd->fdt_blob, node, "clock-frequency", 400000);

	clk_offset = fdt_node_offset_by_phandle(gd->fdt_blob, clkd[0]);
	if (clk_offset < 0)
		return clk_offset;

	err = uclass_get_device_by_of_offset(UCLASS_CLK, clk_offset, &clk_dev);
	if (err)
		return err;

	clk.id = clkd[1];
	err = clk_request(clk_dev, &clk);
	if (err < 0)
		return err;

	err = clk_set_rate(&clk, rate);
	clk_free(&clk);
	if (err < 0)
		return err;

	return 0;
}

static bool is_security_enabled(void)
{
	u32 val = readl(FUSTRAP);

	if (val & FUSTRAP_O_SECBOOT) {
		printf("Security is enabled\n");
		return true;
	} else {
		printf("Security is NOT enabled\n");
		return false;
	}
}

static int check_nist_version(void)
{
#if (CONFIG_NIST_VERSION_ADDR != 0)
	volatile u32 uboot_ver = *(u32*)(UBOOT_RAM_IMAGE_ADDR + HEADER_VERSION_OFFSET);
	volatile u32 nist_ver = *(u32*)(CONFIG_NIST_VERSION_ADDR);

	if (uboot_ver != nist_ver)
		reset_cpu(0);
#endif

	return 0;
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

		u8 fuse_arrays[2 * NPCM_OTP_ARR_BYTE_SIZE];
		u32 fustrap_orig;

		printf("%s(): fuse array image was found on flash in address 0x%x\n", __func__, addr);

		memcpy(fuse_arrays, (u8*)addr, sizeof(fuse_arrays));

		fustrap_orig = *(u32*)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET);

		//TODO: Here, randomize 4 AES keys + generate their nibble parity + embed to image

		printf("%s(): program fuse key array from address 0x%x\n", __func__, addr + SA_KEYS_FLASH_IMAGE_OFFSET);

		rc = fuse_prog_image(NPCM_KEY_SA, (u32)(fuse_arrays + SA_KEYS_FLASH_IMAGE_OFFSET));
		if (rc != 0)
			return rc;

		// clear oSecBoot, will be programmed only after everything is
		// programmed successfuly
		fustrap_orig = *(u32*)(addr + SA_FUSE_FLASH_IMAGE_OFFSET);
		*(u32*)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET) &= ~FUSTRAP_O_SECBOOT;

		printf("%s(): program fuse strap array from address 0x%x\n", __func__, addr + SA_FUSE_FLASH_IMAGE_OFFSET);

		rc = fuse_prog_image(NPCM_FUSE_SA, (u32)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET));
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
			rc = fuse_program_data(NPCM_FUSE_SA, 0, (u8*)&fustrap_orig, sizeof(fustrap_orig));
		} else {
			printf("%s(): secure boot bit is not set in the flash image, secure boot will not be enabled\n", __func__);
		}

		return rc;
	}
	// No fuse image was found in flash, continue with the normal boot flow

#endif

	return 0;
}

int board_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	struct clk_ctl *clkctl = (struct clk_ctl *)npcm_get_base_clk();
	int nodeoff;
	u32 reg_val = 0;
	u32 espi_ch_supp;

	gd->bd->bi_arch_number = CONFIG_MACH_TYPE;
	gd->bd->bi_boot_params = (PHYS_SDRAM_1 + 0x100UL);

	nodeoff = -1;
	while ((nodeoff = fdt_node_offset_by_compatible(gd->fdt_blob, nodeoff,
                "npcm750,runbmc")) >= 0) {
		/* select DAC2 for VGA output */
		reg_val = (1 << INTCR_DACSEL) |
			(1 << INTCR_DACOSOVR) |
			(0x3 << INTCR_GFXIFDIS);
		writel((readl(&gcr->intcr) | reg_val), &gcr->intcr);

		/* select PLL1 clock for Graphic System */
		writel((readl(&clkctl->clksel) | (1 << CLKSEL_GFXCKSEL)), &clkctl->clksel);

		/* set Graphic Reset Delay to fix host stuck */
		writel((readl(&gcr->intcr3) | (0x7 << INTCR3_GFXRSTDLY) ), &gcr->intcr3);;

		board_sd_clk_init("mmc1");
	}

	nodeoff = -1;
	while ((nodeoff = fdt_node_offset_by_compatible(gd->fdt_blob, nodeoff,
                "quanta,olympus")) >= 0) {
		/* Uart Mode7 - BMC UART3 connected to Serial Interface 2 */
		writel(((readl(&gcr->spswc) & ~(SPMOD_MASK)) | SPMOD_MODE7), &gcr->spswc);
	}

	if (readl(&gcr->mfsel4) & (1 << MFSEL4_ESPISEL)) {
		espi_ch_supp = fdtdec_get_config_int(gd->fdt_blob, "espi-channel-support", 0);
		if (espi_ch_supp > 0) {
			reg_val = readl(NPCM_ESPI_BA + ESPICFG);
			writel(reg_val | ((espi_ch_supp & ESPICFG_CHNSUPP_MASK) << ESPICFG_CHNSUPP_SHFT),
					NPCM_ESPI_BA + ESPICFG);
		}
	}

	return 0;
}

int dram_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();

	int RAMsize = (readl(&gcr->intcr3) >> 8) & 0x7;

	switch(RAMsize)
	{
		case 0:
				gd->ram_size = 0x08000000; /* 128 MB. */
				break;
		case 1:
				gd->ram_size = 0x10000000; /* 256 MB. */
				break;
		case 2:
				gd->ram_size = 0x20000000; /* 512 MB. */
				break;
		case 3:
				gd->ram_size = 0x40000000; /* 1024 MB. */
				break;
		case 4:
				gd->ram_size = 0x80000000; /* 2048 MB. */
				break;

		default:
			break;
	}

	return 0;
}

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

	if (dev && (dev->seq_ >= 0)) {
		void *addr;
		addr = dev_read_addr_ptr(dev);

		if (addr) {
			sprintf(value, "uart8250,mmio32,0x%x", (u32)addr);
			env_set("earlycon", value);
		}
		sprintf(value, "ttyS%d,115200n8", dev->seq_);
		env_set("console", value);

	}

	if (is_security_enabled()) {

		rc = check_nist_version();
		if (rc != 0)
			return rc;
	} else {

		// OTP can be programmed only in basic mode
		rc = secure_boot_configuration();
		if (rc != 0)
			return rc;
	}

	return 0;
}
#endif
