// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2010
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>
 *
 * (C) Copyright 2008 Atmel Corporation
 */
#include <common.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <flash.h>
#include <malloc.h>
#include <spi.h>
#include <spi_flash.h>
#include <search.h>
#include <errno.h>
#include <uuid.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <dm/device-internal.h>
#include <u-boot/crc.h>

#ifndef CONFIG_SPL_BUILD
#define INITENV
#endif

#ifdef CONFIG_ENV_OFFSET_REDUND
static ulong env_offset		= CONFIG_ENV_OFFSET;
static ulong env_new_offset	= CONFIG_ENV_OFFSET_REDUND;
#endif /* CONFIG_ENV_OFFSET_REDUND */

DECLARE_GLOBAL_DATA_PTR;

static struct spi_flash *env_flash;

static int setup_flash_device(void)
{
#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
	struct udevice *new;
	int	ret;

	/* speed and mode will be read from DT */
	ret = spi_flash_probe_bus_cs(CONFIG_ENV_SPI_BUS, CONFIG_ENV_SPI_CS,
				     CONFIG_ENV_SPI_MAX_HZ, CONFIG_ENV_SPI_MODE,
				     &new);
	if (ret) {
		env_set_default("spi_flash_probe_bus_cs() failed", 0);
		return ret;
	}

	env_flash = dev_get_uclass_priv(new);
#else
	if (env_flash)
		spi_flash_free(env_flash);

	env_flash = spi_flash_probe(CONFIG_ENV_SPI_BUS, CONFIG_ENV_SPI_CS,
				    CONFIG_ENV_SPI_MAX_HZ, CONFIG_ENV_SPI_MODE);
	if (!env_flash) {
		env_set_default("spi_flash_probe() failed", 0);
		return -EIO;
	}
#endif
	return 0;
}

#if defined(CONFIG_ENV_OFFSET_REDUND)
static int env_sf_save(void)
{
	env_t	env_new;
	char	*saved_buffer = NULL, flag = ENV_REDUND_OBSOLETE;
	u32	saved_size, saved_offset, sector;
	int	ret;

	ret = setup_flash_device();
	if (ret)
		return ret;

	ret = env_export(&env_new);
	if (ret)
		return -EIO;
	env_new.flags	= ENV_REDUND_ACTIVE;

	if (gd->env_valid == ENV_VALID) {
		env_new_offset = CONFIG_ENV_OFFSET_REDUND;
		env_offset = CONFIG_ENV_OFFSET;
	} else {
		env_new_offset = CONFIG_ENV_OFFSET;
		env_offset = CONFIG_ENV_OFFSET_REDUND;
	}

	/* Is the sector larger than the env (i.e. embedded) */
	if (CONFIG_ENV_SECT_SIZE > CONFIG_ENV_SIZE) {
		saved_size = CONFIG_ENV_SECT_SIZE - CONFIG_ENV_SIZE;
		saved_offset = env_new_offset + CONFIG_ENV_SIZE;
		saved_buffer = memalign(ARCH_DMA_MINALIGN, saved_size);
		if (!saved_buffer) {
			ret = -ENOMEM;
			goto done;
		}
		ret = spi_flash_read(env_flash, saved_offset,
					saved_size, saved_buffer);
		if (ret)
			goto done;
	}

	sector = DIV_ROUND_UP(CONFIG_ENV_SIZE, CONFIG_ENV_SECT_SIZE);

	puts("Erasing SPI flash...");
	ret = spi_flash_erase(env_flash, env_new_offset,
				sector * CONFIG_ENV_SECT_SIZE);
	if (ret)
		goto done;

	puts("Writing to SPI flash...");

	ret = spi_flash_write(env_flash, env_new_offset,
		CONFIG_ENV_SIZE, &env_new);
	if (ret)
		goto done;

	if (CONFIG_ENV_SECT_SIZE > CONFIG_ENV_SIZE) {
		ret = spi_flash_write(env_flash, saved_offset,
					saved_size, saved_buffer);
		if (ret)
			goto done;
	}

	ret = spi_flash_write(env_flash, env_offset + offsetof(env_t, flags),
				sizeof(env_new.flags), &flag);
	if (ret)
		goto done;

	puts("done\n");

	gd->env_valid = gd->env_valid == ENV_REDUND ? ENV_VALID : ENV_REDUND;

	printf("Valid environment: %d\n", (int)gd->env_valid);

done:
	if (saved_buffer)
		free(saved_buffer);

	return ret;
}

