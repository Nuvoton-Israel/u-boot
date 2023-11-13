#include <common.h>
#include <command.h>
#include <cpu_func.h>
#include <malloc.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/arch/mailbox.h>
#include <asm/arch/otp.h>
#include <asm/arch/poleg_info.h>
#include <asm/arch/shared_defs.h>

void BOOTBLOCK_GetLoadedBootBlockHeader (u32** bootBlockHeader, u32** secondbootBlockHeader,u32* image)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	bool            bChosenImage;
	bool            bUseSecondLocationForSecondImage;
	u32             swapAddress = 0;
	u32             flashExamAddrArr[FLASH_BOOT_ALTERNATIVES];

	/* init the location array */
	flashExamAddrArr[0] = FLASH_EXAM_ADDR_1;
	flashExamAddrArr[1] = FLASH_EXAM_ADDR_2;

	/* check if location array needs update according to WDC and oAltImgLoc */
	bUseSecondLocationForSecondImage = readl(FUSTRAP) & FUSTRAP_O_ALTIMGLOC;
	bChosenImage                     = readl(&gcr->intcr2) & (1 << INTCR2_CHOSEN_IMAGE);

	/* take second image from CS1 */
	if (bUseSecondLocationForSecondImage)
		flashExamAddrArr[1] = FLASH_EXAM_ADDR_3;

	/* WDC (whatchdog counter) > 1 ? start from second image */
	if (bChosenImage) {
		swapAddress         = flashExamAddrArr[0];
		flashExamAddrArr[0] = flashExamAddrArr[1];
		flashExamAddrArr[1] = swapAddress;
	}

	/*-------------------------------------------------------------------------------------------------*/
	/* Update the cureently tested image location. This is done for debug and visibility of ROM code   */
	/*-------------------------------------------------------------------------------------------------*/
	if ( flashExamAddrArr[0] == FLASH_EXAM_ADDR_1)
		*image = 0;

	if ( flashExamAddrArr[0] == FLASH_EXAM_ADDR_2)
		*image = 1;

	if ( flashExamAddrArr[0] == FLASH_EXAM_ADDR_3)
		*image = 2;

	*bootBlockHeader = (u32 *)flashExamAddrArr[0];
	*secondbootBlockHeader = (u32 *)flashExamAddrArr[1];
}

bool is_ROMCode_Status_Basic_mode (void)
{
	ROM_STATUS_MSG *msgPtrROM = (ROM_STATUS_MSG *)ROM_STATUS_MSG_ADDR;

	// According to ROM_status_ram->status we can decide is secure mode or basic mode
	if ((msgPtrROM->status >= ST_ROM_USE_KEY0_IMAGE0) &&
	    (msgPtrROM->status <= ST_ROM_USE_KEY2_IMAGE2))
		return false;
	// basic mode
	else if ((msgPtrROM->status >= ST_ROM_BASIC_USE_IMAGE_SPI0_CS0_OFFSET0) &&
		 (msgPtrROM->status <= ST_ROM_BASIC_USE_IMAGE_SPI0_CS1_OFFSET0))
		return true;

	return false;
}

int spi_erase_tag(int cs)
{
	struct spi_flash *flash;
	struct udevice *udev;
	int bus = 0;
	int rc;

	rc = spi_flash_probe_bus_cs(bus, cs,
		CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE, &udev);
	if (rc)	{
		printf("SF: failed to probe spi\n");
		return rc;
	}

	flash = dev_get_uclass_priv(udev);
	if (!flash) {
		printf("SF: probe for flash failed\n");
		return -1;
	}

	rc = spi_flash_erase(flash, 0x0, flash->erase_size);
	printf("SF: %zu bytes @ %#x Erased: %s\n", (size_t)flash->erase_size,
		0x0, rc ? "ERROR" : "OK");
	return rc;
}

