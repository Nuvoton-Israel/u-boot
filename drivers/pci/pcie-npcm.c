// SPDX-License-Identifier: GPL-2.0
/*
 * Nuvoton NPCM PCIe host controller driver.
 */

#include <dm.h>
#include <log.h>
#include <pci.h>
#include <reset.h>
#include <regmap.h>
#include <syscon.h>
#include <linux/bitmap.h>
#include <linux/sizes.h>
#include <asm/io.h>
#include <dm/devres.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/printk.h>
#include <asm/gpio.h>
#include "pci_internal.h"

/* gcr register */
#define GCR_INTCR3		0x9C

/* ranges flages define */
#define DT_FLAGS_TO_TYPE(flags)	(((flags) >> 24) & 0x03)
#define DT_TYPE_IO		0x1
#define DT_TYPE_MEM32		0x2

/* NPCM PCIe rc configuration base address */
#define RCCFGNUM		0x140
#define PCIERC_AXI_ERROR_REPORT	0x3E0
#define PCIERC_ISTATUS_LOCAL_ADDR 0x184
#define LINK_UP_FIELD		(0x3F << 20)
#define PCIERC_CFG_NO_SLVERR    BIT(0)
#define INTCR3_RCCORER_BIT	BIT(22)

/* RCAPnSAL register fields */
#define CFG_WIN_NUM 	1
#define CFG_SIZE_4K     11
#define RCA_WIN_EN	BIT(0)

/* RCAPnTP register fields */
#define TRSF_PARAM_MEMORY    (0L << 16)
#define TRSF_PARAM_CONFIG    (1L << 16)
#define TRSF_PARAM_IO        (2L << 16)
#define TRSL_ID_PCIE_TX_RX   0
#define TRSL_ID_PCIE_CONFIG  1

/* RCPAnTP register fields */
#define PLDA_XPRESS_RICH_MEMORY_WINDOW		0
#define PLDA_XPRESS_RICH_CONFIG_WINDOW		1

#define PLDA_XPRESS_RICH_TARGET_PCI_TX_RX	0
#define PLDA_XPRESS_RICH_TARGET_PCI_CONFIG	1
#define PLDA_XPRESS_RICH_TARGET_AXI_MASTER	4

#define PCI_RC_ATTR_TRSF_PARAM_POS		16
#define PCI_RC_ATTR_TRSL_ID_POS			0

/* RCPAnSAL register fields */
#define PCI_RC_ATTR_WIN_EN_POS			0
#define PCI_RC_ATTR_WIN_SIZE_POS		1
#define PCI_RC_ATTR_AP_ADDR_L_POS		12


/* AXI-to-PCIe Window 1 to 4 Registers */
#define RCAPnSAL(n) (0x800 + (0x20 * (n)))
#define RCAPnSAH(n) (0x804 + (0x20 * (n)))
#define RCAPnTAL(n) (0x808 + (0x20 * (n)))
#define RCAPnTAH(n) (0x80C + (0x20 * (n)))
#define RCAPnTP(n)  (0x810 + (0x20 * (n)))

/* PCIe-to-AXI Window 0 and 1 Registers */
#define RCPAnSAL(n) (0x600 + (0x100 * (n)))
#define RCPAnSAH(n) (0x604 + (0x100 * (n)))
#define RCPAnTAL(n) (0x608 + (0x100 * (n)))
#define RCPAnTAH(n) (0x60C + (0x100 * (n)))
#define RCPAnTP(n)  (0x610 + (0x100 * (n)))

struct npcm_pcie {
	void __iomem *reg_base;
	void __iomem *config_base;
	struct regmap *gcr_base;
	struct reset_ctl reset;
	struct resource res;
	struct gpio_desc ep_rst;
	struct udevice *dev;
};

static int npcm_pcie_rc_device_connected(struct npcm_pcie *pcie)
{
	u32 val;

	/*
	 * Check the Link status register on the bridge
	 * configuration space at:
	 * Bus 0, Device 0, function 0 offset 0x92 bit 4
	 */
	iowrite32(0x1F0000 , pcie->reg_base + RCCFGNUM);
	val = ioread32(pcie->config_base + 0x90);
	return  ((val & LINK_UP_FIELD) >> 20);
}

/* Configuration Space Read */
static int npcm_config_read(const struct udevice *bus, pci_dev_t bdf,
		uint offset, ulong *valuep,
		enum pci_size_t size)
{
	struct npcm_pcie *pcie = dev_get_priv(bus);

