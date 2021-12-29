#include <net.h>

#define MAC_ADDR_SIZE       6
#define CONFIG_TX_DESCR_NUM	32
#define CONFIG_RX_DESCR_NUM	32

#define TX_TOTAL_BUFSIZE	((CONFIG_TX_DESCR_NUM + 1) * PKTSIZE_ALIGN + PKTALIGN)
#define RX_TOTAL_BUFSIZE	((CONFIG_RX_DESCR_NUM + 1) * PKTSIZE_ALIGN + PKTALIGN)

#define CONFIG_MDIO_TIMEOUT (3 * CONFIG_SYS_HZ)

struct npcm750_rxbd {
	unsigned int sl;
	unsigned int buffer;
	unsigned int reserved;
	unsigned int next;
} __aligned(ARCH_DMA_MINALIGN);

struct npcm750_txbd {
	unsigned int mode;
	unsigned int buffer;
	unsigned int sl;
	unsigned int next;
} __aligned(ARCH_DMA_MINALIGN);

struct emc_regs {
	u32 camcmr;		/* 0x00 */
	u32 camen;		/* 0x04 */
	u32 cam0m;		/* 0x08 */
	u32 cam0l;		/* 0x0c */
	u32 cam1m;		/* 0x10 */
	u32 cam1l;		/* 0x14 */
	u32 cam2m;		/* 0x18 */
	u32 cam2l;		/* 0x1c */
	u32 cam3m;		/* 0x20 */
	u32 cam3l;		/* 0x24 */
	u32 cam4m;		/* 0x28 */
	u32 cam4l;		/* 0x2c */
	u32 cam5m;		/* 0x30 */
	u32 cam5l;		/* 0x34 */
	u32 cam6m;		/* 0x38 */
	u32 cam6l;		/* 0x3c */
	u32 cam7m;		/* 0x40 */
	u32 cam7l;		/* 0x44 */
	u32 cam8m;		/* 0x48 */
	u32 cam8l;		/* 0x4c */
	u32 cam9m;		/* 0x50 */
	u32 cam9l;		/* 0x54 */
	u32 cam10m;		/* 0x58 */
	u32 cam10l;		/* 0x5c */
	u32 cam11m;		/* 0x60 */
	u32 cam11l;		/* 0x64 */
	u32 cam12m;		/* 0x68 */
	u32 cam12l;		/* 0x6c */
	u32 cam13m;		/* 0x70 */
	u32 cam13l;		/* 0x74 */
	u32 cam14m;		/* 0x78 */
	u32 cam14l;		/* 0x7c */
	u32 cam15m;		/* 0x80 */
	u32 cam15l;		/* 0x84 */
	u32 txdlsa;		/* 0x88 */
	u32 rxdlsa;		/* 0x8c */
	u32 mcmdr;		/* 0x90 */
	u32 miid;		/* 0x94 */
	u32 miida;		/* 0x98 */
	u32 fftcr;		/* 0x9c */
	u32 tsdr;		/* 0xa0 */
	u32 rsdr;		/* 0xa4 */
	u32 dmarfc;		/* 0xa8 */
	u32 mien;		/* 0xac */
	u32 mista;		/* 0xb0 */
	u32 mgsta;		/* 0xb4 */
	u32 mpcnt;		/* 0xb8 */
	u32 mrpc;		/* 0xbc */
	u32 mrpcc;		/* 0xc0 */
	u32 mrepc;		/* 0xc4 */
	u32 dmarfs;		/* 0xc8 */
	u32 ctxdsa;		/* 0xcc */
	u32 ctxbsa;		/* 0xd0 */
	u32 crxdsa;		/* 0xd4 */
	u32 crxbsa;		/* 0xd8 */
};


struct npcm750_eth_dev {
	struct npcm750_txbd tdesc[CONFIG_TX_DESCR_NUM] __aligned(ARCH_DMA_MINALIGN);
	struct npcm750_rxbd rdesc[CONFIG_RX_DESCR_NUM] __aligned(ARCH_DMA_MINALIGN);
	u8 txbuffs[TX_TOTAL_BUFSIZE] __aligned(ARCH_DMA_MINALIGN);
	u8 rxbuffs[RX_TOTAL_BUFSIZE] __aligned(ARCH_DMA_MINALIGN);
	struct emc_regs *emc_regs_p;
	struct phy_device *phydev;
	struct mii_dev *bus;
	struct npcm750_txbd *curr_txd;
	struct npcm750_rxbd *curr_rxd;
	u32 interface;
	u32 max_speed;
	u32 idx;
};

struct npcm750_eth_pdata {
	struct eth_pdata eth_pdata;
};

/* mac controller bit */
#define MCMDR_RXON		(0x01 <<  0)
#define MCMDR_ACP		(0x01 <<  3)
#define MCMDR_SPCRC		(0x01 <<  5)
#define MCMDR_TXON		(0x01 <<  8)
#define MCMDR_NDEF		(0x01 <<  9)
#define MCMDR_FDUP		(0x01 << 18)
#define MCMDR_ENMDC		(0x01 << 19)
#define MCMDR_OPMOD		(0x01 << 20)
#define MCMDR_SWR		(0x01 << 24)

/* cam command regiser */
#define CAMCMR_AUP		0x01
#define CAMCMR_AMP		(0x01 << 1)
#define CAMCMR_ABP		(0x01 << 2)
#define CAMCMR_CCAM		(0x01 << 3)
#define CAMCMR_ECMP		(0x01 << 4)
#define CAM0EN			0x01

/* mac mii controller bit */
#define MDCON			(0x01 << 19)
#define PHYAD			(0x01 <<  8)
#define PHYWR			(0x01 << 16)
#define PHYBUSY			(0x01 << 17)
#define PHYPRESP		(0x01 << 18)
#define CAM_ENTRY_SIZE	0x08