int spi_copy_image(int from, int to)
{
	struct spi_flash *flash;
	struct udevice *udev;
	int bus = 0;
	unsigned long addr = 0x10000000;
	void *src, *buf;
	u32 len, sector_addr, sector_offset;
	u32 dest_addr, end_addr;
	int chunk_sz;
	int newline;
	int rc;

	rc = spi_flash_probe_bus_cs(bus, from,
		CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE, &udev);
	if (rc) {
		printf("SF: failed to probe spi\n");
		return -1;
	}

	flash = dev_get_uclass_priv(udev);
	if (!flash) {
		printf("SF: probe for flash failed\n");
		return -1;
	}

	dest_addr = flash->erase_size;
	end_addr = flash->size;
	len = flash->size - flash->erase_size;
	src = map_physmem(addr, len, MAP_WRBACK);
	if (!src && addr) {
		printf("Failed to map physical memory\n");
		return -1;
	}

	rc = spi_flash_read(flash, dest_addr, len, src);
	printf("SF: %zu bytes @ %#x Read: %s\n", (size_t)len,
		dest_addr, rc ? "ERROR" : "OK");
	if(rc)
		goto done;

	rc = spi_flash_probe_bus_cs(bus, to,
		CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE, &udev);
	if (rc) {
		printf("SF: failed to probe spi\n");
		goto done;
	}

	flash = dev_get_uclass_priv(udev);
	if (!flash) {
		printf("SF: probe for flash failed\n");
		goto done;
	}

	/*
	 * sector_addr                             sector_end
	 * v                                          v
	 * | <-- secotr_offset--> | <## chunk_sz ##>  |
	 *                        ^
	 *                      dest_addr
	 */

	buf = memalign(ARCH_DMA_MINALIGN, flash->erase_size);
	newline = 64;

	while (dest_addr < end_addr) {
		sector_offset = dest_addr % flash->erase_size;
		sector_addr = dest_addr - sector_offset;
		chunk_sz = min(len, (flash->erase_size - sector_offset));

		/* read sector to buf */
		rc = spi_flash_read(flash, sector_addr, flash->erase_size, buf);
		if (rc) {
			printf("Read ERROR @ %#x\n", sector_addr);
			break;
		}

		if (memcmp(src, buf + sector_offset, chunk_sz) == 0) {
			printf(".");
			if (--newline == 0) {
				printf("\n");
				newline = 64;
			}
			/* source and target are the same, skip programming */
			dest_addr += chunk_sz;
			src += chunk_sz;
			len -= chunk_sz;
			continue;
		}

		if (chunk_sz < flash->erase_size) {
			/* erase sector */
			rc= spi_flash_erase(flash, sector_addr, flash->erase_size);
			debug("SF: %zu bytes @ %#x Erased: %s\n", (size_t)flash->erase_size,
				sector_addr, rc ? "ERROR" : "OK");

			/* update buf */
			memcpy(buf + sector_offset, src, chunk_sz);

			/* program sector */
			rc = spi_flash_write(flash, sector_addr, flash->erase_size, buf);
			debug("SF: %zu bytes @ %#x Written: %s\n", (size_t)flash->erase_size,
				sector_addr, rc ? "ERROR" : "OK");
		} else {
			printf("#");
			if (--newline == 0) {
				printf("\n");
				newline = 64;
			}
			/* erase sector */
			rc = spi_flash_erase(flash, sector_addr, flash->erase_size);
			debug("SF: %zu bytes @ %#x Erased: %s\n", (size_t)flash->erase_size,
				sector_addr, rc ? "ERROR" : "OK");

			/* program sector */
			rc = spi_flash_write(flash, sector_addr, chunk_sz, src);
			debug("SF: %zu bytes @ %#x Written: %s\n", (size_t)chunk_sz,
				sector_addr, rc ? "ERROR" : "OK");
		}
		dest_addr += chunk_sz;
		src += chunk_sz;
		len -= chunk_sz;
	}
	printf("\nSF: %zu bytes @ %#x Written: %s\n", (size_t)(flash->size - flash->erase_size),
		flash->erase_size, rc ? "ERROR" : "OK");

done:
	if (src)
		unmap_physmem(src, len);
	if (buf)
		free(buf);
	return rc == 0 ? 0 : 1;
}