	if (npcm_pcie_rc_device_connected(pcie) == 0) {
		dev_err(pcie->dev, "npcm_pcie_rc_config_read - NO LINK\n");
		*valuep = 0xFFFFFFFF;
		return 0;
	}

	if ( (PCI_BUS(bdf) == 0) && ((offset & ~(0x3)) == 8)) {
		*valuep = 0x6040001;
	}else{
		writel(0x1F0000 | (PCI_BUS(bdf) << 8) | (PCI_DEV(bdf) << 3) | PCI_FUNC(bdf), pcie->reg_base + RCCFGNUM);
		*valuep = ioread32(pcie->config_base + (offset & ~(0x3)));
	}

	if (size == PCI_SIZE_8)
		*valuep = (*valuep >> (8 * (offset & 3))) & 0xff;
	else if (size == PCI_SIZE_16)
		*valuep = (*valuep >> (8 * (offset & 3))) & 0xffff;

	debug("Read Address: 0x%p, Read Value: 0x%lx\n",
			pcie->config_base + (offset & ~0x3), *valuep);
	return 0;
}

/* Configuration Space Write */
static int npcm_config_write(struct udevice *bus, pci_dev_t bdf,
		uint offset, ulong value,
		enum pci_size_t size)
{
	struct npcm_pcie *pcie = dev_get_priv(bus);

	if ((PCI_BUS(bdf) > 0) && (npcm_pcie_rc_device_connected(pcie) == 0)) {
		dev_err(pcie->dev, "npcm_pcie_rc_config_write - NO LINK\n");
		return -EIO;
	}

	writel(0x1F0000 | (PCI_BUS(bdf) << 8) | (PCI_DEV(bdf) << 3) | PCI_FUNC(bdf), pcie->reg_base + RCCFGNUM);

	if (size == PCI_SIZE_32) {
		iowrite32(value, pcie->config_base + (offset & ~0x3));
	} else {
		u32 orig = ioread32(pcie->config_base + (offset & ~0x3));
		u32 new;

		if (size == PCI_SIZE_8) {
			new = (orig & ~(0xff << (8 * (offset & 3)))) |
				((value & 0xff) << (8 * (offset & 3)));
		} else {
			new = (orig & ~(0xffff << (8 * (offset & 3)))) |
				((value & 0xffff) << (8 * (offset & 3)));
		}
		iowrite32(new, pcie->config_base + (offset & ~0x3));
	}
	debug("Write Address: 0x%p, Write Value: 0x%lx\n",
			pcie->config_base + (offset & ~0x3), value);
	return 0;
}

static const struct dm_pci_ops npcm_pcie_ops = {
	.read_config  = npcm_config_read,
	.write_config = npcm_config_write,
};

/* Root Complex Initialization */
static void npcm_initialize_as_root_complex(struct npcm_pcie *pcie)
{

	regmap_update_bits(pcie->gcr_base, GCR_INTCR3, INTCR3_RCCORER_BIT, 0x0);

	reset_assert(&pcie->reset);

	regmap_update_bits(pcie->gcr_base, GCR_INTCR3, INTCR3_RCCORER_BIT, INTCR3_RCCORER_BIT);

	reset_deassert(&pcie->reset);

	/* Only for NPCM8XX set error report to no slave error */

	writel(readl(pcie->reg_base + PCIERC_AXI_ERROR_REPORT) | PCIERC_CFG_NO_SLVERR, pcie->reg_base + PCIERC_AXI_ERROR_REPORT);
}

