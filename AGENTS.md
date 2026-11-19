# AGENTS.md

Guidance for AI agents working in this repository (Nuvoton U-Boot fork, v2024.10 base, Tavor BMC SoC). Full instructions, including the Tavor SVB hardware workflow, are in `.github/copilot-instructions.md`. Read it first. A short project overview is in `README.nuvoton.md`.

## Scope

Tavor (NPCM9xx, AArch64), **silicon Z1** only. Ignore Poleg/Arbel and A1.
For specs use the pdftomd RAG skill with Z1 docs only (latest: `NPCM9mnx_AS_Rev.0.75_Z1_new`); never use `*_A1` docs.

## Build

```sh
source ~/tavor-toolchain/env.sh
make tavor_veloce_fiu_mmc_moc_ddr_usb_net_XIP.flash_defconfig   # or another tavor_veloce_* defconfig
make CROSS_COMPILE=aarch64-none-linux-gnu- -j$(nproc)
```

Output: `u-boot.bin`. Code must build warning-free (CI uses `-Werror`).

## Tests

Standard U-Boot sandbox tests: `./test/py/test.py --bd sandbox --build -k <test_name>`. There is no NPCM-specific suite; hardware validation is on Tavor SVB/Veloce over serial.

## Conventions

- Upstream U-Boot style: Kconfig/Kbuild, driver model, Linux kernel coding style, `subsystem: summary` commit subjects with `Signed-off-by`.
- New board or SoC support needs a Kconfig entry, a board `MAINTAINERS` file, a defconfig and a DT. Drivers are wired through `obj-$(CONFIG_...)` in the relevant `Makefile`.
- Patches go to the mailing list, not GitHub PRs.

## Tavor SVB hardware

Details are in `/mnt/storage/work/flash-prog/AGENTS.md` and the Tavor SVB section of `.github/copilot-instructions.md`. Key rules:
- Console is `/dev/ttyUSB2`. The baud rate depends on the BootBlock: 115200 with BootBlock 0.2.9+ (SVB-03, UART clock 24 MHz), 750000 with older ones. `tavor-svb-deploy.sh` auto-detects it (`BAUD=auto`). Ports are exclusive, so close serial sessions when done.
- CS1 flash size varies per board (32 MiB or 256 MiB); a full backup (`tavor-svb-deploy.sh -F`) takes ~4 minutes on 256 MiB.
- Reset with `scripts/tavor-svb-porst.sh` (in the flash-prog directory) or dbgprobe `board.porst` with `board="tavor-svb"`.
- Never read `CPU_CORESIGHT_BA` (`0x5F800000`+) via the AXI-AP; it hangs it until PORST.
- The board boots from SPI flash CS1. Back up with a full dump before writing, and confirm the CS first.

## Deploying

U-Boot is flashed to boot flash **CS1 at offset `0x200000`** (XIP at `0x30200000`) with `FTDI_SPI_PROG` while the BMC is held in reset. Use `scripts/tavor-svb-deploy.sh [-n] [-y] [-F] [u-boot.bin]` (backs up, programs, verifies, resets and checks the console). By default it backs up only the 1 MiB window at `0x200000` and programs only the 4 KiB sectors that differ (about 6 s for a small change, 22 s for a full rewrite); `-F` backs up the whole CS1 first. Program only `u-boot.bin` at `0x200000`. Full steps are in the "Deploying U-Boot to the Tavor SVB" section of `.github/copilot-instructions.md`.

## Collaterals

Tavor RTL (Z1) sources: `/mnt/tanap1/homes_sw/softw/softw/vlevyato/workspace/rtl/design/` (subdirs `top`, `common`, `ip`, `a35_wrapper_hlb`, `caliptra_wrapper_hlb`, `mc_ddr_hlb`, `pcie_ep_hlb`, `usb_hlb`). Use it as the reference for register maps and peripheral behaviour when writing drivers.

## xgmac lpbk (1G PMA serial loopback)

`cmd/nuvoton/xgmac_lpbk.c`: `xgmac lpbk on|off|test`. `on`/`off` set `g_gmac_loopback` (2/0) and, if the
PHY is already up, toggle `VR_PMA_MISC_CTRL0[0]` via `npcm9xx_xpcs_set_pma_loopback()`. `test` starts the
MAC (which re-runs the XPCS init and applies the loopback), sends a 64-byte DMA frame and compares RX.
Verified on SVB (Z1, menu 2): `on` + `test` -> PASS (repeatable), `off` + `test` -> FAIL (no frame).

XPCS access facts (`drivers/net/pcs-xpcs-npcm9xx.c`):
- APB3_32 indirect access: 23-bit address = `{MMD, reg} << 2`; bits [22:10] go to PCS+0x3FC, [9:0] are the
  offset. The register defines use the Z1 spec table values (`<< 3`), so read/write convert with `>> 1`.
  Segments: PMA VR 0x180 (SRAM at 0x26C, MISC_CTRL0 at 0x240), PCS SR 0x300, PCS VR 0x380, MII 0x1F00.
- Until `GMACSS_XGMAC_CTRL.CONFIG_DONE` is set and the PHY SRAM handshake (wait `SRAM.INIT_DN`, set
  `EXT_LD_DN`) is done, the XPCS is in reset and reads 0x8004. Init order follows the UVM 1G sequence.
- TX2RX loopback must be off across `DIG_CTRL1.VR_RST`, or the lane does not recover on a second start.

## eMMC (sdhci0, DWC MSHC @ 0x2230000)
- `npcm945_sdhci_init()` in `drivers/mmc/npcm_sdhci.c`: release reset (rst+0x1600 BIT29), set CFG_Done (0x22c100c BIT1), wait PHY PWRGOOD, set `PHY_RSTN`.
- Pad RXSEL must be 1 (1.8 V); with 0 the CMD/DAT lines read low (CMD0 timeout). It only latches after `PHY_RSTN` and is cleared again by bus power-up, so it is re-applied from `set_ios_post`.
- Base clock is 200 MHz (`clock-frequency`); CAPABILITIES base-clock field is wrong. The old generic SDHCI divider-doubling workaround made a 400 kHz request run at 200 kHz; it is removed. HS52 requests yield 50 MHz with the integer divider. HS200+ remain masked via `sdhci-caps-mask` (no tuning yet).
- `npcm945_sdhci_set_delay()` programs `SDCLKDL_DC` (0x31e) to 0x40 while SDHCI has the card clock gated, asserting then clearing `SDCLKDL_CNFG.UPDATE_DC` (0x31d bit 4). The reset delay code of zero caused 4/8-bit write failures; correct clock/data timing fixes them without changing drive strength or slew.
- `initr_mmc` was commented out in `common/board_r.c`; it is needed for `mmc` to see devices.
- The board DTS enables 8-bit. Verified on SVB-03 with single- and eight-block write/read comparisons in 1/4/8-bit modes at 400 kHz, 25 MHz and 50 MHz, using zero, all-one and mixed patterns. Original contents at scratch LBA 0xe00000 were saved and restored. A DT `bus-width = <1>` still provides a fallback.
- Board RAM test buffer: use 0x10f0000 for `mmc read/write`.
