// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <dm.h>
#include <spi_flash.h>
#include <spi.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/arch/otp.h>
#include <cpu_func.h>

#define NPCM_GCR_INTCR2_SELFTEST_PASSED		BIT(11)
#define NPCM_GCR_INTCR2_WDC			BIT(21)
#define NPCM_GCR_FLOCKR1_UPDATE_APPROVE		BIT(28)
#define NPCM_GCR_FLOCKR1_UPDATE_APPROVE_LOCK	BIT(29)

#define UBOOT_RAM_IMAGE_ADDR            (0x8000)
#define HEADER_ADDR_OFFSET              (0x140)
#define HEADER_SIZE_OFFSET              (0x144)
#define HEADER_VERSION_OFFSET           (0x148)
#define HEADER_SIZE                     (0x200)

static bool is_security_enabled(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)(uintptr_t)npcm_get_base_gcr();
	u32 val = readl(FUSTRAP);

	if (IS_ENABLED(CONFIG_ARCH_NPCM7xx)) {
		if (val & FUSTRAP_O_SECBOOT) {
			printf("Security is enabled\n");
			return true;
		}
	} else {
		if ((readl(&gcr->pwron) & (1 << PWRON_SECEN))) {
			printf("Security is enabled\n");
			return true;
		}
	}
	printf("Security is NOT enabled\n");

	return false;
}

static int check_nist_version(void)
{
	u32 uboot_ver, nist_ver;

	if (CONFIG_NIST_VERSION_ADDR != 0) {
		uboot_ver = readl(UBOOT_RAM_IMAGE_ADDR + HEADER_VERSION_OFFSET);
		nist_ver = readl(CONFIG_NIST_VERSION_ADDR);
		if (uboot_ver != nist_ver)
			reset_cpu(0);
	}

	return 0;
}

static int secure_boot_configuration(void)
{
	const u8 tag[SA_TAG_FLASH_IMAGE_SIZE] = SA_TAG_FLASH_IMAGE_VAL;
	struct spi_flash *flash;
	struct udevice *udev;
	u32 addr, addr_align;
	int rc, i, offset;
	u8 *buf = NULL;

	rc = spi_flash_probe_bus_cs(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
				    CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE, &udev);
	if (rc)
		return rc;

	flash = dev_get_uclass_priv(udev);
	if (!flash)
		return -1;

	/*
	 * fuse images should be a part of the flash image, right after the uboot
	 * TODO: set addr
	 */
	//addr = POLEG_UBOOT_END;

	/*
	 * if found, program the image to the fuse arrays, set the secure boot
	 * bit and erase the image from the flash
	 */
	if (((u32 *)(uintptr_t)(addr + SA_TAG_FLASH_IMAGE_OFFSET))[0] == ((u32 *)tag)[0] &&
	    ((u32 *)(uintptr_t)(addr + SA_TAG_FLASH_IMAGE_OFFSET))[1] == ((u32 *)tag)[1]) {
		u8 fuse_arrays[2 * NPCM_OTP_ARR_BYTE_SIZE];
		u32 fustrap_orig;

		printf("%s(): fuse array image was found on flash in address 0x%x\n",
		       __func__, addr);

		memcpy(fuse_arrays, (u8 *)(uintptr_t)addr, sizeof(fuse_arrays));

		fustrap_orig = *(u32 *)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET);

		//TODO: Here, randomize 4 AES keys + generate their nibble parity + embed to image

		printf("%s(): program fuse key array from address 0x%x\n", __func__,
		       addr + SA_KEYS_FLASH_IMAGE_OFFSET);

		rc = fuse_prog_image(NPCM_KEY_SA, (uintptr_t)(fuse_arrays +
				     SA_KEYS_FLASH_IMAGE_OFFSET));
		if (rc != 0)
			return rc;

		/*
		 * clear oSecBoot, will be programmed only after everything is
		 * programmed successfully
		 */
		fustrap_orig = *(u32 *)(uintptr_t)(addr + SA_FUSE_FLASH_IMAGE_OFFSET);
		*(u32 *)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET) &= ~FUSTRAP_O_SECBOOT;

		printf("%s(): program fuse strap array from address 0x%x\n", __func__,
		       addr + SA_FUSE_FLASH_IMAGE_OFFSET);

		rc = fuse_prog_image(NPCM_FUSE_SA, (uintptr_t)(fuse_arrays +
				     SA_FUSE_FLASH_IMAGE_OFFSET));
		if (rc != 0)
			return rc;

		// erase the whole sector
		addr_align = addr & ~(u32)(uintptr_t)(flash->erase_size - 1);

		offset = addr - addr_align;
		printf("%s(): erase the sector of addr 0x%x\n", __func__, addr_align);

		addr_align -= SPI_FLASH_BASE_ADDR;

		buf = (u8 *)(uintptr_t)malloc(flash->erase_size);
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

		// program SECBOOT bit if required
		if (fustrap_orig & FUSTRAP_O_SECBOOT) {
			printf("%s(): program secure boot bit to FUSTRAP\n", __func__);
			rc = fuse_program_data(NPCM_FUSE_SA, 0, (u8 *)&fustrap_orig,
					       sizeof(fustrap_orig));
		} else {
			printf("%s(): secure boot bit is not set in the flash image, secure boot will not be enabled\n",
			       __func__);
		}

		return rc;
	}
	/* No fuse image was found in flash, continue with the normal boot flow */

	return 0;
}

static void npcm_check_selftest(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	int val = 0;

	if (readl(&gcr->intcr2) & NPCM_GCR_INTCR2_SELFTEST_PASSED) {
		val = readl(&gcr->flockr1);
		val |= NPCM_GCR_FLOCKR1_UPDATE_APPROVE;
		writel(val, &gcr->flockr1);

		/* clear INTCR2.WDC */
		reset_misc();
	} else {
		val = readl(&gcr->flockr1);
		val &= ~NPCM_GCR_FLOCKR1_UPDATE_APPROVE;
		writel(val, &gcr->flockr1);

		val = readl(&gcr->flockr1);
		val |= NPCM_GCR_FLOCKR1_UPDATE_APPROVE_LOCK;
		writel(val, &gcr->flockr1);
	}
}

void arch_preboot_os(void)
{
	if (is_security_enabled()) {
		check_nist_version();
	} else {
		// OTP can be programmed only in basic mode
		secure_boot_configuration();
	}
	npcm_check_selftest();
}

