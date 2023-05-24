// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 Nuvoton Technology Corp.
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <fdtdec.h>
#include <asm/arch/cpu.h>
#include <asm/arch/espi.h>
#include <asm/arch/gcr.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <linux/bitfield.h>
#include <linux/delay.h>

#ifdef CONFIG_EXT_TPM2_SPI
#include <mapmem.h>
#include <spi.h>
#include <tpm-v2.h>
#include <asm/arch/sha.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#define CLKSEL	0x4
#define PIXCKSEL_GFX	0
#define PIXCKSEL_MASK	GENMASK(5, 4)
#define SR_MII_CTRL_SWR_BIT15	15

#define DRAM_512MB_ECC_SIZE	0x1C000000ULL
#define DRAM_512MB_SIZE		0x20000000ULL
#define DRAM_1GB_ECC_SIZE	0x38000000ULL
#define DRAM_1GB_SIZE		0x40000000ULL
#define DRAM_2GB_ECC_SIZE	0x70000000ULL
#define DRAM_2GB_SIZE		0x80000000ULL
#define DRAM_4GB_ECC_SIZE	0xE00000000ULL
#define DRAM_4GB_SIZE		0x100000000ULL

#ifdef CONFIG_EXT_TPM2_SPI
#define CONFIG_TPM_SPI_BUS	5
#define CONFIG_TPM_SPI_CS	0
#define CONFIG_TPM_SPI_MODE	0
#define CONFIG_TPM_SPI_FREQ	10000000
#define NPCM_MEASURE_BASE	0xF0848000
#define NPCM_MEASURE_UBT	0x294
#define NPCM_MEASURE_SKMT	0xC1E
#define NPCM_MEASURE_SIZE	64

u8 ubt_digest[TPM2_DIGEST_LEN];
u8 skmt_digest[TPM2_DIGEST_LEN];

extern int get_tpm(struct udevice **devp);
extern void print_byte_string(u8 *data, size_t count);
#endif

static void arbel_eth_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)(uintptr_t)npcm_get_base_gcr();
	u32 val;
	char *evb_ver;
	unsigned int start;

	/* Power voltage select setup */
	val = readl(&gcr->vsrcr);
	writel(val | BIT(30), &gcr->vsrcr);

	/* EVB X00 version - need to swap sgmii lane polarity HW issue */
	evb_ver = env_get("evb_version");
	if (evb_ver && !strcmp(evb_ver, "X00")) {
		/* SGMII PHY reset */
		writew(0x1F00, 0xF07801FE);           /* Get access to 0x3E... (SR_MII_CTRL) */
		writew(readw(0xF0780000) | (1 << SR_MII_CTRL_SWR_BIT15), 0xF0780000);
		start = get_timer(0);

		printf("SGMII PCS PHY reset wait\n");
		while (readw(0xF0780000) & (1 << SR_MII_CTRL_SWR_BIT15)) {
			if (get_timer(start) >= 3 * CONFIG_SYS_HZ) {
				printf("SGMII PHY reset timeout\n");
				return;
			}
			mdelay(1);
		};
		/* Get access to 0x3F... (VR_MII_MMD_DIG_CTRL1) */
		writew(0x1F80, 0xF07801FE);
		/* Swap lane polarity on EVB only */
		writew(readw(0xf07801c2) | BIT(0), 0xf07801c2);
		/* Set SGMII MDC/MDIO pins to output slew-rate high */
		writel(readl(0xf001305) | 0x3000, 0xf001305c);
		printf("EVB-X00 SGMII Work-Around\n");
	}
}

static void arbel_clk_init(void)
{
	u32 val;

	/* Select GFX_PLL as PIXCK source */
	val = readl(NPCM_CLK_BA + CLKSEL);
	val &= ~PIXCKSEL_MASK;
	val |= FIELD_PREP(PIXCKSEL_MASK, PIXCKSEL_GFX);
	writel(val, NPCM_CLK_BA + CLKSEL);
}

int board_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();

	u32 val;

        val = readl(&gcr->intcr2);
	if((val & INTCR2_CORST) && (val & INTCR2_PORST))
	{
		val &= ~INTCR2_CORST;
		writel(val, &gcr->intcr2);
	}

	arbel_clk_init();
	arbel_eth_init();

	return 0;
}

