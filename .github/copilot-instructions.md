# Copilot instructions

This is Nuvoton's fork of U-Boot (v2024.10 base). **The goal of this work is the Tavor (NPCM9xx, AArch64) SoC only, silicon revision Z1** (not A1); ignore Poleg/Arbel unless asked. Upstream U-Boot conventions apply (Kconfig/Kbuild, driver model, device tree, Linux kernel coding style). Patches go to the mailing list, not GitHub PRs.

## Build

Toolchain: `source ~/tavor-toolchain/env.sh` (user-level, no sudo; see `~/tavor-toolchain/README.md`). It provides the CI cross compiler (`aarch64-none-linux-gnu-`, Arm GNU 10.3) plus bison/flex. Add missing packages with `apt-get download <pkg>` + `dpkg-deb -x <deb> ~/tavor-toolchain/root`.

```sh
source ~/tavor-toolchain/env.sh
make tavor_veloce_fiu_mmc_moc_ddr_usb_net_XIP.flash_defconfig   # or another tavor_veloce_*_defconfig
make CROSS_COMPILE=aarch64-none-linux-gnu- -j$(nproc)                # CI adds KCFLAGS="-Werror"
```

Output is `u-boot.bin` (verified: builds clean from `tavor_veloce_fiu_mmc_moc_ddr_usb_net_XIP.flash_defconfig`). An in-tree `.config` blocks out-of-tree (`O=`) builds; build in a copy (`rsync -a --exclude=.git`) or `make mrproper` first (this deletes `.config`). Tavor defconfigs are `configs/tavor_veloce_*` (Veloce emulation variants: moc_ddr, usb, net, mmc, fiu, XIP/flash). Check `.config` to see which one the tree was last configured with. Don't commit `.config` or build output.

Tests are the stock U-Boot ones (`./test/py/test.py --bd sandbox --build -k <test_name>`). There is no NPCM-specific suite; validation is on Tavor SVB/Veloce over serial.

## Architecture

- Tavor SoC code: `arch/arm/mach-npcm/npcm9xx` (cpu init, reset); `ARCH_NPCM` is the common Kconfig.
- Board: `board/nuvoton/tavor_evb` plus shared `board/nuvoton/common` (UART). Defconfigs live in `configs/`.
- Device trees: `arch/arm/dts/nuvoton-*` (`nuvoton-common-npcm9xx.dtsi`, `nuvoton-npcm945.dtsi`, `nuvoton-npcm945-veloce.dts`, and the `nuvoton-npcm9xx-u-boot.dtsi` overlay for U-Boot-only properties). The default DT is set via `CONFIG_DEFAULT_DEVICE_TREE` in each defconfig. Note `dts/upstream` is present in the tree.
- Ethernet: Synopsys XGMAC driver `drivers/net/dwc_eth_xgmac_npcm9xx.c` with XPCS PCS support in `pcs-xpcs-npcm9xx.c` (Kconfig `DWC_ETH_XGMAC_NUVOTON_NPCM9XX`).

## Conventions

- Add new SoC/board support the upstream way: Kconfig entry + `MAINTAINERS` file in the board dir + defconfig + DT; wire drivers through Kconfig symbols in the relevant `Makefile` (`obj-$(CONFIG_...)`).
- Code must build warning-free (CI uses `-Werror`).
- Commit messages follow U-Boot style (`subsystem: summary`, `Signed-off-by`).

## Tavor SVB board (NPCM9xx hardware)

Source: `/mnt/storage/work/flash-prog/AGENTS.md` (read it for full details; scripts such as `scripts/tavor-svb-porst.sh` and `scripts/tavor-a35-openocd.sh` live in that directory).

- Board: Tavor SVB Rev A, FTDI FT4232H (0403:6011, serial `Tavor_SVB-02-1`): `/dev/ttyUSB0..3` = channels A..D.
- Console UART: `/dev/ttyUSB2`, 8N1, **115200 baud** with BootBlock 0.2.9+ (SVB-03), **750000** with older BootBlocks (`minicom -D /dev/ttyUSB2 -b 115200` or serial-mcp `serial_open`). Ports are exclusive; close the session (`serial_close`) when done. MCU console is `/dev/ttyUSB0` (750000); if the console is silent after PORST, check it first (A35 DAP is then unreachable).
- Reset: `scripts/tavor-svb-porst.sh [assert_ms]` (PORST = channel D DDBUS6, high = reset), or dbgprobe `board.porst`/`board.release` with `board="tavor-svb"`. Don't use Arbel channel A pins (4, 6) on the SVB. Power cycle (unverified): channel B BDBUS5.
- dbgprobe MCP (`http://127.0.0.1:8788/mcp`): `connect(backend="openocd", device="tavor-a35[.N]", probe_id=<J-Link serial>)`. The `jlink` backend cannot debug the A35. `mem.read`/`mem.write` use the AXI-AP and work while running. Needs the board booted (BootBlock running). Never read `CPU_CORESIGHT_BA` (`0x5F800000`+) via the AXI-AP; it hangs the AXI-AP until PORST.
- A35 JTAG is header JS2 (DAP has no APB-AP; needs the patched OpenOCD, `scripts/tavor-a35-openocd.sh build` then run it; GDB ports 3333..3336 = cores 0..3, telnet 4444).
- Boot flash: SPI NOR memory-mapped at `0x30000000` (MCU firmware), BootBlock at `0x30100000`. Flashing is over FTDI channel B (`FTDI_SPI_PROG -open-desc "Tavor_SVB_Rev_A B" -pin-set 6 1 1 -pin-set 7 1 0 ...`) while holding the BMC in reset (`scripts/tavor-svb-porst.sh 60000 &`). Both CS0/CS1 are W25Q256JW; **this board boots from CS1** (CS0 is blank), so verify the CS before writing. Back up with a full dump first. Channel B pin 5 high powers the board off.

