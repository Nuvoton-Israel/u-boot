#ifndef __ARBEL_ESPI_H_
#define __ARBEL_ESPI_H_

#define NPCM850_ESPI_BA			  0xF009F000
#define ESPICFG					      0x04
#define ESPIHINDP             0x80
#define ESPICFG_CHNSUPP_MASK	0x0F
#define ESPICFG_CHNSUPP_SHFT	24

#define ESPICFG_IOMODE        8
#define ESPICFG_MAXFREQ       10
#define ESPICFG_RSTO          31

#define NPCM850_SHM_BA        0XC0001000
#define SMC_CTL               0x01


#define HOSTWAIT              7


/*---------------------------------------------------------------------------------------------------------*/
/* I/O Mode Supported                                                                                      */
/*---------------------------------------------------------------------------------------------------------*/
typedef enum
{
    ESPI_IO_MODE_SINGLE         = 0,    /* (default) do not change - code relies on enum value */
    ESPI_IO_MODE_SINGLE_DUAL,
    ESPI_IO_MODE_SINGLE_QUAD,
    ESPI_IO_MODE_SINGLE_DUAL_QUAD,
} ESPI_IO_MODE;

/*---------------------------------------------------------------------------------------------------------*/
/* Maximum Frequency Supported                                                                             */
/*---------------------------------------------------------------------------------------------------------*/
typedef enum
{
    ESPI_MAX_20_MHz             = 0,    /* (default) do not change - code relies on enum value */
    ESPI_MAX_25_MHz,
    ESPI_MAX_33_MHz,
    ESPI_MAX_50_MHz,
    ESPI_MAX_66_MHz,
} ESPI_MAX_FREQ;

/*---------------------------------------------------------------------------------------------------------*/
/* Operating Frequency                                                                                     */
/*---------------------------------------------------------------------------------------------------------*/
typedef enum
{
    ESPI_OP_20_MHz              = 0,    /* (default) do not change - code relies on enum value */
    ESPI_OP_25_MHz,
    ESPI_OP_33_MHz,
    ESPI_OP_50_MHz,
    ESPI_OP_66_MHz,
} ESPI_OP_FREQ;

/*---------------------------------------------------------------------------------------------------------*/
/* Reset Output                                                                                            */
/*---------------------------------------------------------------------------------------------------------*/
typedef enum
{
    ESPI_RST_OUT_LOW            = 0,    /* (default) do not change - code relies on enum value */
    ESPI_RST_OUT_HIGH,
} ESPI_RST_OUT;



#endif
