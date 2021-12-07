#ifndef _NPCM_SMB_H_
#define _NPCM_SMB_H_
#include <linux/bitops.h>

/* SMBCTL1 */
#define SMBCTL1_START		BIT(0)
#define SMBCTL1_STOP		BIT(1)
#define SMBCTL1_INTEN		BIT(2)
#define SMBCTL1_ACK			BIT(4)
#define SMBCTL1_STASTRE		BIT(7)

/* SMBCTL2 */
#define SMBCTL2_ENABLE		BIT(0)

/* SMBCTL3 */
#define SMBCTL3_SCL_LVL		BIT(7)
#define SMBCTL3_SDA_LVL		BIT(6)

/* SMBCST */
#define SMBCST_BB		BIT(1)
#define SMBCST_TGSCL	BIT(5)

/* SMBST */
#define SMBST_XMIT		BIT(0)
#define SMBST_MASTER	BIT(1)
#define SMBST_STASTR	BIT(3)
#define SMBST_NEGACK	BIT(4)
#define SMBST_BER		BIT(5)
#define SMBST_SDAST		BIT(6)

/* SMBCST3 in bank0 */
#define SMBCST3_EO_BUSY	BIT(7)

/* SMBFIF_CTS in bank1 */
#define SMBFIF_CTS_CLR_FIFO	BIT(6)

#define SMBFIF_CTL_FIFO_EN	BIT(4)
#define SMBCTL3_BNK_SEL		BIT(5)

struct smb_bank0_regs {
	u8 addr3;
	u8 addr7;
	u8 addr4;
	u8 addr8;
	u16 addr5;
	u16 addr6;
	u8 cst2;
	u8 cst3;
	u8 ctl4;
	u8 ctl5;
	u8 scllt;
	u8 fif_ctl;
	u8 sclht;
};

struct smb_bank1_regs {
	u8 fif_cts;
	u8 fair_per;
	u16 txf_ctl;
	u32 t_out;
	u8 cst2;
	u8 cst3;
	u16 txf_sts;
	u16 rxf_sts;
	u8 rxf_ctl;
};

struct npcm750_smb_regs {
	u16	sda;
	u16 st;
	u16 cst;
	u16 ctl1;
	u16 addr;
	u16 ctl2;
	u16 addr2;
	u16 ctl3;
	union {
		struct smb_bank0_regs bank0;
		struct smb_bank1_regs bank1;
	};

};
#endif