static int set_translation_window(struct npcm_pcie *pcie, u32 win_num,
		u64 source_addr, u32 size , u64 dest_addr,
		u8 win_type, u8 target)
{
	u8 win_size = CFG_SIZE_4K;
	u32 val;

	if (size < SZ_4K){
		dev_err(pcie->dev, "window size should be greater then 4KB\n ");
		return -1;
	}

	size = (size >> (CFG_SIZE_4K + 2));

	/* conuting bit set */
	while(size) {
		size= (size >> 1);
		win_size++;
	}

#ifdef __LP64__
	writel(((uint64_t)source_addr & 0xffffffff) +
			(win_size << PCI_RC_ATTR_WIN_SIZE_POS)+(1 << PCI_RC_ATTR_WIN_EN_POS) , pcie->reg_base + RCPAnSAL(win_num));
	writel(((uint64_t)source_addr >> 32 ) & 0xffffffff ,pcie->reg_base + RCPAnSAH(win_num));
	writel(((uint64_t)dest_addr & 0xffffffff) , pcie->reg_base + RCPAnTAL(win_num));
	writel(((uint64_t)dest_addr >> 32 ) & 0xffffffff , pcie->reg_base + RCPAnTAH(win_num));
#else
	writel( ((u32)source_addr  ) +
			(win_size << PCI_RC_ATTR_WIN_SIZE_POS)+(1 << PCI_RC_ATTR_WIN_EN_POS) , pcie->reg_base + RCPAnSAL(win_num));
	writel(0 ,pcie->reg_base + RCPAnSAH(win_num));
	writel((u32)dest_addr, pcie->reg_base + RCPAnTAL(win_num));
	writel(0, pcie->reg_base + RCPAnTAH(win_num));
#endif

	val = (win_type << PCI_RC_ATTR_TRSF_PARAM_POS) + (target << PCI_RC_ATTR_TRSL_ID_POS);
	writel(val, pcie->reg_base + RCPAnTP(win_num));

	return 0;
}

static void npcm_pcie_rc_init_config_window(struct npcm_pcie *pcie)
{
	ofnode node = dev_ofnode(pcie->dev);
	ofnode parent_node = dev_ofnode(pcie->dev->parent);
	const fdt32_t *ranges;
	int max_regions;
	struct pci_region dma_ranges;
	int start_win_num = CFG_WIN_NUM + 1;
	int len, i, cells_per_record;
	u64 pci_addr_cells, cpu_addr_cells, size_cells;

	if (!pcie->res.start) {
		dev_err(pcie->dev, "Invalid resource start address\n");
		return;
	}

	/* Enable configuration window */
	iowrite32((pcie->res.start & 0xFFFFF000) | (CFG_SIZE_4K << 1) | RCA_WIN_EN, pcie->reg_base + RCAPnSAL(CFG_WIN_NUM));
	iowrite32(0, pcie->reg_base + RCAPnSAH(CFG_WIN_NUM));
	iowrite32(pcie->res.start, pcie->reg_base + RCAPnTAL(CFG_WIN_NUM));
	iowrite32(0, pcie->reg_base + RCAPnTAH(CFG_WIN_NUM));
	iowrite32(TRSF_PARAM_CONFIG | TRSL_ID_PCIE_CONFIG, pcie->reg_base + RCAPnTP(CFG_WIN_NUM));

	ranges = ofnode_get_property(node, "ranges", &len);
	if (!ranges) {
		dev_err(pcie->dev, "No ranges property found\n");
		return;
	}

	pci_addr_cells = ofnode_read_simple_addr_cells(node);
	cpu_addr_cells = ofnode_read_simple_addr_cells(parent_node);
	size_cells = ofnode_read_simple_size_cells(node);

	len /= sizeof(fdt32_t);
	cells_per_record = pci_addr_cells + cpu_addr_cells + size_cells;
	max_regions = len / cells_per_record;

	/* Initial AXI-to-PCI windows */
	for (i = 0; i < max_regions; i++, len -= cells_per_record, start_win_num++) {
		u64 pci_addr, cpu_addr;
		u32 flags;
		int bit_size;
		unsigned long size;

		if (len < cells_per_record)
			break;

		flags = fdt32_to_cpu(ranges[0]);
		pci_addr = fdtdec_get_number(ranges + 1, 2);
		ranges += pci_addr_cells;
		cpu_addr = fdtdec_get_number(ranges, cpu_addr_cells);
		ranges += cpu_addr_cells;
		size = fdtdec_get_number(ranges, size_cells);
		ranges += size_cells;
		bit_size = find_first_bit(&size, 32);
		debug( "region %d, pci_addr=%llx, addr=%llx, size=%lx ,flags=%x\n",
				__func__, i, pci_addr, cpu_addr, size, flags);

		switch (DT_FLAGS_TO_TYPE(flags)) {
			case DT_TYPE_IO:
				iowrite32(pci_addr, pcie->reg_base + RCAPnTAL(start_win_num));
				iowrite32(0, pcie->reg_base + RCAPnTAH(start_win_num));
				iowrite32(TRSF_PARAM_IO | TRSL_ID_PCIE_CONFIG, pcie->reg_base + RCAPnTP(start_win_num));
				iowrite32(0, pcie->reg_base + RCAPnSAH(start_win_num));
				iowrite32((cpu_addr & 0xFFFFF000) | ((bit_size - 1) << 1) | RCA_WIN_EN, pcie->reg_base + RCAPnSAL(start_win_num));
				break;
			case DT_TYPE_MEM32:
				iowrite32(pci_addr, pcie->reg_base + RCAPnTAL(start_win_num));
				iowrite32(0, pcie->reg_base + RCAPnTAH(start_win_num));
				iowrite32(TRSF_PARAM_MEMORY | TRSL_ID_PCIE_TX_RX, pcie->reg_base + RCAPnTP(start_win_num));
				iowrite32(0, pcie->reg_base + RCAPnSAH(start_win_num));
				iowrite32((cpu_addr & 0xFFFFF000) | ((bit_size - 1) << 1) | RCA_WIN_EN, pcie->reg_base + RCAPnSAL(start_win_num));
				break;
			default:
				debug("Unknown flages = %x\n", DT_FLAGS_TO_TYPE(flags));
				break;
		}
	}

	/* Initial PCI-to-AXI windows */
	for ( i = 0;; i++ )
	{
		if(pci_get_dma_regions(pcie->dev, &dma_ranges, i) == 0) {
			debug("%s: region %d, pci_addr=%lx, addr=%llx, size=%lx\n",
					__func__, i, dma_ranges.bus_start, dma_ranges.phys_start, dma_ranges.size);
			set_translation_window(pcie, i, dma_ranges.phys_start,
					dma_ranges.size,
					dma_ranges.bus_start,
					PLDA_XPRESS_RICH_MEMORY_WINDOW,
					PLDA_XPRESS_RICH_TARGET_AXI_MASTER);
		}else{
			break;
		}
	}

	return;
}


