/*
 * Copyright (c) 2016-2021 Nuvoton Technology Corp.
 *
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gcr.h>
#include <asm/arch/uart.h>
#include <asm/mach-types.h>
#include <asm/arch/clock.h>
#include <asm/arch/otp.h>
#include <asm/arch/info.h>
#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <clk.h>
#include <fuse.h>
#include <spi_flash.h>
#include <spi.h>
#include <asm/gpio.h>
#include <asm/arch/arbel_espi.h>

DECLARE_GLOBAL_DATA_PTR;

extern void sdelay(unsigned long loops);

/*---------------------------------------------------------------------------------------------------------*/
/* Function:        CLK_GetPLL0toAPBdivisor                                                                */
/*                                                                                                         */
/* Parameters:      apb: number of APB                                                                     */
/* Returns:                                                                                                */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine returns the value achieved by dividing PLL0 frequency to APB frequency    */
/*---------------------------------------------------------------------------------------------------------*/
u32  CLK_GetPLL0toAPBdivisor(u32 apb)
{
    struct clk_ctl *clkctl = (struct clk_ctl *)(uintptr_t)npcm850_get_base_clk();

	volatile u32 apb_divisor = 1;

	apb_divisor = apb_divisor * ((readl(&clkctl->clkdiv1) & (0x1)) + 1);         /* AXI divider ( div by 1\2) */
	apb_divisor = apb_divisor * (((readl(&clkctl->clkdiv1) >> 26 ) & (0x3)) + 1);       /* AHBn divider (div by 1\2\3\4) */

	switch (apb)
	{
        case APB1:
            apb_divisor = apb_divisor * (1 << ((readl(&clkctl->clkdiv2) >> 24 ) & (0x3)));     /* APB divider */
            break;
        case APB2:
            apb_divisor = apb_divisor * (1 << ((readl(&clkctl->clkdiv2) >> 26 ) & (0x3)));     /* APB divider */
            break;
        case APB3:
            apb_divisor = apb_divisor * (1 << ((readl(&clkctl->clkdiv2) >> 28 ) & (0x3)));     /* APB divider */
            break;
        case APB4:
            apb_divisor = apb_divisor * (1 << ((readl(&clkctl->clkdiv2) >> 30 ) & (0x3)));     /* APB divider */
            break;
        case APB5:
            apb_divisor = apb_divisor * (1 << ((readl(&clkctl->clkdiv2) >> 22 ) & (0x3)));     /* APB divider */
            break;
        default:
            apb_divisor = 0xFFFFFFFF;
            break;
	}

	return apb_divisor;
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:        CLK_ConfigureUartClock                                                                 */
/*                                                                                                         */
/* Parameters:      none                                                                                   */
/* Returns:                                                                                                */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine configures the Uart clock source to be closest to 24MHz by                */
/*                  modifying the UART divider.                                                            */
/*                  In _PALLADIUM_ bypass mode the UART input frequency is set to be highest possible -    */
/*                  same as APB frequency                                                                  */
/*---------------------------------------------------------------------------------------------------------*/
u32 CLK_ConfigureUartClock(void)
{
    struct clk_ctl *clkctl = (struct clk_ctl *)(uintptr_t)npcm850_get_base_clk();
    u32 uart_clk = 0;

	/*-------------------------------------------------------------------------------------------------*/
	/* Set UART to 24MHz. Source is PLL2 (960Mhz), which is divided by 2, so we get 24Mhz = 960/2/20   */
	/*-------------------------------------------------------------------------------------------------*/
#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
	u32 uartDesiredFreq  = 25000000; /*Hz */
#else
	u32 uartDesiredFreq  = 24000000; /*Hz */
#endif
	/*-----------------------------------------------------------------------------------------------------*/
	/* Normal configuration - UART from PLL0 with divider calculated from PLL0 configuration to get 24MHz  */
	/*-----------------------------------------------------------------------------------------------------*/

	/*-------------------------------------------------------------------------------------------------*/
	/* Calculate the divider given PLL2 output and desired frequency:                                  */
	/*-------------------------------------------------------------------------------------------------*/

#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
	u32 pllFreq = 500000000/2; //CLK_GetPll2Freq();
#else
	u32 pllFreq = 960000000/2; //CLK_GetPll2Freq();
#endif
	u32 uartDiv = pllFreq/uartDesiredFreq;

	uart_clk = pllFreq / uartDiv;

	/*----------------------------------------------------[]---------------------------------------------*/
	/* Set divider:                                                                                    */
	/*-------------------------------------------------------------------------------------------------*/
	writel(((readl(&clkctl->clkdiv1) & ~(0x1F << 16)) | ((uartDiv-1) << 16)), &clkctl->clkdiv1);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Setting PLL2 source clock                                                                           */
    /*-----------------------------------------------------------------------------------------------------*/
	writel((readl(&clkctl->clksel) | (0x3 << 8)), &clkctl->clksel);  // SRC_PLL2 = 0x03

	/*-----------------------------------------------------------------------------------------------------*/
	/* Wait for 200 clock cycles between clkDiv change and clkSel change:                                  */
	/*-----------------------------------------------------------------------------------------------------*/
	sdelay(420000UL); 	/* udelay(200) */

	return uart_clk;

}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:        UART_SetParity                                                                         */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*                  parity -                                                                               */
/*                                                                                                         */
/* Returns:         none                                                                                   */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine sets parity configuration                                                 */
/*---------------------------------------------------------------------------------------------------------*/
int UART_SetParity(UART_PARITY_T parity)
{
	struct npcmX50_uart *uart = (struct npcmX50_uart *)(uintptr_t)npcm850_get_base_uart();

    if (parity != UART_PARITY_NONE)
    {
        /*-------------------------------------------------------------------------------------------------*/
        /* Parity enable, choosing type                                                                    */
        /*-------------------------------------------------------------------------------------------------*/
		writel((readl(&uart->lcr) | (1 << 3)), &uart->lcr);  // LCR_PBE


        if (parity == UART_PARITY_EVEN)
        {
			writel((readl(&uart->lcr) | (1 << 4)), &uart->lcr);  // LCR_EPE

        }
        else if (parity == UART_PARITY_ODD)
        {
			writel((readl(&uart->lcr) & ~(1 << 4)), &uart->lcr);  // LCR_EPE
        }

    }
    else  //No parity
    {
		writel((readl(&uart->lcr) & ~(1 << 3)), &uart->lcr);  // LCR_PBE
    }

    return 0;
}


/*---------------------------------------------------------------------------------------------------------*/
/* Function:        UART_SetBitsPerChar                                                                    */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*                  bits -                                                                                 */
/*                                                                                                         */
/* Returns:         none                                                                                   */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine set bits per char                                                         */
/*---------------------------------------------------------------------------------------------------------*/
int UART_SetBitsPerChar(unsigned int  bits)
{
	struct npcmX50_uart *uart = (struct npcmX50_uart *)(uintptr_t)npcm850_get_base_uart();

    switch (bits)
    {
        case 5:   writel(((readl(&uart->lcr) & ~(0x03)) | LCR_WLS_5bit ), &uart->lcr);   break;
        case 6:   writel(((readl(&uart->lcr) & ~(0x03)) | LCR_WLS_6bit ), &uart->lcr);   break;
        case 7:   writel(((readl(&uart->lcr) & ~(0x03)) | LCR_WLS_7bit ), &uart->lcr);   break;
        default:
        case 8:   writel(((readl(&uart->lcr) & ~(0x03)) | LCR_WLS_8bit ), &uart->lcr);   break;
    }

    return 0;
}


/*---------------------------------------------------------------------------------------------------------*/
/* Function:        UART_SetBaudrate                                                                       */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*                  baudrate -                                                                             */
/*                                                                                                         */
/* Returns:         none                                                                                   */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine sets new baudrate                                                         */
/*---------------------------------------------------------------------------------------------------------*/
int UART_SetBaudrate(UART_BAUDRATE_T baudrate)
{
	struct npcmX50_uart *uart = (struct npcmX50_uart *)(uintptr_t)npcm850_get_base_uart();
    s32         divisor     = 0;
    u32         uart_clock  = 0;
    int		ret         = 0;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Configuring UART clock                                                                              */
    /*-----------------------------------------------------------------------------------------------------*/
    uart_clock = CLK_ConfigureUartClock();

    /*-----------------------------------------------------------------------------------------------------*/
    /* Computing the divisor for the given baudrate.                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    divisor = ((s32)uart_clock / ((s32)baudrate * 16)) - 2;

    // since divisor is rounded down check if it is better when rounded up
    if ( ((s32)uart_clock / (16 * (divisor + 2)) - (s32)baudrate) >
         ((s32)baudrate - (s32)uart_clock / (16 * ((divisor+1) + 2))) )
    {
        divisor++;
    }

    if (divisor < 0 )
    {
        divisor = 0;
        ret = -1;
    }

#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
	/* Maximum Baudrate possible = PLL2_CLK/2/10/32/COMPILATION_RATIO = Baudrate  */
	/* Maximum Baudrate possible = 960000000/2/10/32/2079 = 722  */
	/* Maximum Baudrate possible = 500000000/2/10/32/1040 = 752  */
    divisor = 0;

    ret = 0;
#endif

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set baud rate to baudRate bps                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
	writel((readl(&uart->lcr) | (1 << 7)), &uart->lcr);  // set LCR_DLAB
	writeb(divisor & 0xff, &uart->dll);
	writeb(divisor >> 8, &uart->dlm);
	writel((readl(&uart->lcr) & ~(1 << 7)), &uart->lcr);  // clr LCR_PBE

    return ret;
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:        UART_SetStopBit                                                                        */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*                  devNum -                                                                               */
/*                  stopbit -                                                                              */
/*                                                                                                         */
/* Returns:         none                                                                                   */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine sets number of stopbits                                                   */
/*---------------------------------------------------------------------------------------------------------*/
int UART_SetStopBit(UART_STOPBIT_T stopbit)
{
	struct npcmX50_uart *uart = (struct npcmX50_uart *)(uintptr_t)npcm850_get_base_uart();

    if (stopbit == UART_STOPBIT_1)
    {
		writel((readl(&uart->lcr) & ~(1 << 2)), &uart->lcr);  // LCR_NSB
    }
    else if (stopbit == UART_STOPBIT_DYNAMIC)
    {
		writel((readl(&uart->lcr) | (1 << 2)), &uart->lcr);  // LCR_NSB
    }

    return 0;
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:        CHIP_Mux_Uart                                                                          */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*                  CoreSP -                                                                               */
/*                  redirection_mode -                                                                     */
/*                  sp1 -                                                                                  */
/*                  sp2 -                                                                                  */
/*                                                                                                         */
/* Returns:         none                                                                                   */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine selects UART muxing                                                       */
/*---------------------------------------------------------------------------------------------------------*/
int CHIP_Mux_Uart (UART_MUX_T redirection_mode, bool CoreSP, bool sp1, bool sp2)
{
	struct npcm850_gcr *gcr = (struct npcm850_gcr *)(uintptr_t)npcm850_get_base_gcr();

	/* 111 combination is reserved: */
	if (redirection_mode < 7)
	{
		writel(((readl(&gcr->spswc) & ~(0x7)) | redirection_mode), &gcr->spswc);
	}
	else if (CoreSP == false)
	{
		// if both redirection_mode >= 7 and CoreSP (which does not need a mode)
		// was not selected it is error
		return -1;
	}

	if (sp1)
	{
		writel((readl(&gcr->mfsel1) | (1 << 10)), &gcr->mfsel1);
		writel((readl(&gcr->mfsel4) & ~(1 << 1)), &gcr->mfsel4); /* Select TXD1+RXD1 */
	}
	if (sp2)
	{
		writel((readl(&gcr->mfsel1) | (1 << 10)), &gcr->mfsel1);
		writel((readl(&gcr->mfsel4) & ~(1 << 1)), &gcr->mfsel4); /* Select TXD2+RXD2 */
	}
	if (CoreSP)
	{
#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
		if ((readl((volatile uint32_t *)(0xf0800efc)) & BIT(3)) == BIT(3))   /* Check SRCPAD 63 bit3=1 -> pllladium 57 else pllladium 38 (uart0 mux select)  */
#else
		if ((readl(&gcr->pwron) & (1 << PWRON_BSPA)) == 0)                   /* Check STRAP5 for uart0 mux select */
#endif
		{
			writel((readl(&gcr->mfsel4) | (1 << MFSEL4_BSPASEL)), &gcr->mfsel4);    /* SET  MFSEL4_BSPASEL 1 */
			/* Note: If this bit is set, MFSEL1 bit 9 and 11 must be set to 0. */
			writel((readl(&gcr->mfsel1) & ~(1 << MFSEL1_BSPSEL)), &gcr->mfsel1);    /* SET  MFSEL4_BSPSEL 0 */
			writel((readl(&gcr->mfsel1) & ~(1 << MFSEL1_HSI2ASEL)), &gcr->mfsel1);  /* SET  MFSEL1_HSI2ASEL 0 */
		}
		else
		{
			writel((readl(&gcr->mfsel4) & ~(1 << MFSEL4_BSPASEL)), &gcr->mfsel4);   /* SET  MFSEL4_BSPASEL 0 */
			writel((readl(&gcr->mfsel1) | (1 << MFSEL1_BSPSEL)), &gcr->mfsel1);     /* SET  MFSEL4_BSPSEL 1 */
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:        CLK_ResetUART                                                                          */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*                  deviceNum -                                                                            */
/*                                                                                                         */
/* Returns:         none                                                                                   */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine performs SW reset of UART                                                 */
/*---------------------------------------------------------------------------------------------------------*/
void CLK_ResetUART(void)
{
    struct clk_ctl *clkctl = (struct clk_ctl *)(uintptr_t)npcm850_get_base_clk();

	writel((readl(&clkctl->ipsrst1) | (1 << 11)), &clkctl->ipsrst1);
	writel((readl(&clkctl->ipsrst1) & ~(1 << 11)), &clkctl->ipsrst1);

	writel((readl(&clkctl->ipsrst1) | (1 << 7)), &clkctl->ipsrst1);
	writel((readl(&clkctl->ipsrst1) & ~(1 << 7)), &clkctl->ipsrst1);
}

int UART_Init (UART_DEV_T devNum, UART_MUX_T muxMode, UART_BAUDRATE_T baudRate)
{
	struct npcmX50_uart *uart = (struct npcmX50_uart *)(uintptr_t)npcm850_get_base_uart();

	u32 FCR_Val      = 0;

    bool CoreSP  = false;
    bool sp1     = false;
    bool sp2     = false;
    u32  ret     = 0;


    /*-----------------------------------------------------------------------------------------------------*/
    /* Muxing for UART0                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    if (devNum == UART0_DEV)
    {
        CoreSP = true;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Enable serial interfaces according to mux mode                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
    switch (muxMode)
    {
        case UART_MUX_MODE4_HSP1_SI1____HSP2_SI2____UART1_s_SI1___UART3_s_SI2__UART2_s_HSP1:
        case UART_MUX_MODE6_HSP1_SI1____HSP2_SI2____UART1_s_SI1___UART3_s_SI2__UART2_s_HSP2:
        case UART_MUX_MODE7_HSP1_SI1____HSP2_UART2__UART1_s_HSP1__UART3_SI2:
            {
                sp1 = true;
                sp2 = true;
                break;
            }
        case UART_MUX_MODE5_HSP1_SI1____HSP2_UART2__UART1_s_HSP1__UART3_s_SI1:
            {
                sp1 = true;
                break;
            }
        case UART_MUX_MODE1_HSP1_SI2____HSP2_UART2__UART1_s_HSP1__UART3_s_SI2:
        case UART_MUX_MODE2_HSP1_UART1__HSP2_SI2____UART2_s_HSP2__UART3_s_SI2:
        case UART_MUX_MODE3_HSP1_UART1__HSP2_UART2__UART3_SI2:
            {
                sp2= true;
                break;
            }

        case UART_MUX_SKIP_CONFIG:
            {
                /* Do nothing. Don't call CHIP_Mux_UART. Assuming it was called before */
                break;
            }

        /*-------------------------------------------------------------------------------------------------*/
        /* Illegal mux mode                                                                                */
        /*-------------------------------------------------------------------------------------------------*/
        default:
		  return -1;
    }

    CHIP_Mux_Uart(muxMode, CoreSP, sp1, sp2);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Disable interrupts                                                                                  */
    /*-----------------------------------------------------------------------------------------------------*/
	writel(0x0, &uart->lcr);
	writel(0x0, &uart->ier);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set baudrate                                                                                        */
    /*-----------------------------------------------------------------------------------------------------*/
    ret += UART_SetBaudrate(baudRate);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set port for 8 bit, 1 stop, no parity                                                               */
    /*-----------------------------------------------------------------------------------------------------*/
    ret += UART_SetBitsPerChar(8);
    ret += UART_SetStopBit(UART_STOPBIT_1);
    ret += UART_SetParity(UART_PARITY_NONE);

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set the RX FIFO trigger level, reset RX, TX FIFO                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    FCR_Val = (FCR_RFITL_4B << 4) | 0x0F ;

    writel(FCR_Val, &uart->fcr);
    writel(0x0, &uart->tor);

    if (ret > 0)
        return -1;
    else
        return 0;
}

#define SR_MII_CTRL_ANEN_BIT12  12
#define SR_MII_CTRL_SWR_BIT15   15
#define VR_MII_MMD_DIG_CTRL1_R2TLBE_BIT14 14
#define VR_MII_MDD_DIG_CTRL2_RX_POL_INV_0_BIT0 0

int board_uart_init(void)
{
#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
	CLK_ResetUART();
	sdelay(210000UL);	  /* 	udelay (100) */
	UART_Init(UART0_DEV, UART_MUX_MODE1_HSP1_SI2____HSP2_UART2__UART1_s_HSP1__UART3_s_SI2, UART_BAUDRATE_115200);
	sdelay(210000UL);	  /* 	udelay (100) */
#endif
	return 0;
}

/*
 * Routine: get_board_version_id
 * Description: Detect board version by reading  GPIO79 (VER_ID1), GPIO78 (VER_ID0).
 *		GPIO79 (VER_ID1), GPIO78 (VER_ID0): 1 1 => X00
 *		GPIO79 (VER_ID1), GPIO78 (VER_ID0): 1 0 => X01
 *		GPIO79 (VER_ID1), GPIO78 (VER_ID0): 0 1 => Reserved
 *		GPIO79 (VER_ID1), GPIO78 (VER_ID0): 0 0 => Reserved
 */
int get_board_version_id(void)
{
	static int pcb_version = -1;

	if (pcb_version == -1)
	{
		if (!gpio_request(PCB_VER_ID0, "rev0") &&
		    !gpio_request(PCB_VER_ID1, "rev1"))
	    {
			gpio_direction_input(PCB_VER_ID0);
			gpio_direction_input(PCB_VER_ID1);

			pcb_version = gpio_get_value(PCB_VER_ID1) << 1 | gpio_get_value(PCB_VER_ID0);

			switch(pcb_version)
			{
				case 3:
					printf("NPCM850 EVB PCB version ID 0x%01x -> version X00 \n", pcb_version);
				break;
				case 2:
					printf("NPCM850 EVB PCB version ID 0x%01x -> version X01 \n", pcb_version);
				break;

				default:
					printf("NPCM850 EVB PCB version ID 0x%01x -> unknown version ID \n", pcb_version);
				break;
			}
			gpio_free(PCB_VER_ID0);
			gpio_free(PCB_VER_ID1);
		} else {
			printf("Error: unable to acquire board version GPIOs\n");
		}
	}
	return pcb_version;
}

void ESPI_ConfigAutoHandshake (u32 AutoHsCfg)
{
	writel( AutoHsCfg,
	        NPCM850_ESPI_BA + ESPIHINDP);
}

void ESPI_Config (
        ESPI_IO_MODE  ioMode,
        ESPI_MAX_FREQ maxFreq,
        u32           ch_supp
)
{
	u32 var = readl(NPCM850_ESPI_BA + ESPICFG);
	var |= ioMode << ESPICFG_IOMODE;
	var |= maxFreq << ESPICFG_MAXFREQ;
	var |= ((ch_supp & ESPICFG_CHNSUPP_MASK) << ESPICFG_CHNSUPP_SHFT);
	writel( var,
	        NPCM850_ESPI_BA + ESPICFG);
}

int board_init(void)
{
	u32 espi_ch_supp;

#ifdef CONFIG_ETH_DESIGNWARE
	unsigned int start;

	struct clk_ctl *clkctl = (struct clk_ctl *)(uintptr_t)npcm850_get_base_clk();
	struct npcm850_gcr *gcr = (struct npcm850_gcr *)(uintptr_t)npcm850_get_base_gcr();

    /* Power voltage select setup  TBD  move to dts */
	writel( 0x40004800, &gcr->vsrcr);

    /* Clock setups */
	writel((readl(&clkctl->clkdiv2) & ~(0x1f << CLKDIV2_CLKOUTDIV)) | (31 << CLKDIV2_CLKOUTDIV), &clkctl->clkdiv2);
	sdelay(420000UL); 	/* udelay(200) */
	writel((readl(&clkctl->clksel) & ~(0x7 << CLKSEL_CLKOUTSEL)) | (CLKSEL_CLKOUTSEL_PLL0 << CLKSEL_CLKOUTSEL), &clkctl->clksel);  // Select PLL0 for CLKOUTSEL

	writel(readl(&gcr->flockr2) | (0x1 << FLOCKR2_G35DA2P), &gcr->flockr2);


//#define GMAC1_SGMII_PCS_LB


    /* GMAC INTERNAL CLK  setup */
#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
//	writel(((readl(&clkctl->clksel) & ~(0x3 << CLKSEL_RGSEL)) | (CLKSEL_CPUCKSEL_PLL0 << CLKSEL_RGSEL)), &clkctl->clksel);  // Select PLL0 for GMAC 500MHz /4 = 125MHz
	writel(((readl(&clkctl->clksel) & ~(0x3 << CLKSEL_RGSEL)) | (CLKSEL_CPUCKSEL_PLL1 << CLKSEL_RGSEL)), &clkctl->clksel);  // Select PLL1 for GMAC 500MHz /4 = 125MHz
	writel((readl(&clkctl->clkdiv4) & ~(0xF << CLKDIV4_RGREFDIV) | (3 << CLKDIV4_RGREFDIV)), &clkctl->clkdiv4);           // Select Divider 3+1=4 for GMAC 500MHz /4 = 125MHz
#endif


#if 0  /* TBD - Remove when BB take-over setups */
	writel(((readl(&clkctl->clksel) & ~(0x3 << CLKSEL_RGSEL)) | (CLKSEL_CPUCKSEL_PLL0 << CLKSEL_RGSEL)), &clkctl->clksel);  // Select PLL0 for GMAC 500MHz /4 = 125MHz
//	writel(((readl(&clkctl->clksel) & ~(0x3 << CLKSEL_RGSEL)) | (CLKSEL_CPUCKSEL_PLL1 << CLKSEL_RGSEL)), &clkctl->clksel);  // Select PLL1 for GMAC 500MHz /4 = 125MHz
	writel((readl(&clkctl->clkdiv4) & ~(0xF << CLKDIV4_RGREFDIV) | (3 << CLKDIV4_RGREFDIV)), &clkctl->clkdiv4);             // Select Divider 3+1=4 for GMAC 500MHz/4 = 125MHz
//	writel((readl(&clkctl->clkdiv3) & ~(0x1F << CLKDIV3_SPI0CKDV) | (1 << CLKDIV3_SPI0CKDV)), &clkctl->clkdiv3);            // Select Divider 1+1=2 for SPI0 500MHz/4/2 = 62.5MHz
#endif

	/* Enable SGMII/RGMII for GMAC1/2 module */
	writel((readl(&gcr->mfsel4) | (1 << MFSEL4_SG1MSEL)), &gcr->mfsel4);     // GMAC1 - MDIO SGMII

#ifdef GMAC2_RGMII
#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
	writel(readl((volatile uint32_t *)(0xf0800efc)) & ~(1 << 2), (volatile uint32_t *)(0xf0800efc));	// Clear SCRCHPAD63 Bit2
	printf("board_init: GMAC2 = RGMII2 on Port2 SCRCHPAD63=0x%x \n", *(volatile uint32_t *)(0xf0800efc));
#endif
	writel((readl(&gcr->mfsel4) | (1 << MFSEL4_RG2SEL)), &gcr->mfsel4);      // GMAC2 - RGMII
	writel((readl(&gcr->mfsel4) | (1 << MFSEL4_RG2MSEL)), &gcr->mfsel4);     // GMAC2 - MDIO

#else  /* GMAC2 RMII3 */
#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
	writel(readl((volatile uint32_t *)(0xf0800efc)) | (1 << 2), (volatile uint32_t *)(0xf0800efc));		//  Set SCRCHPAD63 Bit2
	printf("board_init: GMAC2 = RMII3 on Port4  SCRCHPAD63=0x%x \n", *(volatile uint32_t *)(0xf0800efc));
#endif
	writel((readl(&gcr->mfsel4) & ~(1 << MFSEL4_RG2SEL)), &gcr->mfsel4);     // Switch GMAC2 to RMII3 Mode
	writel((readl(&gcr->mfsel3) & ~(1 << MFSEL3_DDRDVOSEL)), &gcr->mfsel3);  // Switch GMAC2 to RMII3 Mode
	writel((readl(&gcr->mfsel4) | (1 << MFSEL4_RG2MSEL)), &gcr->mfsel4);     // GMAC2 - MDIO
	writel((readl(&gcr->mfsel5) | (1 << MFSEL5_RMII3SEL)), &gcr->mfsel5);    // RMII3 select
	writel((readl(&gcr->intcr4) | (1 << INTCR4_R3EN)), &gcr->intcr4);        // RMII3 Set INTCR4_R3EN Enable
	writel((readl(&gcr->mfsel5) | (1 << MFSEL5_R3OENSEL)), &gcr->mfsel5);    // RMII3 Set MFSEL5_R3OENSEL Output-Enable
#endif /* #endif GMAC2_RGMII */

	writel((readl(&gcr->mfsel3) | (1 << MFSEL3_RMII1SEL)), &gcr->mfsel3);    // GMAC3 - RMII1 Select
	writel((readl(&gcr->intcr4) | (1 << INTCR4_R1EN)), &gcr->intcr4);        // GMAC3 - RMII1 Set INTCR4_R1EN
	writel((readl(&gcr->mfsel5) | (1 << MFSEL5_R1OENSEL)), &gcr->mfsel5);    // GMAC3 - RMII1 Set MFSEL5_R1OENSEL
	writel((readl(&gcr->mfsel1) & ~(1 << MFSEL1_R1ERRSEL)), &gcr->mfsel1);   // GMAC3 - RMII1 Clear MFSEL1_R1ERRSEL

	writel((readl(&gcr->mfsel1) | (1 << MFSEL1_RMII2SEL)), &gcr->mfsel1);    // GMAC4 - RMII2 Select
	writel((readl(&gcr->intcr4) | (1 << INTCR4_R2EN)), &gcr->intcr4);        // GMAC4 - RMII2 Set INTCR4_R2EN
	writel((readl(&gcr->mfsel5) | (1 << MFSEL5_R2OENSEL)), &gcr->mfsel5);    // GMAC4 - RMII2 Set MFSEL5_R2OENSEL
	writel((readl(&gcr->mfsel1) & ~(1 << MFSEL1_R2ERRSEL)), &gcr->mfsel1);   // GMAC4 - RMII2 Clear MFSEL1_R2ERRSEL

	/* IP Software Reset for GMAC1/2 module */
	writel(readl(&clkctl->ipsrst2) | (1 << IPSRST2_GMAC1), &clkctl->ipsrst2);
	writel(readl(&clkctl->ipsrst2) & ~(1 << IPSRST2_GMAC1), &clkctl->ipsrst2);
	writel(readl(&clkctl->ipsrst2) | (1 << IPSRST2_GMAC2), &clkctl->ipsrst2);
	writel(readl(&clkctl->ipsrst2) & ~(1 << IPSRST2_GMAC2), &clkctl->ipsrst2);

	/* IP Software Reset for GMAC3/4 module */
	writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_GMAC3), &clkctl->ipsrst1);
	writel(readl(&clkctl->ipsrst1) & ~(1 << IPSRST1_GMAC3), &clkctl->ipsrst1);
	writel(readl(&clkctl->ipsrst1) | (1 << IPSRST1_GMAC4), &clkctl->ipsrst1);
	writel(readl(&clkctl->ipsrst1) & ~(1 << IPSRST1_GMAC4), &clkctl->ipsrst1);


        /* SGMII PHY reset */
	writew(0x1F00, 0xF07801FE);           /* Get access to 0x3E... (SR_MII_CTRL) */
	writew(readw(0xF0780000) | (1 << SR_MII_CTRL_SWR_BIT15), 0xF0780000);
	start = get_timer(0);

	printf("SGMII PCS PHY reset wait \n");
	while (readw(0xF0780000) & (1 << SR_MII_CTRL_SWR_BIT15))
	{
		if (get_timer(start) >= 3*CONFIG_SYS_HZ)
		{
			printf("SGMII PHY reset timeout\n");
			return -ETIMEDOUT;
		}

		mdelay(1);
	};

	/* Clear SGMII PHY default auto neg. */
	writew(readw(0xF0780000) & ~(1 << SR_MII_CTRL_ANEN_BIT12), 0xF0780000);
	printf("SGMII PCS PHY reset done and clear Auto Negotiation \n");

#ifdef CONFIG_TARGET_ARBEL_EVB
    if (get_board_version_id() == 3 )  /* EVB X00 version - need to swap sgmii lane polarity HW issue */
	{
		writew(0x1F80, 0xF07801FE);                           /* Get access to 0x3F... (VR_MII_MMD_DIG_CTRL1) */
		writew(readw(0xf07801c2) | (1 << VR_MII_MDD_DIG_CTRL2_RX_POL_INV_0_BIT0), 0xf07801c2);                      /* Swap lane polarity on EVB only */
		writel(readl((volatile uint32_t *)(0xf001305c)) | 0x3000, (volatile uint32_t *)(0xf001305c));           	/* Set SGMII MDC/MDIO pins to output slew-rate high */
		printf("EVB-X00 SGMII Work-Around: RX Polarity Invert Lane-0 and MDC/MDIO pins output slew-rate high\n");
	}

	/* Set reg SMC_CTL bit HOSTWAIT Write 1 to Clear */
	writeb(readb((volatile uint8_t *)(0xC0001001)) | 0x80, (volatile uint8_t *)(0xC0001001));
#endif

#ifdef GMAC1_SGMII_PCS_LB
	writew(0x1F80, 0xF07801FE);           /* Get access to 0x3F... (VR_MII_MMD_DIG_CTRL1) */
	writew(readw(0xF0780000) | (1 << VR_MII_MMD_DIG_CTRL1_R2TLBE_BIT14), 0xF0780000);
#endif

	gd->bd->bi_arch_number = CONFIG_MACH_TYPE;
	gd->bd->bi_boot_params = (PHYS_SDRAM_1 + 0x100UL);
#endif

	espi_ch_supp = fdtdec_get_config_int(gd->fdt_blob, "espi-channel-support", 0);

	if (espi_ch_supp > 0) {
	  u32 hindp = 0x00011110 | espi_ch_supp;
	  // Set ESPI_SEL bit in MFSEL4 register.
	  writel((readl(&gcr->mfsel4) | (1 << MFSEL4_ESPISEL)), &gcr->mfsel4);

	  // In eSPI HOST INDEPENDENCE register, set bits
	  // AUTO_SBLD, AUTO_FCARDY, AUTO_OOBCRDY,
	  // AUTO_VWCRDY, AUTO_PCRDY, AUTO_HS1, AUTO_HS2, AUTO_HS3.
	  ESPI_ConfigAutoHandshake(hindp);

	  // In eSPI ESPICFG register set ESPICFG.MAXREQ to 33 MHz and ESPICFG. IOMODE
	  // to Quad.
	  ESPI_Config(ESPI_IO_MODE_SINGLE_DUAL_QUAD, ESPI_MAX_33_MHz, espi_ch_supp);
	}
	else {
	  // set LPCSEL bit in MFSEL1 register.
	  writel((readl(&gcr->mfsel1) | (1 << MFSEL1_LPCSEL)), &gcr->mfsel1);
	}

	return 0;
}

int dram_init(void)
{
	struct npcm850_gcr *gcr = (struct npcm850_gcr *)(uintptr_t)npcm850_get_base_gcr();
#ifdef CONFIG_TARGET_ARBEL_PALLADIUM
    struct clk_ctl *clkctl = (struct clk_ctl *)(uintptr_t)npcm850_get_base_clk();
#endif

// For bootblock 0.0.5 and below:
#if 0
	int RAMsize = (readl(&gcr->intcr4) >> 20) & 0x7;   /* Read only 3bit's MSB of GMMAP0 */

	switch(RAMsize)
	{
		case 1:
				gd->ram_size = 0x20000000; /* 512 MB. */
				break;
		case 3:
				gd->ram_size = 0x40000000; /* 1 GB. */
				break;
		case 6:
		case 7:
				gd->ram_size = 0x80000000; /* 2 GB. */
				break;

		default:
			printf("GMMAP is not set correctly intcr4=0x%08x\n", readl(&gcr->intcr4));
			break;
	}
#endif


	// get dram active size value from bootblock. Value sent using scrpad_02 register.
	// feature available in bootblock 0.0.6 and above.
	gd->ram_size = readl(&gcr->scrpad_b);

#ifdef CONFIG_TARGET_ARBEL_PALLADIUM

	gd->ram_size = 0x10000000; /* 256 MB.  set for ecc test */


	#if defined  (CONFIG_NPCMX50_CORE0) || !defined (CONFIG_NPCMX50_CORE0) && !defined (CONFIG_NPCMX50_CORE1) && !defined (CONFIG_NPCMX50_CORE2) && !defined (CONFIG_NPCMX50_CORE3)

	//	writel((readl(&gcr->intcr4) | (0x0B << INTCR4_GMMAP1)  | (0x0B << INTCR4_GMMAP0) ), &gcr->intcr4);   /* Set BMC ECC GFX1 PCI GFX0 to (16MB)  top of 256MB DDR on Palladium */
	//	writel((readl(&gcr->intcr4) | (0x0A << INTCR4_GMMAP1)  | (0x0B << INTCR4_GMMAP0) ), &gcr->intcr4);   /* Set BMC ECC refresh GFX1 & PCI GFX0 to (16MB)  top of 256MB DDR on Palladium */

		writel((readl(&gcr->intcr4) | (0x0F << INTCR4_GMMAP1)  | (0x0F << INTCR4_GMMAP0) ), &gcr->intcr4);   /* Set BMC GFX1 PCI GFX0 to (16MB + ECC)  top of 256MB DDR on Palladium */
		writel((readl(&gcr->intcr)  & ~(0x01 << INTCR_DAC_SNS) ), &gcr->intcr);                              /* Clear INTCR_DAC_SNS for GFX Diag test on Palladium */
		writel(0xFFFFFFDF, &clkctl->wd0rcr);                                                                 /* Disable Memory Controler MC reset upon WDOG reset - done for GFX test */
	#endif

	#ifdef CONFIG_NPCMX50_CORE0
		gd->ram_size = 0x9F00000; /* 159 MB.  keep space for GFX+ECC */
	#endif

	#ifdef CONFIG_NPCMX50_CORE1
		gd->ram_size = 0x7800000; /* 120 MB. */
	#endif

	#ifdef CONFIG_NPCMX50_CORE2
		gd->ram_size = 0x5000000; /* 80 MB. */
	#endif

	#ifdef CONFIG_NPCMX50_CORE3
		gd->ram_size = 0x2800000; /* 40 MB. */
	#endif

#else  /* else CONFIG_TARGET_ARBEL_PALLADIUM */

	#ifdef CONFIG_NPCMX50_CORE0
		gd->ram_size = 0x40000000; /* 1024 MB.  keep space for GFX+ECC */
	#endif

	#ifdef CONFIG_NPCMX50_CORE1
		gd->ram_size = 0x20000000; /* 512 MB. */
	#endif

	#ifdef CONFIG_NPCMX50_CORE2
		gd->ram_size = 0x10000000; /* 256 MB. */
	#endif

	#ifdef CONFIG_NPCMX50_CORE3
		gd->ram_size = 0x8000000; /* 128 MB. */
	#endif

#endif /* Endif CONFIG_TARGET_ARBEL_PALLADIUM */

	return 0;
}

#ifdef CONFIG_BOARD_EARLY_INIT_F
int board_early_init_f(void)
{
	int err;

	err = board_uart_init();
	if (err) {
		debug("UART init failed\n");
		return err;
	}

	return 0;
}
#endif


int board_eth_init(bd_t *bis)
{
	return 0;
}


#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	const char *board_info;

	board_info = fdt_getprop(gd->fdt_blob, 0, "model", NULL);
	printf("Board: %s\n", board_info ? board_info : "unknown");
#ifdef CONFIG_BOARD_TYPES
	board_info = get_board_type();
	if (board_info)
		printf("Type:  %s\n", board_info);
#endif
	return 0;
}
#endif

#ifdef CONFIG_LAST_STAGE_INIT
#ifdef SECURE_BOOT
static bool is_security_enabled(void)
{
	struct npcm850_gcr *gcr = (struct npcm850_gcr *)(uintptr_t)npcm850_get_base_gcr();

	if ((readl(&gcr->pwron) & (1 << PWRON_SECEN))) {
		printf("Security is enabled\n");
		return true;
	} else {
		printf("Security is NOT enabled\n");
		return false;
	}
}

static int check_nist_version(void)
{
#if (CONFIG_NIST_VERSION_ADDR != 0)
	volatile u32 uboot_ver = *(u32*)(uintptr_t)(UBOOT_RAM_IMAGE_ADDR + HEADER_VERSION_OFFSET);
	volatile u32 nist_ver = *(u32*)(uintptr_t)(CONFIG_NIST_VERSION_ADDR);

	if (uboot_ver != nist_ver)
		reset_cpu(0);
#endif

	return 0;
}

static int secure_boot_configuration(void)
{
#if defined(CONFIG_SPI_FLASH) && defined(SPI_FLASH_BASE_ADDR)

	const u8 tag[SA_TAG_FLASH_IMAGE_SIZE] = SA_TAG_FLASH_IMAGE_VAL;
	struct spi_flash *flash;
	struct udevice *udev;
	u32 addr, addr_align;
	int rc , i, offset;
	u8 *buf = NULL;

	rc = spi_flash_probe_bus_cs(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
			CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE, &udev);
	if (rc)
		return rc;

	flash = dev_get_uclass_priv(udev);
	if (!flash)
		return -1;

	// fuse images should be a part of the flash image, right after the uboot
	addr = POLEG_UBOOT_END;

	// if found, program the image to the fuse arrays, set the secure boot
	// bit and erase the image from the flash
	if (((u32*)(uintptr_t)(addr + SA_TAG_FLASH_IMAGE_OFFSET))[0] == ((u32*)tag)[0] &&
		((u32*)(uintptr_t)(addr + SA_TAG_FLASH_IMAGE_OFFSET))[1] == ((u32*)tag)[1]) {

		u8 fuse_arrays[2 * NPCMX50_OTP_ARR_BYTE_SIZE];
		u32 fustrap_orig;

		printf("%s(): fuse array image was found on flash in address 0x%x\n", __func__, addr);

		memcpy(fuse_arrays, (u8*)(uintptr_t)addr, sizeof(fuse_arrays));

		fustrap_orig = *(u32*)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET);

		//TODO: Here, randomize 4 AES keys + generate their nibble parity + embed to image

		printf("%s(): program fuse key array from address 0x%x\n", __func__, addr + SA_KEYS_FLASH_IMAGE_OFFSET);

		rc = fuse_prog_image(NPCMX50_KEY_SA, (u32)(uintptr_t)(fuse_arrays + SA_KEYS_FLASH_IMAGE_OFFSET));
		if (rc != 0)
			return rc;

		// clear oSecBoot, will be programmed only after everything is
		// programmed successfuly
		fustrap_orig = *(u32*)(uintptr_t)(addr + SA_FUSE_FLASH_IMAGE_OFFSET);
		*(u32*)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET) &= ~FUSTRAP_O_SECBOOT;

		printf("%s(): program fuse strap array from address 0x%x\n", __func__, addr + SA_FUSE_FLASH_IMAGE_OFFSET);

		rc = fuse_prog_image(NPCMX50_FUSE_SA, (u32)(uintptr_t)(fuse_arrays + SA_FUSE_FLASH_IMAGE_OFFSET));
		if (rc != 0)
			return rc;

		// erase the whole sector
		addr_align = addr & ~(u32)(uintptr_t)(flash->erase_size -1);

		offset = addr - addr_align;
		printf("%s(): erase the sector of addr 0x%x\n", __func__, addr_align);

		addr_align -= SPI_FLASH_BASE_ADDR;

		buf = (u8 *)(uintptr_t)malloc(flash->erase_size);
		if (buf) {
			spi_flash_read(flash, addr_align, flash->erase_size, buf);
		} else {
			printf("%s(): failed to alloc buffer, skip otp program\n", __func__);
			return -1;
		}

		for (i = 0 ; i < (SA_TAG_FLASH_IMAGE_OFFSET + SA_TAG_FLASH_IMAGE_SIZE) ; i++)
			buf[offset + i] = 0xff;

		rc = spi_flash_erase(flash, addr_align, flash->erase_size);
		if (rc != 0)
			return rc;

		rc = spi_flash_write(flash, addr_align, flash->erase_size, buf);
		if (rc != 0)
			return rc;

		free(buf);

		// programm SECBOOT bit if required
		if (fustrap_orig & FUSTRAP_O_SECBOOT) {
			printf("%s(): program secure boot bit to FUSTRAP\n", __func__);
			rc = fuse_program_data(NPCMX50_FUSE_SA, 0, (u8*)(uintptr_t)&fustrap_orig, sizeof(fustrap_orig));
		} else {
			printf("%s(): secure boot bit is not set in the flash image, secure boot will not be enabled\n", __func__);
		}

		return rc;
	}
	// No fuse image was found in flash, continue with the normal boot flow

#endif

	return 0;
}
#endif

int last_stage_init(void)
{
	char value[32];
	struct udevice *dev = gd->cur_serial_dev;

	if (gd->ram_size > 0) {
		sprintf(value, "%lldM", (gd->ram_size / 0x100000));
		env_set("mem", value);
	}

	if (dev && (dev->seq >= 0)) {
		void *addr;
		addr = dev_read_addr_ptr(dev);

		if (addr) {
			sprintf(value, "uart8250,mmio32,0x%x", (u32)(uintptr_t)addr);
			env_set("earlycon", value);
		}
		sprintf(value, "ttyS%d,115200n8", dev->seq);
		env_set("console", value);

	}
#ifdef SECURE_BOOT
	if (is_security_enabled()) {
		int rc = check_nist_version();
		if (rc != 0)
			return rc;
	} else {
		// OTP can be programmed only in basic mode
		int rc = secure_boot_configuration();
		if (rc != 0)
			return rc;
	}
#endif
	return 0;
}
#endif
