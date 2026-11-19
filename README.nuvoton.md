# U-Boot for Nuvoton NPCM BMC SoCs

Nuvoton's fork of [U-Boot](https://source.denx.de/u-boot/u-boot) (v2024.10 base) with support for the NPCM9xx (Tavor) BMC. The upstream documentation is in `README` and `doc/`.

## Target

Tavor (NPCM9xx, AArch64): `configs/tavor_veloce_*_defconfig` (Veloce emulation variants) and the Tavor SVB board.

## Build

```sh
source ~/tavor-toolchain/env.sh   # user-level toolchain, bison/flex, no sudo

make tavor_veloce_fiu_mmc_moc_ddr_usb_net_XIP.flash_defconfig
make CROSS_COMPILE=aarch64-none-linux-gnu- -j$(nproc)
```

The result is `u-boot.bin`.

## Layout

| Path | Contents |
|---|---|
| `arch/arm/mach-npcm/npcm9xx` | SoC code |
| `board/nuvoton/tavor_evb` | Board code |
| `arch/arm/dts/nuvoton-*` | Device trees and `*-u-boot.dtsi` overlays |
| `drivers/net/dwc_eth_xgmac_npcm9xx.c`, `pcs-xpcs-npcm9xx.c` | XGMAC and XPCS Ethernet drivers. The XPCS driver follows the Linux `pcs-xpcs` structure (Clause 45 style accessors, compat table, PHY-specific PMA tables for SGMII and USXGMII) |
| `drivers/mmc/npcm_sdhci.c` | eMMC (DWC MSHC) driver. The PHY release and pad setup run in the `post_reset` op, which `sdhci_init()` calls after the controller reset |
| `cmd/nuvoton/` | Debug commands (`nu_mmc`, `nu_gmac`, ...) |

## Deploy

On the Tavor SVB, U-Boot for XIP is stored in SPI flash CS1 at offset `0x200000` (executes in place at `0x30200000`). The U-Boot that runs from DDR (built with `Type DDR`, e.g. `tavor_veloce_fiu_mmc_moc_ddr_usb_net_defconfig`) is stored at offset `0x300000`. Steps are in `.github/copilot-instructions.md`.

## Testing notes

- Network: `setenv ipaddr`, `setenv serverip`, `setenv ethact eth@44800000`, then `ping` and `tftp`. `nu_gmac 2` enables the XPCS serial loopback.
- eMMC: `mmc dev 0`, `mmc info`, `mmc read`.

## Contributing

Patches follow the upstream process (mailing list, Linux kernel coding style, `Signed-off-by`). See `.github/pull_request_template.md` and https://u-boot.readthedocs.io/en/latest/develop/sending_patches.html.

## License

GPL-2.0+ (see `Licenses/`).