## Deploying U-Boot to the Tavor SVB

U-Boot lives in the boot flash **CS1** (W25Q256JW, 32 MB) at offset **`0x200000`** (verified from a CS1 dump on 11 Nov 2026). It executes in place at `0x30200000` (`CONFIG_TEXT_BASE`). Flash layout: `0x000000` MCU RISC-V firmware, `0x100000` BootBlock, `0x200000` U-Boot (about 0x9b000 bytes), erased after `0x29b000`. Only touch `0x200000` onward when updating U-Boot; the other regions are the MCU firmware and BootBlock.

**Use the script** `scripts/tavor-svb-deploy.sh [-n] [-y] [-F] [-b backup.bin] [-c] [u-boot.bin]` (default image `./u-boot.bin`). It holds PORST, backs up the 1 MiB U-Boot window at `0x200000` (`-F`: the whole CS1), erases/programs/verifies only the 4 KiB sectors that differ from the image, compares a read-back, pulses PORST to release the reset, then drives the BootBlock menu on `/dev/ttyUSB2` (option 2 by default, `MENU=4` to train DDR first) and checks for the U-Boot prompt. `-n` is a dry run (backup only). Verified on 11 Nov 2026 with a fresh build (`U-Boot 2024.10 (Nov 11 2026 - 15:14:52 +0200)` reached `U-Boot>`). Close any serial session on the console first.

The BootBlock menu (options 1-5) waits on the console after reset; "XIP U-Boot" options jump to FIU3 CS1 + 2 MiB (`0x30200000`).

The manual steps, run from `/mnt/storage/work/flash-prog` (see the Tavor SVB section above for the pin details):

```sh
# 1. Hold the BMC in reset for the whole flash access (PORST on channel D)
scripts/tavor-svb-porst.sh 120000 &

# 2. Back up CS1 first (about 30 s)
./build/FTDI_SPI_PROG -open-desc "Tavor_SVB_Rev_A B" -pin-set 6 1 1 -pin-set 7 1 0 \
    -flash-info -dump-file cs1-backup.bin 0 -1

# 3. Program and verify u-boot.bin at 0x200000, then reset
./build/FTDI_SPI_PROG -open-desc "Tavor_SVB_Rev_A B" -pin-set 6 1 1 -pin-set 7 1 0 \
    -flash-info -verify-on -prog-file /path/to/u-boot.bin 0x200000 -reset
```

- `-pin-set 6 1 1` selects **CS1** (channel B pin 6 high; low would be CS0). `-pin-set 7 1 0` enables the SPI3 buffer. Never set channel B pin 5 high (powers the board off).
- Check the result on `/dev/ttyUSB2` (115200 or 750000, see above) (`serial_open`); the U-Boot banner version string should show the new build time. If the console stays silent, check the MCU console on `/dev/ttyUSB0`.
- Roll back by programming `cs1-backup.bin` at offset 0 with the same command.
- `-prog-file` uses read-modify-program and erases as needed, and accepts hex or decimal flash offsets (checked against `-help`). A verify failure returns exit code 1.

## Collaterals

- **Tavor RTL (Z1) sources:** `/mnt/tanap1/homes_sw/softw/softw/vlevyato/workspace/rtl/design/` (CIFS share `//tanap1/homes_sw`; may need to be mounted). Subdirectories: `top`, `common`, `ip`, `a35_wrapper_hlb`, `caliptra_wrapper_hlb`, `mc_ddr_hlb`, `pcie_ep_hlb`, `usb_hlb`, `arbel`. Use it as the reference for register maps, reset/clock behaviour and peripheral wiring (for example the XGMAC/XPCS and USB blocks) when writing or debugging drivers.
- **Flash/debug tooling and SVB notes:** `/mnt/storage/work/flash-prog/` (`AGENTS.md`).
- **XGMAC PHY loopback UVM sequences:** `/mnt/storage/uboot/*.txt` (10G USXGMII and 1G).

## Silicon revision and specs

We work with **Tavor Z1** silicon. For spec lookups (pdftomd RAG, skill `.github/skills/pdftomd-rag`) use Z1 documents only: latest is `NPCM9mnx_AS_Rev.0.75_Z1_new`. Never use `*_A1` documents (for example `NPCM9mnx_AS_Rev.0.77_A1`, which is newer but a different silicon), and never fill Z1 gaps from A1. Always filter searches by `doc_id`.