int spi_copy_header(int from, int to)
{
	struct spi_flash *flash;
	struct udevice *udev;
	int bus = 0;
	char *buf;
	int rc;

	rc = spi_flash_probe_bus_cs(bus, from,
		CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE, &udev);
	if (rc) {
		printf("SF: failed to probe spi\n");
		return rc;
	}

	flash = dev_get_uclass_priv(udev);
	if (!flash) {
		printf("SF: probe for flash failed\n");
		return -1;
	}

	buf = memalign(ARCH_DMA_MINALIGN, flash->erase_size);

	rc = spi_flash_read(flash, 0, flash->erase_size, buf);
	printf("SF: %zu bytes @ %#x Read: %s\n", (size_t)flash->erase_size,
		0x0, rc ? "ERROR" : "OK");
	if(rc)
		goto done;

	rc = spi_flash_probe_bus_cs(bus, to,
		CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE, &udev);
	if (rc) {
		printf("SF: failed to probe spi\n");
		goto done;
	}

	flash = dev_get_uclass_priv(udev);
	if (!flash) {
		printf("SF: probe for flash failed\n");
		goto done;
	}

	rc = spi_flash_write(flash, 0x0, flash->erase_size, buf);
	printf("SF: %zu bytes @ %#x Written: %s\n", (size_t)flash->erase_size,
		0x0, rc ? "ERROR" : "OK");

done:
	free(buf);
	return rc;
}


int recovery_flash(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	ROM_STATUS_MSG *msgPtrROM = (ROM_STATUS_MSG *)ROM_STATUS_MSG_ADDR;
	ROM_STATUS_MSG *msgPtrBB = (ROM_STATUS_MSG *)BOOTBLK_STATUS_MSG_ADDR;
	u32 val;
	int status = 0;

	printf("> 9\n");
	printf("> NIST recovery\n");
	printf("> copy from 0x88000000 to 0x80000000 size 0x4000000\n");
	printf("> Prepare to copy...\n");

	printf("> erase tag 0\n");
	status = spi_erase_tag(0);
	if (status)
		return status;

	printf("> erase image and program, one sector at a time\n");
	status = spi_copy_image(1, 0);
	if (status)
		return status;

	printf("> copy header\n");
	status = spi_copy_header(1, 0);
	if (status)
		return status;

	printf("> done copy\n");

	val = readl(&gcr->intcr2);
	val &= ~(1 << INTCR2_SELFTEST_PASSED);
	writel(val, &gcr->intcr2);
	printf("> set rSelfTestPassed flag 0, INTCR2= 0x%08x\n", readl(&gcr->intcr2));

	val = readl(&gcr->intcr2);
	val &= ~(1 << INTCR2_SELFTEST_REQUEST);
	writel(val, &gcr->intcr2);
	printf("> set rSelfTestRequest flag 0, INTCR2= 0x%08x\n", readl(&gcr->intcr2));

	val = readl(&gcr->flockr1);
	val &= ~(1 << FLOCKR1_UPDATE_APPROVE);
	writel(val, &gcr->flockr1);
	printf("> set rUpdateApprove flag 0, FLOCKR1= 0x%08x\n", readl(&gcr->flockr1));

	reset_misc();
	printf("> set WDC 0, INTCR2= 0x%08x\n", readl(&gcr->intcr2));
	msgPtrBB->imageState.bytes[0] = IMAGE_NEW_COPY;
	msgPtrROM->imageState.bytes[0] = IMAGE_NEW_COPY;
	reset_cpu(0);

	return 0;
}

