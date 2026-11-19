Tavor NPCM9xx XGMAC and BCM84991
==============================

The Tavor device tree enables XGMAC with a fixed 1 Gb/s SGMII link and a
125 MHz reference clock, matching the imported Ethernet configuration.
For an external 125 MHz reference, add ``nuvoton,ref-clock-from-pad``.
USXGMII requires ``phy-mode = "usxgmii"``, an external 156250000 Hz
``nuvoton,ref-clock-frequency``, ``nuvoton,ref-clock-from-pad``, and a
fixed-link speed of 2500, 5000 or 10000. These settings must match the board
wiring and external PHY configuration; a fixed link does not report the
external copper link's negotiated speed.

Build the existing DRAM configuration (not the older branch's XIP image)::

    make tavor_veloce_fiu_mmc_moc_ddr_usb_net_defconfig
    make CROSS_COMPILE=aarch64-none-linux-gnu- -j$(nproc)

The image is ``u-boot.bin``. No flashing or board interaction is part of
compilation.

This configuration enables LMB, which is required by the newer U-Boot's
``booti`` command. Linux images may be loaded into RAM over TFTP and booted
with ``booti`` using the kernel, device tree and ramdisk addresses appropriate
to the image set. This does not require writing flash or saving environment.

The on-board eMMC is enabled as a non-removable 8-bit device. The NPCM945
driver releases the reset hold, initializes the 1.8 V PHY pads and programs
the card-clock delay required for wide-bus operation. Its input clock is
200 MHz; HS200 and HS400 remain masked until PHY tuning is implemented.
``mmc list`` and ``mmc info`` inspect the device without writing storage.

The ``bcmphy`` command provides BCM84991 clause 45 MDIO access and firmware
download. Select the XGMAC MDIO bus using ``mii device`` before issuing
``bcmphy`` commands. ``bcmphy init`` selects the MDC/MDIO pin functions;
``bcmphy fw <phy>`` downloads the embedded source-branch firmware, while
``bcmphy fw <phy> <addr> <len>`` downloads an image already in memory.
PHY addresses and register arguments are hexadecimal. ``bcmphy id``,
``scan``, ``rd`` and ``lpbk`` deliberately retain their requested result
output; MDIO transaction tracing and firmware progress dots are disabled.
Firmware loading is explicit, not performed automatically at boot.

The imported ``xgmac lpbk`` and ``bcmphy`` diagnostic commands remain
available for later authorized board work. Building them does not execute
them. Loopback mode 1 selects MAC loopback; mode 2 selects XPCS PMA loopback.

For UART-only TFTP diagnosis, ``setenv nettrace yes`` enables XGMAC
descriptor status and the first 64 bytes of the first 16 packets in each
direction, plus TFTP block/port information and UDP send results. An RX-ring
snapshot is printed every five seconds while waiting for a descriptor.
Tracing changes packet timing and is not evidence that a timing-sensitive
fault is fixed. ``setenv nettrace`` disables tracing for the next network
command; no ``saveenv`` is needed. Normal operation remains quiet.

Each NPCM9xx network start resets the MAC/DMA after configuring XPCS and
waits for reset completion before rebuilding descriptor rings. This avoids
reusing DMA state from an earlier network command. Transmit and stop waits
are bounded to 100 milliseconds; a transmit timeout reports descriptor and
DMA status even when tracing is disabled.

Sequential RX DMA is enabled by default to avoid the observed normal-mode
DMA fault. This selects the IP's debug mode, disabling descriptor prefetch
and overlap; it is an experimental workaround, not a proven production fix.
``setenv xgmac_rx_no_sequential yes`` selects normal parallel mode on the
next network start. ``setenv xgmac_rx_no_sequential`` restores the sequential
default on the next start. The old ``xgmac_rx_sequential`` variable is no
longer used. These changes are temporary unless saved with ``saveenv``.
Fatal DMA bus errors report status and the current RX buffer address once
per network command, and invalid receive data is not passed to the stack.
The receive buffer-size register encodes the byte size shifted by one, with
the low three size bits read-only; it must match the allocated buffer size.

Ethernet transmit truncation
----------------------------

All Ethernet transmit callers use ``eth_send_udev()``, including the legacy
stack, lwIP, DSA master forwarding and the XGMAC loopback command. Frames are
unconditionally truncated to 507 software-supplied bytes, including Ethernet
headers and software-supplied tags. Shorter frames are unchanged. Four bytes
are reserved for the MAC-generated FCS, giving at most 511 bytes from the
destination MAC address through FCS. Preamble, SFD and inter-frame gap are
excluded. This assumes the MAC appends only the four-byte FCS, with no extra
hardware-inserted tags; a physical wire-size cap has not been verified.

This is raw truncation, not fragmentation or MTU negotiation. Protocol length
fields and checksums are not repaired, so oversized IP/UDP/TCP packets become
invalid and may be discarded by the receiver. Receive sizes are unchanged;
this limit does not fix the RX DMA fault or reduce incoming TFTP DATA packets.
