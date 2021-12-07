/*
 *  Copyright (c) 2017 Nuvoton Technology Corp.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __POLEG_CLOCK_H_
#define __POLEG_CLOCK_H_

enum {
	APB1  = 1,
	APB2  = 2,
	APB3  = 3,
	APB4  = 4,
	APB5  = 5,
	SPI0  = 10,
	SPI3  = 13,
};

/* Clock Enable 1 Register (CLKEN1) */
#define CLKEN1_TIMER0_4             19
#define CLKEN1_TIMER5_9             20
#define CLKEN1_EMC1                 6

/* Clock Enable 2 Register (CLKEN2) */
#define CLKEN2_GMAC1                28
#define CLKEN2_USBIF                27
#define CLKEN2_USBHOST              26
#define CLKEN2_GMAC2                25
#define CLKEN2_SDHC                 9
#define CLKEN2_MMC                  8

/* Clock Select Register (CLKSEL) */
#define CLKSEL_DVCSSEL              23
#define CLKSEL_GFXMSEL              21
#define CLKSEL_CLKOUTSEL            18
#define CLKSEL_GFXCKSEL             16
#define CLKSEL_TIMCKSEL             14
#define CLKSEL_MCCKSEL              12
#define CLKSEL_SUCKSEL              10
#define CLKSEL_UARTCKSEL            8
#define CLKSEL_SDCKSEL              6
#define CLKSEL_PIXCKSEL             4
#define CLKSEL_GPRFSEL              2
#define CLKSEL_CPUCKSEL             0

/* Clock Divider Control Register 1 (CLKDIV1) */
#define CLKDIV1_ADCCKDIV            28
#define CLKDIV1_CLK4DIV             26
#define CLKDIV1_TIMCKDIV            21
#define CLKDIV1_UARTDIV             16
#define CLKDIV1_MMCCKDIV            11
#define CLKDIV1_AHB3CKDIV           6
#define CLKDIV1_PCICKDIV            2
#define CLKDIV1_CLK2DIV             0

/* Clock Divider Control Register 2 (CLKDIV2) */
#define CLKDIV2_APB4CKDIV           30
#define CLKDIV2_APB3CKDIV           28
#define CLKDIV2_APB2CKDIV           26
#define CLKDIV2_APB1CKDIV           24
#define CLKDIV2_APB5CKDIV           22
#define CLKDIV2_CLKOUTDIV           16
#define CLKDIV2_GFXCKDIV            13
#define CLKDIV2_SUCKDIV             8
#define CLKDIV2_SU48CKDIV           4
#define CLKDIV2_SD1CKDIV            0

/* Clock Divider Control Register 3 (CLKDIV3) */
#define CLKDIV3_SPI0CKDV	    6
#define CLKDIV3_SPIXCKDV	    1

/* PLL Control Register 2 (PLLCON2) */
#define PLLCONn_LOKI                31
#define PLLCONn_LOKS                30
#define PLLCONn_FBDV                16
#define PLLCONn_OTDV2               13
#define PLLCONn_PWDEN               12
#define PLLCONn_OTDV1               8
#define PLLCONn_INDV                0

/* CPUCKSEL (CPU/AMBA/MC Clock Source Select Bit) */
#define CLKSEL_CPUCKSEL_PLL0        0x00   /* 0 0: PLL0 clock*/
#define CLKSEL_CPUCKSEL_PLL1        0x01   /* 0 1: PLL1 clock */
#define CLKSEL_CPUCKSEL_CLKREF      0x02   /* 1 0: CLKREF input (25 MHz, default) */
#define CLKSEL_CPUCKSEL_SYSBPCK     0x03   /* 1 1: Bypass clock from pin SYSBPCK */

/* UARTCKSEL (Core and Host UART Clock Source Select Bit). */
#define CLKSEL_UARTCKSEL_PLL0       0x00  /* 0 0: PLL0    clock. */
#define CLKSEL_UARTCKSEL_PLL1       0x01  /* 0 1: PLL1    clock. */
#define CLKSEL_UARTCKSEL_CLKREF     0x02  /* 1 0: CLKREF  clock (25 MHz, default). */
#define CLKSEL_UARTCKSEL_PLL2       0x03  /* 1 1: PLL2    clock divided by 2. */

/* SDCKSEL (SDHC Clock Source Select Bit). */
#define CLKSEL_SDCKSEL_PLL0         0x00   /* 0 0: PLL0    clock.  */
#define CLKSEL_SDCKSEL_PLL1         0x01   /* 0 1: PLL1    clock.  */
#define CLKSEL_SDCKSEL_CLKREF       0x02   /* 1 0: CLKREF clock (25 MHz, default).  */
#define CLKSEL_SDCKSEL_PLL2         0x03   /* 1 1: PLL2    clock divided by 2.  */

/* TIMCKSEL (Timer Clock Source Select Bit) */
#define CLKSEL_TIMCKSEL_PLL0        0x00   /*  0 0: PLL0    clock.  */
#define CLKSEL_TIMCKSEL_PLL1        0x01   /*  0 1: PLL1    clock.  */
#define CLKSEL_TIMCKSEL_CLKREF      0x02   /*  1 0: CLKREF clock (25 MHz, default).  */
#define CLKSEL_TIMCKSEL_PLL2        0x03   /*  1 1: PLL2    clock divided by 2.  */

/* IP Software Reset Register 1 (IPSRST1), offset 0x20 */
#define IPSRST1_USBDEV1             5
#define IPSRST1_EMC1                6
#define IPSRST1_USBDEV2             8
#define IPSRST1_USBDEV3             25
#define IPSRST1_USBDEV4             22
#define IPSRST1_USBDEV5             23
#define IPSRST1_USBDEV6             24

/* IP Software Reset Register 2 (IPSRST2), offset 0x24 */
#define IPSRST2_GMAC1               28
#define IPSRST2_GMAC2               25
#define IPSRST2_USBHOST             26
#define IPSRST2_SDHC                9
#define IPSRST2_MMC                 8

/* IP Software Reset Register 3 (IPSRST3), offset 0x34 */
#define IPSRST3_USBPHY1             24
#define IPSRST3_USBPHY2             25
#define IPSRST3_USBHUB              8
#define IPSRST3_USBDEV9             7
#define IPSRST3_USBDEV8             6
#define IPSRST3_USBDEV7             5
#define IPSRST3_USBDEV0             4

/* Watchdog 0 Reset Control Regoster, offset 0x38 */
#define WDORCR_GPIO_M0 16
#define WDORCR_GPIO_M1 17
#define WDORCR_GPIO_M2 18
#define WDORCR_GPIO_M3 19
#define WDORCR_GPIO_M4 20
#define WDORCR_GPIO_M5 21
#define WDORCR_GPIO_M6 22
#define WDORCR_GPIO_M7 23
#endif
