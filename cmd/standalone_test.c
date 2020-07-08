// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

/*
 * Misc boot support
 */
#include <common.h>
#include <command.h>
#include <net.h>
#include <asm/arch/poleg_info.h>
#include <spi_flash.h>
#include <spi.h>

//                                        S     T     D     A     L     T     S     T
#define TEST_TAG_FLASH_IMAGE_VAL          {0x53, 0x54, 0x44, 0x41, 0x4C, 0x54, 0x53, 0x54}
#define TEST_TAG_FLASH_IMAGE_SIZE         (8)
#define TEST_IMG_CNT 3

/* Allow ports to override the default behavior */
__attribute__((weak))
unsigned long do_go_exec(ulong (*entry)(int, char * const []), int argc,
				 char * const argv[])
{
	return entry (argc, argv);
}

static int do_stdalonetst(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	u32 rc, rc_spi, *result;
	int rcode = 0, i=0, test_bin_number;
	struct spi_flash *flash;
	struct udevice *udev;
	u32 result_addr, addr_align;
	int offset;
	u8 *buf = NULL;
	const u8 tag[TEST_TAG_FLASH_IMAGE_SIZE] = TEST_TAG_FLASH_IMAGE_VAL;
	u32 image_addr[TEST_IMG_CNT], tst_bin_size[TEST_IMG_CNT], exec_addr[TEST_IMG_CNT], exec_result[TEST_IMG_CNT], start_addr;

	// standalone test images should be a part of the flash image, right after the uboot
	start_addr = POLEG_UBOOT_END;

	for( i=0 ; i<TEST_IMG_CNT ; i++)
	{
		if (((u32*)start_addr)[0] == ((u32*)tag)[0] &&
			((u32*)start_addr)[1] == ((u32*)tag)[1])
		{
			printf("%s(): Standalone TEST image %d is found on flash\n", __func__, i+1);
			printf("%s(): TAG address on flash 0x%x\n", __func__, start_addr);
			char dsc[65] = {0};
			tst_bin_size[i] = ((u32*)start_addr)[2];
			exec_addr[i] = ((u32*)start_addr)[3];
			image_addr[i] = start_addr+0x100;//header size is 0x100
			memcpy(dsc, start_addr+16, 64);
			exec_result[i] = ((u32*)start_addr)[20];

			printf("%s(): BIN address on flash: 0x%x\n", __func__, image_addr[i]);
			printf("%s(): BIN size is %d\n", __func__, tst_bin_size[i]);
			printf("%s(): Execution address: 0x%x\n", __func__, exec_addr[i]);
			printf("%s(): Description: %s\n", __func__, dsc);
			printf("%s(): Last Exec Result: %08x(%d)\n\n", __func__, exec_result[i],exec_result[i]);

			start_addr += 0x100 + tst_bin_size[i];//next image tag address
			if( start_addr & 0xFFF)
				start_addr = (start_addr & 0xFFFFF000) + 0x1000;
		}
		else
			break;
	}

	if (argc == 3)
	{
		if (!strncmp(argv[1], "exec", 4))
			test_bin_number = simple_strtoul(argv[2], NULL, 16);

		if( test_bin_number > i || test_bin_number <= 1 )
		{
			printf("%s(): Only %d Standalone Test exist\n", __func__, i);
			return CMD_RET_FAILURE;
		}
		test_bin_number--;

		/*
		* pass address parameter as argv[0] (aka command name),
		* and all remaining args
		*/
		if( exec_addr[test_bin_number] != 0xFFFFFFFF )
		{
			memcpy((void *)exec_addr[test_bin_number], (void *)image_addr[test_bin_number], tst_bin_size[test_bin_number]);
			printf ("## Starting application at 0x%08lX ...\n", exec_addr[test_bin_number]);
			rc = do_go_exec ((void *)exec_addr[test_bin_number], argc - 1, argv + 1);
		}
		else
		{
			printf ("## Starting application at 0x%08lX (PIE)...\n", image_addr[test_bin_number]);
			rc = do_go_exec ((void *)image_addr[test_bin_number], argc - 1, argv + 1);
		}
		if (rc != 0) rcode = 1;

		printf ("## Application terminated, rc = 0x%lX\n", rc);

		/*Save Test Result */
		char tmp[9]= {0};
		memcpy(tmp,image_addr[test_bin_number]-240,8);
		if(rc!=0)
			env_set(tmp, "Fail");
		else
			env_set(tmp, "Pass");
		env_save();

		rc_spi = spi_flash_probe_bus_cs(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
			CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE, &udev);
		if (rc_spi)
		{
			printf("spi_flash_probe_bus_cs fail\n");
			return rc_spi;
		}

		flash = dev_get_uclass_priv(udev);
		if (!flash)
		{
			printf("dev_get_uclass_priv fail\n");
			return -1;
		}

		result_addr = image_addr[test_bin_number] - 0xB0;
		// erase the whole sector
		addr_align = result_addr & ~(u32)(flash->erase_size -1);
		offset = result_addr - addr_align;
		addr_align -= SPI_FLASH_BASE_ADDR;

		buf = (u8 *)malloc(flash->erase_size);
		if (buf) {
			spi_flash_read(flash, addr_align, flash->erase_size, buf);
		} else {
			printf("%s(): failed to alloc buffer, skip writing result\n", __func__);
			return -1;
		}

		buf[offset]= (rc&0xFF);
		buf[offset+1]= (rc&0xFF00)>>8;
		buf[offset+2]= (rc&0xFF0000)>>16;
		buf[offset+3]= (rc&0xFF000000)>>24;

		rc_spi = spi_flash_erase(flash, addr_align, flash->erase_size);
		if (rc_spi != 0)
		{
			printf("%s(): spi_flash_erase fail\n", __func__);
			return rc_spi;
		}

		rc_spi = spi_flash_write(flash, addr_align, flash->erase_size, buf);
		if (rc_spi != 0)
		{
			printf("%s(): spi_flash_write fail\n", __func__);
			return rc_spi;
		}

		free(buf);

		return rcode;
	}

	return 0;

}

/* -------------------------------------------------------------------- */

U_BOOT_CMD(
	stdtst, 3, 0,	do_stdalonetst,
	"Stand Alone Test",
	"Execute Standalone Test append after uboot\n"
	"  exec number_of_standalone_test"
);
