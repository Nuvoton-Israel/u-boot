// SPDX-License-Identifier: GPL-2.0+
/*
 * xgmac lpbk: PMA serial (TX->RX) loopback of the NPCM9xx XGMAC multi-protocol
 * PHY at 1G, with an optional DMA test frame sent through the MAC.
 */

#include <command.h>
#include <dm.h>
#include <net.h>
#include <string.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/err.h>
#include "../../drivers/net/pcs-xpcs-npcm9xx.h"

#define LPBK_FRAME_LEN		64
#define LPBK_RX_TIMEOUT_MS	100
#define LPBK_ETHERTYPE		0x88b5	/* IEEE local experimental */

static struct udevice *xgmac_find_dev(void)
{
	struct udevice *dev;

	for (uclass_first_device(UCLASS_ETH, &dev); dev;
	     uclass_next_device(&dev)) {
		if (!strcmp(dev->driver->name, "eth_xgmac"))
			return dev;
	}

	return NULL;
}

static int xgmac_lpbk_test(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	const struct eth_ops *ops = eth_get_ops(dev);
	uchar tx[LPBK_FRAME_LEN], *rx = NULL;
	int ret, len = -EAGAIN, i;

	memset(tx, 0xff, 6);			/* broadcast */
	memcpy(tx + 6, pdata->enetaddr, 6);
	tx[12] = LPBK_ETHERTYPE >> 8;
	tx[13] = LPBK_ETHERTYPE & 0xff;
	for (i = 14; i < LPBK_FRAME_LEN; i++)
		tx[i] = i;

	ret = ops->start(dev);
	if (ret) {
		printf("xgmac start failed: %d\n", ret);
		return ret;
	}

	ret = eth_send_udev(dev, tx, LPBK_FRAME_LEN);
	if (ret) {
		printf("xgmac send failed: %d\n", ret);
		goto out;
	}

	for (i = 0; i < LPBK_RX_TIMEOUT_MS; i++) {
		len = ops->recv(dev, 0, &rx);
		if (len != -EAGAIN)
			break;
		mdelay(1);
	}

	if (len == -EAGAIN) {
		printf("loopback FAIL: no frame received within %d ms\n",
		       LPBK_RX_TIMEOUT_MS);
		ret = -ETIMEDOUT;
		goto out;
	}
	if (len < 0) {
		printf("loopback FAIL: recv error %d\n", len);
		ret = len;
		goto out;
	}

	if (len < LPBK_FRAME_LEN || memcmp(tx, rx, LPBK_FRAME_LEN)) {
		printf("loopback FAIL: sent %d bytes, received %d bytes, data mismatch\n",
		       LPBK_FRAME_LEN, len);
		print_hex_dump_bytes("rx: ", DUMP_PREFIX_OFFSET, rx, len);
		ret = -EBADMSG;
	} else {
		printf("loopback PASS: %d bytes sent and received intact\n",
		       LPBK_FRAME_LEN);
		ret = 0;
	}

	if (ops->free_pkt)
		ops->free_pkt(dev, rx, len);
out:
	ops->stop(dev);

	return ret;
}

static int do_xgmac(struct cmd_tbl *cmdtp, int flag, int argc,
		    char *const argv[])
{
	struct udevice *dev;
	int ret;

	if (argc != 3 || strcmp(argv[1], "lpbk"))
		return CMD_RET_USAGE;

	dev = xgmac_find_dev();
	if (!dev) {
		printf("no XGMAC ethernet device found\n");
		return CMD_RET_FAILURE;
	}

	if (!strcmp(argv[2], "on") || !strcmp(argv[2], "off")) {
		bool on = argv[2][1] == 'n';

		ret = npcm9xx_xpcs_set_pma_loopback(dev, on);
		if (ret) {
			printf("failed to set PMA loopback: %d\n", ret);
			return CMD_RET_FAILURE;
		}
		printf("PMA serial loopback %s\n", on ? "enabled" : "disabled");
		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[2], "test"))
		return xgmac_lpbk_test(dev) ? CMD_RET_FAILURE : CMD_RET_SUCCESS;

	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	xgmac, 3, 0, do_xgmac,
	"XGMAC PHY loopback (1G)",
	"lpbk on   - enable PMA serial loopback (TX->RX) in the multi-PHY\n"
	"                  (applied now if the PHY is up, else at next start)\n"
	"xgmac lpbk off  - disable it\n"
	"xgmac lpbk test - send a DMA test frame and compare the received one"
);