int dram_init(void)
{
	struct npcm_gcr *gcr = (struct npcm_gcr *)npcm_get_base_gcr();
	uint64_t delta = 0ULL;

	/*
	 * get dram active size value from bootblock.
	 * Value sent using scrpad_03 register.
	 * feature available in bootblock 0.0.6 and above.
	 */
	gd->ram_size = readl(&gcr->scrpad_c);
	debug("%s: scrpad_c: %llx ", __func__, gd->ram_size);

	if (gd->ram_size == 0) {
		gd->ram_size = readl(&gcr->scrpad_b);
		debug("%s: scrpad_b: %llx ", __func__, gd->ram_size);
	} else {
		gd->ram_size *= 0x100000ULL;
	}

	gd->bd->bi_dram[0].start = 0;
	debug("ram_size: %llx ", gd->ram_size);

	switch (gd->ram_size) {
	case DRAM_512MB_ECC_SIZE:
	case DRAM_512MB_SIZE:
	case DRAM_1GB_ECC_SIZE:
	case DRAM_1GB_SIZE:
	case DRAM_2GB_ECC_SIZE:
	case DRAM_2GB_SIZE:
		gd->bd->bi_dram[0].size = gd->ram_size;
		gd->bd->bi_dram[1].start = 0;
		gd->bd->bi_dram[1].size = 0;
		break;
	case DRAM_4GB_ECC_SIZE:
		gd->bd->bi_dram[0].size = DRAM_2GB_ECC_SIZE;
		gd->bd->bi_dram[1].start = DRAM_4GB_SIZE;
		gd->bd->bi_dram[1].size = DRAM_2GB_SIZE;
		delta = DRAM_4GB_SIZE - DRAM_2GB_ECC_SIZE;
		break;
	case DRAM_4GB_SIZE:
		gd->bd->bi_dram[0].size = DRAM_2GB_SIZE;
		gd->bd->bi_dram[1].start = DRAM_4GB_SIZE;
		gd->bd->bi_dram[1].size = DRAM_2GB_SIZE;
		delta = DRAM_4GB_SIZE - DRAM_2GB_SIZE;
		break;
	default:
		gd->bd->bi_dram[0].size = DRAM_1GB_SIZE;
		gd->bd->bi_dram[1].start = 0;
		gd->bd->bi_dram[1].size = 0;
		break;
	}

	gd->ram_size -= delta;

	return 0;
}

int dram_init_banksize(void)
{
	dram_init();

	return 0;
}

#ifdef CONFIG_EXT_TPM2_SPI
static void hash_show(uint8_t *buf, ulong addr, ulong len)
{
	int i;

	printf("sha256 for %08lx ... %08lx ==> ", addr, addr + len - 1);

	for (i = 0; i < TPM2_DIGEST_LEN; i++) {
		printf("%02x", buf[i]);
	}

	printf("\n");
}

static int measure_write(struct udevice *dev)
{
	struct tpm_chip_priv *priv;
	u8 buf[NPCM_MEASURE_SIZE];
	void *value;
	void *hash_ubt;
	void *hash_skmt;
	u32 rc;

	priv = dev_get_uclass_priv(dev);
	if (!priv)
		return -EINVAL;

	value = map_sysmem(NPCM_MEASURE_BASE + NPCM_MEASURE_UBT, NPCM_MEASURE_SIZE);
	memcpy(buf, value, NPCM_MEASURE_SIZE);
	unmap_sysmem(buf);

	npcm_sha_calc(npcm_sha_type_sha2, buf, NPCM_MEASURE_SIZE, hash_ubt);
	hash_show(hash_ubt, NPCM_MEASURE_BASE + NPCM_MEASURE_UBT, NPCM_MEASURE_SIZE);

	rc = tpm2_pcr_extend(dev, 0, TPM2_ALG_SHA256, hash_ubt, TPM2_DIGEST_LEN);
	if (rc)
		return rc;

	value = map_sysmem(NPCM_MEASURE_BASE + NPCM_MEASURE_SKMT, NPCM_MEASURE_SIZE);
	memcpy(buf, value, NPCM_MEASURE_SIZE);
	unmap_sysmem(buf);

	npcm_sha_calc(npcm_sha_type_sha2, buf, NPCM_MEASURE_SIZE, hash_skmt);
	hash_show(hash_skmt, NPCM_MEASURE_BASE + NPCM_MEASURE_SKMT, NPCM_MEASURE_SIZE);

	rc = tpm2_pcr_extend(dev, 1, TPM2_ALG_SHA256, hash_skmt, TPM2_DIGEST_LEN);

	return rc;
}

static int measure_read(struct udevice *dev, u32 index)
{
	struct tpm_chip_priv *priv;
	unsigned int updates;
	void *digest;
	u32 rc;

	priv = dev_get_uclass_priv(dev);
	if (!priv)
		return -EINVAL;

	rc = tpm2_pcr_read(dev, index, priv->pcr_select_min, digest, &updates);

	if (!rc) {
		printf("PCR #%u content (%u known updates):\n", index, updates);
		print_byte_string(digest, TPM2_DIGEST_LEN);
	}

	return rc;
}

int tpm_measure(void)
{
	struct udevice *dev;
	struct tpm_chip_priv *priv;
	int rc;

	rc = get_tpm(&dev);
	if (rc)
		return rc;

	rc = tpm_init(dev);
	if (rc)
		return rc;

	rc = tpm2_startup(dev, TPM2_SU_CLEAR);
	if (rc)
		return rc;

	rc = tpm2_self_test(dev, TPMI_YES);
	if (rc)
		return rc;

	priv = dev_get_uclass_priv(dev);
	if (!priv)
		return -EINVAL;

	rc = measure_write(dev);
	rc = measure_read(dev, 0);
	rc = measure_read(dev, 1);

	return rc;
}

int last_stage_init(void)
{
	struct udevice *dev;
	struct spi_slave *slave;

	/* probe spi tpm device on bus5/cs0 */
	spi_get_bus_and_cs(CONFIG_TPM_SPI_BUS, CONFIG_TPM_SPI_CS,
			   CONFIG_TPM_SPI_FREQ, CONFIG_TPM_SPI_MODE,
			   "tpm_drv", "tpm_dev", &dev, &slave);

	/* write measurements to spi tpm device */
	tpm_measure();

	return 0;
}
#endif