static int env_sf_load(void)
{
	int ret;
	int read1_fail, read2_fail;
	env_t *tmp_env1, *tmp_env2;

	tmp_env1 = (env_t *)memalign(ARCH_DMA_MINALIGN,
			CONFIG_ENV_SIZE);
	tmp_env2 = (env_t *)memalign(ARCH_DMA_MINALIGN,
			CONFIG_ENV_SIZE);
	if (!tmp_env1 || !tmp_env2) {
		env_set_default("malloc() failed", 0);
		ret = -EIO;
		goto out;
	}

	ret = setup_flash_device();
	if (ret)
		goto out;

	read1_fail = spi_flash_read(env_flash, CONFIG_ENV_OFFSET,
				    CONFIG_ENV_SIZE, tmp_env1);
	read2_fail = spi_flash_read(env_flash, CONFIG_ENV_OFFSET_REDUND,
				    CONFIG_ENV_SIZE, tmp_env2);

	ret = env_import_redund((char *)tmp_env1, read1_fail, (char *)tmp_env2,
				read2_fail, H_EXTERNAL);

	spi_flash_free(env_flash);
	env_flash = NULL;
out:
	free(tmp_env1);
	free(tmp_env2);

	return ret;
}
#else
static int env_sf_save(void)
{
	u32	saved_size, saved_offset, sector;
	char	*saved_buffer = NULL;
	int	ret = 1;
	env_t	env_new;

	ret = setup_flash_device();
	if (ret)
		return ret;

	/* Is the sector larger than the env (i.e. embedded) */
	if (CONFIG_ENV_SECT_SIZE > CONFIG_ENV_SIZE) {
		saved_size = CONFIG_ENV_SECT_SIZE - CONFIG_ENV_SIZE;
		saved_offset = CONFIG_ENV_OFFSET + CONFIG_ENV_SIZE;
		saved_buffer = malloc(saved_size);
		if (!saved_buffer)
			goto done;

		ret = spi_flash_read(env_flash, saved_offset,
			saved_size, saved_buffer);
		if (ret)
			goto done;
	}

	ret = env_export(&env_new);
	if (ret)
		goto done;

	sector = DIV_ROUND_UP(CONFIG_ENV_SIZE, CONFIG_ENV_SECT_SIZE);

	puts("Erasing SPI flash...");
	ret = spi_flash_erase(env_flash, CONFIG_ENV_OFFSET,
		sector * CONFIG_ENV_SECT_SIZE);
	if (ret)
		goto done;

	puts("Writing to SPI flash...");
	ret = spi_flash_write(env_flash, CONFIG_ENV_OFFSET,
		CONFIG_ENV_SIZE, &env_new);
	if (ret)
		goto done;

	if (CONFIG_ENV_SECT_SIZE > CONFIG_ENV_SIZE) {
		ret = spi_flash_write(env_flash, saved_offset,
			saved_size, saved_buffer);
		if (ret)
			goto done;
	}

	ret = 0;
	puts("done\n");

done:
	if (saved_buffer)
		free(saved_buffer);

	return ret;
}

#ifdef CONFIG_ENV_IS_BEHIND_UBOOT
uint32_t npcm_env_offset;
#endif

static int env_sf_load(void)
{
	int ret;
	char *buf = NULL;

#ifdef CONFIG_ENV_IS_BEHIND_UBOOT
	uint32_t uboot_offset, uboot_size;
	uboot_offset = readl(CONFIG_UBOOT_OFFSET_REG);
	uboot_size = readl(CONFIG_UBOOT_SIZE_REG);
	if (!uboot_offset || uboot_offset == 0xFFFFFFFF ) {
		printf("invalid uboot offset 0x%x\n", uboot_offset);
		npcm_env_offset = 0xFFFFFFFF;
		return -ENXIO;
	}
	npcm_env_offset = roundup(uboot_offset + uboot_size, CONFIG_ENV_SECT_SIZE);
	printf("env_offset:0x%x ", npcm_env_offset);
#endif
	buf = (char *)memalign(ARCH_DMA_MINALIGN, CONFIG_ENV_SIZE);
	if (!buf) {
		env_set_default("malloc() failed", 0);
		return -EIO;
	}

	ret = setup_flash_device();
	if (ret)
		goto out;

	ret = spi_flash_read(env_flash,
		CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE, buf);
	if (ret) {
		env_set_default("spi_flash_read() failed", 0);
		goto err_read;
	}

	ret = env_import(buf, 1, H_EXTERNAL);
	if (!ret)
		gd->env_valid = ENV_VALID;

err_read:
	spi_flash_free(env_flash);
	env_flash = NULL;
out:
	free(buf);

	return ret;
}
#endif