int update_flash(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	ROM_STATUS_MSG *msgPtrROM = (ROM_STATUS_MSG *)ROM_STATUS_MSG_ADDR;
	ROM_STATUS_MSG *msgPtrBB = (ROM_STATUS_MSG *)BOOTBLK_STATUS_MSG_ADDR;
	u32 val;
	int status;

	printf("> 6\n");
	printf("> NIST update\n");
	printf("> copy from 0x80000000 to 0x88000000 size 0x4000000\n");
	printf("> Prepare to copy...\n");

	printf("> erase tag 1\n");
	status = spi_erase_tag(1);
	if (status)
		return status;

	printf("> erase image and program, one sector at a time\n");
	status = spi_copy_image(0, 1);
	if (status)
		return status;

	printf("> copy header\n");
	status = spi_copy_header(0, 1);
	if (status)
		return status;

	printf("> done copy\n");

	val = readl(&gcr->intcr2);
	val &= ~(1 << INTCR2_SELFTEST_PASSED);
	writel(val, &gcr->intcr2);
	printf("> set rSelfTestPassed flag 0, INTCR2= 0x%08x\n", readl(&gcr->intcr2));

	val = readl(&gcr->intcr2);
	val &= ~(1 << INTCR2_SELFTEST_REQUEST);
	writel(val, &gcr->intcr2);
	printf("> set rSelfTestRequest flag 0, INTCR2= 0x%08x\n", readl(&gcr->intcr2));

	val = readl(&gcr->flockr1);
	val &= ~(1 << FLOCKR1_UPDATE_APPROVE);
	writel(val, &gcr->flockr1);
	printf("> set rUpdateApprove flag 0, FLOCKR1= 0x%08x\n", readl(&gcr->flockr1));

	reset_misc();
	printf("> set WDC 0, INTCR2= 0x%08x\n", readl(&gcr->intcr2));
	msgPtrBB->imageState.bytes[2] = IMAGE_NEW_COPY;
	msgPtrROM->imageState.bytes[2] = IMAGE_NEW_COPY;
	reset_cpu(0);

	return 0;
}

void lock_flash1(void)
{
	printf("> 13\n");
	printf("Lock Flash #1\n");
}

