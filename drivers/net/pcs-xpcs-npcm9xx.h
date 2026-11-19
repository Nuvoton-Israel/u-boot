/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __PCS_XPCS_NPCM9XX_H
#define __PCS_XPCS_NPCM9XX_H

#include <linux/types.h>

struct udevice;

int npcm9xx_xpcs_configure(struct udevice *dev);
int npcm9xx_xpcs_set_pma_loopback(struct udevice *dev, bool on);

#endif