#if CONFIG_ENV_ADDR != 0x0
__weak void *env_sf_get_env_addr(void)
{
	return (void *)CONFIG_ENV_ADDR;
}
#endif

#if defined(INITENV) && (CONFIG_ENV_ADDR != 0x0)
/*
 * check if Environment on CONFIG_ENV_ADDR is valid.
 */
static int env_sf_init_addr(void)
{
	env_t *env_ptr = (env_t *)env_sf_get_env_addr();

	if (crc32(0, env_ptr->data, ENV_SIZE) == env_ptr->crc) {
		gd->env_addr	= (ulong)&(env_ptr->data);
		gd->env_valid	= 1;
	} else {
		gd->env_addr = (ulong)&default_environment[0];
		gd->env_valid = 1;
	}

	return 0;
}
#endif

#if defined(CONFIG_ENV_SPI_EARLY)
/*
 * early load environment from SPI flash (before relocation)
 * and check if it is valid.
 */
static int env_sf_init_early(void)
{
	int ret;
	int read1_fail;
	int read2_fail;
	int crc1_ok;
	env_t *tmp_env2 = NULL;
	env_t *tmp_env1;

	/*
	 * if malloc is not ready yet, we cannot use
	 * this part yet.
	 */
	if (!gd->malloc_limit)
		return -ENOENT;

	tmp_env1 = (env_t *)memalign(ARCH_DMA_MINALIGN,
			CONFIG_ENV_SIZE);
	if (IS_ENABLED(CONFIG_SYS_REDUNDAND_ENVIRONMENT))
		tmp_env2 = (env_t *)memalign(ARCH_DMA_MINALIGN,
					     CONFIG_ENV_SIZE);

	if (!tmp_env1 || !tmp_env2)
		goto out;

	ret = setup_flash_device();
	if (ret)
		goto out;

	read1_fail = spi_flash_read(env_flash, CONFIG_ENV_OFFSET,
				    CONFIG_ENV_SIZE, tmp_env1);

	if (IS_ENABLED(CONFIG_SYS_REDUNDAND_ENVIRONMENT)) {
		read2_fail = spi_flash_read(env_flash,
					    CONFIG_ENV_OFFSET_REDUND,
					    CONFIG_ENV_SIZE, tmp_env2);
		ret = env_check_redund((char *)tmp_env1, read1_fail,
				       (char *)tmp_env2, read2_fail);

		if (ret == -EIO || ret == -ENOMSG)
			goto err_read;

		if (gd->env_valid == ENV_VALID)
			gd->env_addr = (unsigned long)&tmp_env1->data;
		else
			gd->env_addr = (unsigned long)&tmp_env2->data;
	} else {
		if (read1_fail)
			goto err_read;

		crc1_ok = crc32(0, tmp_env1->data, ENV_SIZE) ==
				tmp_env1->crc;
		if (!crc1_ok)
			goto err_read;

		/* if valid -> this is our env */
		gd->env_valid = ENV_VALID;
		gd->env_addr = (unsigned long)&tmp_env1->data;
	}

	return 0;
err_read:
	spi_flash_free(env_flash);
	env_flash = NULL;
	free(tmp_env1);
	if (IS_ENABLED(CONFIG_SYS_REDUNDAND_ENVIRONMENT))
		free(tmp_env2);
out:
	/* env is not valid. always return 0 */
	gd->env_valid = ENV_INVALID;
	return 0;
}
#endif

static int env_sf_init(void)
{
#if defined(INITENV) && (CONFIG_ENV_ADDR != 0x0)
	return env_sf_init_addr();
#elif defined(CONFIG_ENV_SPI_EARLY)
	return env_sf_init_early();
#endif
	/*
	 * return here -ENOENT, so env_init()
	 * can set the init bit and later if no
	 * other Environment storage is defined
	 * can set the default environment
	 */
	return -ENOENT;
}

U_BOOT_ENV_LOCATION(sf) = {
	.location	= ENVL_SPI_FLASH,
	ENV_NAME("SPIFlash")
	.load		= env_sf_load,
	.save		= CONFIG_IS_ENABLED(SAVEENV) ? ENV_SAVE_PTR(env_sf_save) : NULL,
	.init		= env_sf_init,
};