void nist_boot(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	const u8 startTag[START_TAG_SIZE] = START_TAG_ARR_BOOTBLOCK;
	u32 *BootBlockHeaderPtr, *secondBootBlockHeaderPtr;
	u32 *uBootHeaderPtr, *seconduBootHeaderPtr;
	u32 image = 0;
	ROM_STATUS_MSG *msgPtrROM = (ROM_STATUS_MSG *)ROM_STATUS_MSG_ADDR;
	ROM_STATUS_MSG *msgPtrBB = (ROM_STATUS_MSG *)BOOTBLK_STATUS_MSG_ADDR;
	u32 bb_ver0, bb_ver1;
	u32 uboot_ver0, uboot_ver1;
	u32 val;

	reset_misc();

	// Secure Boot Authentication Check are all failed
	if (is_ROMCode_Status_Basic_mode()) {
		printf(KRED "\n\n>Halt and catch fire.\n" KNRM );
		while(1);
	}

	BOOTBLOCK_GetLoadedBootBlockHeader(&BootBlockHeaderPtr, &secondBootBlockHeaderPtr, &image);
	uBootHeaderPtr = (u32 *)(0xFFFFF000 &
		((u32)BootBlockHeaderPtr + HEADER_SIZE + readl((u32)BootBlockHeaderPtr + HEADER_SIZE_OFFSET) + 0xFFF));
	seconduBootHeaderPtr = (u32 *)(0xFFFFF000 &
		((u32)secondBootBlockHeaderPtr + HEADER_SIZE + readl((u32)secondBootBlockHeaderPtr + HEADER_SIZE_OFFSET) + 0xFFF));
	printf("\n> NIST: bootblock at 0x%p, uboot at 0x%p, image num %d\n\n",
		  BootBlockHeaderPtr,  uBootHeaderPtr, image);

	if (image == 0) {
		bb_ver0 = readl((u32)BootBlockHeaderPtr + HEADER_VERSION_OFFSET);
		bb_ver1 = readl((u32)secondBootBlockHeaderPtr + HEADER_VERSION_OFFSET);
		uboot_ver0 = readl((u32)uBootHeaderPtr + HEADER_VERSION_OFFSET);
		uboot_ver1 = readl((u32)seconduBootHeaderPtr + HEADER_VERSION_OFFSET);
	}
	else {
		bb_ver0 = readl((u32)secondBootBlockHeaderPtr + HEADER_VERSION_OFFSET);
		bb_ver1 = readl((u32)BootBlockHeaderPtr + HEADER_VERSION_OFFSET);
		uboot_ver0 = readl((u32)seconduBootHeaderPtr + HEADER_VERSION_OFFSET);
		uboot_ver1 = readl((u32)uBootHeaderPtr + HEADER_VERSION_OFFSET);
	}
	printf("> BB ver0 = 0x%06x, ver1 = 0x%06x\n", bb_ver0, bb_ver1);
	printf("> UBOOT ver0 0x%06x , ver1 0x%06x\n", uboot_ver0, uboot_ver1);

	if (image == 0)	{	//Run Active Bootblock from flash #0
		printf("> 4\n");
		if (memcmp((void *)(secondBootBlockHeaderPtr), startTag, START_TAG_SIZE)) {
			printf(">bb : Check flash #1 start tag at 0x%p is invalid!\n", secondBootBlockHeaderPtr);
			update_flash();
		}

		printf(">bb : start tag OK at 0x%p\n", BootBlockHeaderPtr);

		if ((msgPtrROM->imageState.bytes[2] == IMAGE_BAD_SIGNATURE) ||
			(msgPtrROM->imageState.bytes[2] == IMAGE_REJECTED_BY_BB) ) {
			printf(">bb : Authenticate flash #1 invalid!\n");
			update_flash();
		}

		printf("> 7\n");
		if(msgPtrBB->imageState.bytes[0]== IMAGE_OK) {
			printf("image%d valid\n", image);
			printf("> 10 Perform Version Control\n");

			if (bb_ver0 < bb_ver1) {
				printf("> 8 Core reset\n");
				writel((readl(&gcr->intcr2) | (0x1 << INTCR2_WDC)), &gcr->intcr2);
				printf("> set WDC 1, INTCR2= 0x%08x\n", readl(&gcr->intcr2));
				reset_cpu(0);
			}
			else if (bb_ver0 > bb_ver1) {
				printf("> 11\n");
				val = readl(&gcr->intcr2) & (1 << INTCR2_SELFTEST_PASSED);
				printf("> get rSelfTestPassed flag %d, INTCR2= 0x%08x\n", val?1:0, readl(&gcr->intcr2));
				if (readl(&gcr->intcr2) & (1 << INTCR2_SELFTEST_PASSED)) {
					printf("> 14\n");
					val = readl(&gcr->flockr1) & (1 << FLOCKR1_UPDATE_APPROVE);
					printf("> get rUpdateApprove flag %d, FLOCKR1 = 0x%08x\n", val?1:0, readl(&gcr->flockr1));
					if(readl(&gcr->flockr1) & (1 << FLOCKR1_UPDATE_APPROVE))
						update_flash();
					else
						lock_flash1();
				} else {
					printf("> 12\n");
					val = readl(&gcr->intcr2);
					val |= (1 << INTCR2_SELFTEST_REQUEST);
					writel(val, &gcr->intcr2);
					printf("> set rSelfTestRequest flag 1, INTCR2= 0x%08x\n", readl(&gcr->intcr2));
					writel((readl(&gcr->intcr2) | (0x1 << INTCR2_WDC)), &gcr->intcr2);
					printf("> set WDC 1, INTCR2= 0x%08x\n", readl(&gcr->intcr2));
					lock_flash1();
				}
			} else {
				lock_flash1();
			}
		}
		else {
			printf("image%d invalid\n", image);
			printf("> 8 Core reset\n");
			writel((readl(&gcr->intcr2) | (0x1 << INTCR2_WDC)), &gcr->intcr2);
			printf("> set WDC 1, INTCR2= 0x%08x\n", readl(&gcr->intcr2));
			reset_cpu(0);
		}
	}
	else {		//Run Recovery Bootblock from flash #1
		printf("> 5\n");
		if(msgPtrBB->imageState.bytes[2]== IMAGE_OK) {
			printf("image%d valid\n", image);
			recovery_flash();
		}
	}
}