/* Probe Function */
static int npcm_pcie_probe(struct udevice *dev)
{
	struct npcm_pcie *pcie = dev_get_priv(dev);
	int node = dev_of_offset(dev);
	int ret;

	pcie->gcr_base = syscon_regmap_lookup_by_phandle(dev, "syscon-gcr");
	if (IS_ERR_OR_NULL(pcie->gcr_base)) {
		dev_err(dev, "Failed to get syscon-gcr\n");
		return PTR_ERR(pcie->gcr_base);
	}

	pcie->reg_base = dev_read_addr_ptr(dev);
	if (IS_ERR_OR_NULL(pcie->reg_base)){
		dev_err(dev, "Failed to get address\n");
		return -ENOENT;
	}

	ret =  dev_read_resource(dev, 1, &pcie->res);
	if (ret) {
		dev_err(dev, "Failed to read resource\n");
		return ret;
	}

	pcie->config_base = devm_ioremap(dev, pcie->res.start, resource_size(&pcie->res));
	if (!pcie->config_base) {
		dev_err(dev, "Failed to map config_base\n");
		return -ENOMEM;
	}

	ret = reset_get_by_index(dev, 0, &pcie->reset);
	if (ret) {
		dev_err(dev, "Failed to get reset control\n");
		return ret;
	}

	ret = gpio_request_by_name_nodev(offset_to_ofnode(node), "pci-ep-rst", 0,
			&pcie->ep_rst, GPIOD_IS_OUT| GPIOD_ACTIVE_LOW);
	if (ret) {
		dev_err(dev, "Failed to find ep-gpios property\n");
		return ret;
	}
	pcie->dev = dev;

	dm_gpio_set_value(&pcie->ep_rst, 1);

	npcm_initialize_as_root_complex(pcie);
	npcm_pcie_rc_init_config_window(pcie);

	dm_gpio_set_value(&pcie->ep_rst, 0);
	dm_gpio_free(dev, &pcie->ep_rst);

	return 0;
}

static const struct udevice_id npcm_pcie_ids[] = {
	{ .compatible = "nuvoton,npcm845-pcie" },
	{ }
};

U_BOOT_DRIVER(pcie_npcm) = {
	.name           = "pcie_npcm",
	.id             = UCLASS_PCI,
	.of_match       = npcm_pcie_ids,
	.ops		= &npcm_pcie_ops,
	.probe          = npcm_pcie_probe,
	.priv_auto      = sizeof(struct npcm_pcie),
};
