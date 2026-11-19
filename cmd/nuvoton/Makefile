
obj-$(CONFIG_ARCH_NPCM9XX) += benchmarks.o
obj-$(CONFIG_ARCH_NPCM9XX) += dry.o
obj-$(CONFIG_ARCH_NPCM9XX) += whetstone.o
obj-$(CONFIG_ARCH_NPCM9XX) += usb_mass_storage_nuvoton.o
obj-$(CONFIG_ARCH_NPCM9XX) += mmc_nuvoton.o
obj-$(CONFIG_ARCH_NPCM9XX) += cache_test.o
obj-$(CONFIG_ARCH_NPCM9XX) += gmac_test.o
obj-$(CONFIG_DWC_ETH_XGMAC_NUVOTON_NPCM9XX) += xgmac_lpbk.o
obj-$(CONFIG_ARCH_NPCM9XX) += unalign_test.o

ccflags-y += -DTIME


PLATFORM_CPPFLAGS := $(filter-out -mgeneral-regs-only,$(PLATFORM_CPPFLAGS))
PLATFORM_CPPFLAGS += -O3  -ffast-math