/* rx and tx status */
#define TXDS_TXCP		(0x01 << 19)
#define RXDS_CRCE		(0x01 << 17)
#define RXDS_PTLE		(0x01 << 19)
#define RXDS_RXGD		(0x01 << 20)
#define RXDS_ALIE		(0x01 << 21)
#define RXDS_RP			(0x01 << 22)

/* mac interrupt status*/
#define MISTA_RXINTR	(0x01 <<  0)
#define MISTA_CRCE  	(0x01 <<  1)
#define MISTA_RXOV		(0x01 <<  2)
#define MISTA_PTLE		(0x01 <<  3)
#define MISTA_RXGD		(0x01 <<  4)
#define MISTA_ALIE		(0x01 <<  5)
#define MISTA_RP		(0x01 <<  6)
#define MISTA_MMP		(0x01 <<  7)
#define MISTA_DFOI		(0x01 <<  8)
#define MISTA_DENI		(0x01 <<  9)
#define MISTA_RDU		(0x01 << 10)
#define MISTA_RXBERR	(0x01 << 11)
#define MISTA_CFR 		(0x01 << 14)
#define MISTA_TXINTR	(0x01 << 16)
#define MISTA_TXEMP 	(0x01 << 17)
#define MISTA_TXCP		(0x01 << 18)
#define MISTA_EXDEF		(0x01 << 19)
#define MISTA_NCS		(0x01 << 20)
#define MISTA_TXABT		(0x01 << 21)
#define MISTA_LC		(0x01 << 22)
#define MISTA_TDU   	(0x01 << 23)
#define MISTA_TXBERR	(0x01 << 24)

#define ENSTART			0x01
#define ENRXINTR		(0x01 <<  0)
#define ENCRCE  		(0x01 <<  1)
#define EMRXOV  		(0x01 <<  2)
#define ENPTLE  		(0x01 <<  3)
#define ENRXGD			(0x01 <<  4)
#define ENALIE			(0x01 <<  5)
#define ENRP			(0x01 <<  6)
#define ENMMP			(0x01 <<  7)
#define ENDFO			(0x01 <<  8)
#define ENDENI			(0x01 <<  9)
#define ENRDU			(0x01 << 10)
#define ENRXBERR		(0x01 << 11)
#define ENCFR   		(0x01 << 14)
#define ENTXINTR		(0x01 << 16)
#define ENTXEMP 		(0x01 << 17)
#define ENTXCP			(0x01 << 18)
#define ENTXDEF			(0x01 << 19)
#define ENNCS			(0x01 << 20)
#define ENTXABT			(0x01 << 21)
#define ENLC			(0x01 << 22)
#define ENTDU   		(0x01 << 23)
#define ENTXBERR		(0x01 << 24)

#define RX_STAT_RBC     0xffff
#define RX_STAT_RXINTR  0x01 << 16
#define RX_STAT_CRCE    0x01 << 17
#define RX_STAT_PTLE    0x01 << 19
#define RX_STAT_RXGD    0x01 << 20
#define RX_STAT_ALIE    0x01 << 21
#define RX_STAT_RP      0x01 << 22
#define RX_STAT_OWNER   0x03 << 30

#define TX_STAT_TBC     0xffff
#define TX_STAT_TXINTR  0x01 << 16
#define TX_STAT_DEF     0x01 << 17
#define TX_STAT_TXCP    0x01 << 19
#define TX_STAT_EXDEF   0x01 << 20
#define TX_STAT_NCS     0x01 << 21
#define TX_STAT_TXBT    0x01 << 22
#define TX_STAT_LC      0x01 << 23
#define TX_STAT_TXHA    0x01 << 24
#define TX_STAT_PAU     0x01 << 25
#define TX_STAT_SQE     0x01 << 26

/* rx and tx owner bit */
#define RX_OWEN_DMA		(0x02 << 30)
#define RX_OWEN_CPU		(0x00 << 30)
#define TX_OWEN_DMA		(0x01 << 31)
#define TX_OWEN_CPU		(~(0x01 << 31))

/* tx frame desc controller bit */
#define MACTXINTEN		0x04
#define CRCMODE			0x02
#define PADDINGMODE		0x01

/* fftcr controller bit */
#define RXTHD 			(0x03 <<  0)
#define TXTHD 			(0x03 <<  8)
#define BLENGTH			(0x02 << 20)

/* global setting for driver */
#define RX_DESC_SIZE	128
#define TX_DESC_SIZE	64
#define MAX_RBUFF_SZ	0x600
#define MAX_TBUFF_SZ	0x600
#define TX_TIMEOUT	    50
#define DELAY		    1000
#define CAM0		    0x0
#define RX_POLL_SIZE    (RX_DESC_SIZE/2)
#define MII_TIMEOUT	    100

enum MIIDA_MDCCR_T
{
    MIIDA_MDCCR_4       = 0x00,
    MIIDA_MDCCR_6       = 0x01,
    MIIDA_MDCCR_8       = 0x02,
    MIIDA_MDCCR_12      = 0x03,
    MIIDA_MDCCR_16      = 0x04,
    MIIDA_MDCCR_20      = 0x05,
    MIIDA_MDCCR_24      = 0x06,
    MIIDA_MDCCR_28      = 0x07,
    MIIDA_MDCCR_30      = 0x08,
    MIIDA_MDCCR_32      = 0x09,
    MIIDA_MDCCR_36      = 0x0A,
    MIIDA_MDCCR_40      = 0x0B,
    MIIDA_MDCCR_44      = 0x0C,
    MIIDA_MDCCR_48      = 0x0D,
    MIIDA_MDCCR_54      = 0x0E,
    MIIDA_MDCCR_60      = 0x0F,
};
