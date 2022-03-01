/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __NPCM750_GCR_H_
#define __NPCM750_GCR_H_

/* On-Chip POLEG NPCM750 VERSIONS */
#define POLEG_Z1                    0x00A92750
#define POLEG_A1                    0x04A92750
#define POLEG_NPCM750				0x00000000
#define POLEG_NPCM730				0x00300395
#define POLEG_NPCM710				0x00200380

/* Multiple Function Pin Select Register 1 (MFSEL1) */
#define MFSEL1_SIRQSE               31
#define MFSEL1_IOX1SEL              30
#define MFSEL1_DVH1SEL              27
#define MFSEL1_LPCSEL               26
#define MFSEL1_PECIB                25
#define MFSEL1_GSPISEL              24
#define MFSEL1_SMISEL               22
#define MFSEL1_CLKOSEL              21
#define MFSEL1_DVOSEL               18
#define MFSEL1_KBCICSEL             17
#define MFSEL1_R2MDSEL              16
#define MFSEL1_R2ERRSEL             15
#define MFSEL1_RMII2SEL             14
#define MFSEL1_R1MDSEL              13
#define MFSEL1_R1ERRSEL             12
#define MFSEL1_HSI2SEL              11
#define MFSEL1_HSI1SEL              10
#define MFSEL1_BSPSEL               9
#define MFSEL1_SMB2SEL              8
#define MFSEL1_SMB1SEL              7
#define MFSEL1_SMB0SEL              6
#define MFSEL1_S0CS3SEL             5
#define MFSEL1_S0CS2SEL             4
#define MFSEL1_S0CS1SEL             3
#define MFSEL1_SMB5SEL              2
#define MFSEL1_SMB4SEL              1
#define MFSEL1_SMB3SEL              0

/* Multiple Function Pin Select Register 3 (MFSEL3) */
#define MFSEL3_HSDVOSEL             26
#define MFSEL3_MMCCDSEL             25
#define MFSEL3_GPOCSEL              22
#define MFSEL3_WDO2SEL              20
#define MFSEL3_WDO1SEL              19
#define MFSEL3_IOXHSEL              18
#define MFSEL3_PCIEPUSE             17
#define MFSEL3_CLKRUNSEL            16
#define MFSEL3_IOX2SEL              14
#define MFSEL3_PSPI2SEL             13
#define MFSEL3_SD1SEL               12
#define MFSEL3_MMC8SEL              11
#define MFSEL3_MMCSEL               10
#define MFSEL3_RMII1SEL             9
#define MFSEL3_SMB15SEL             8
#define MFSEL3_SMB14SEL             7
#define MFSEL3_SMB13SEL             6
#define MFSEL3_SMB12SEL             5
#define MFSEL3_PSPI1SEL             3
#define MFSEL3_SMB7SEL              2
#define MFSEL3_SMB6SEL              1
#define MFSEL3_SCISEL               0

/* Multiple Function Pin Select Register 4 (MFSEL4) */
#define MFSEL4_SMB11DDC             29
#define MFSEL4_SXCS1SEL             28
#define MFSEL4_SPXSEL               27
#define MFSEL4_RG2SEL               24
#define MFSEL4_RG2MSEL              23
#define MFSEL4_RG1SEL               22
#define MFSEL4_RG1MSEL              21
#define MFSEL4_SP3QSEL              20
#define MFSEL4_S3CS3SEL             19
#define MFSEL4_S3CS2SEL             18
#define MFSEL4_S3CS1SEL             17
#define MFSEL4_SP3SEL               16
#define MFSEL4_SP0QSEL              15
#define MFSEL4_SMB11SEL             14
#define MFSEL4_SMB10SEL             13
#define MFSEL4_SMB9SEL              12
#define MFSEL4_SMB8SEL              11
#define MFSEL4_DBGTRSEL             10
#define MFSEL4_CKRQSEL              9
#define MFSEL4_ESPISEL              8
#define MFSEL4_MMCRSEL              6
#define MFSEL4_SD1PSEL              5
#define MFSEL4_ROSEL                4
#define MFSEL4_ESPIPMESEL           2
#define MFSEL4_BSPASEL              1
#define MFSEL4_JTAG2SEL             0

/* USB PHY1 Control Register (USB1PHYCTL) */
#define USB1PHYCTL_RS				28
/* USB PHY2 Control Register (USB2PHYCTL) */
#define USB2PHYCTL_RS				28

/* Integration Control Register (INTCR) */
#define  INTCR_DUDKSMOD             30
#define  INTCR_DDC3I                29
#define  INTCR_KVMSI                28
#define  INTCR_DEHS                 27
#define  INTCR_GGPCT2_0             24
#define  INTCR_SGC2                 23
#define  INTCR_DSNS_TRIG            21
#define  INTCR_DAC_SNS              20
#define  INTCR_SGC1                 19
#define  INTCR_LDDRB                18
#define  INTCR_GIRST                17
#define  INTCR_DUDKSEN              16
#define  INTCR_DACOFF               15
#define  INTCR_DACSEL               14
#define  INTCR_GFXINT               12
#define  INTCR_DACOSOVR             10
#define  INTCR_GFXIFDIS             8
#define  INTCR_GFXINT2              7
#define  INTCR_VGAIOEN              6
#define  INTCR_R1EN                 5
#define  INTCR_PSPIFEN              4
#define  INTCR_HIFEN                3
#define  INTCR_SMBFEN               2
#define  INTCR_MFTFEN               1
#define  INTCR_KCSRST_MODE          0

/* Integration Control Register 3 (INTCR3) */
#define  INTCR3_PCIRREL             30
#define  INTCR3_GFXRSTDLY           26
#define  INTCR3_GFXRDEN             25
#define  INTCR3_DACSBYOFF           23
#define  INTCR3_RCCORER             22
#define  INTCR3_GFX_ORL0            20
#define  INTCR3_PECIVSEL            19
#define  INTCR3_PCIEDEM             17
#define  INTCR3_USBPHYSW            12
#define  INTCR3_GMMAP               8
#define  INTCR3_FIU_FIX             6
#define  INTCR3_GFXACCDIS           4
#define  INTCR3_HSRDIS              2
#define  INTCR3_BIOSEN              1
#define  INTCR3_HHRDIS              0

/* Serial Ports (SPSWC) */
#define SPSWC_RTSS  7
#define SPSWC_DTRS  6
#define SPSWC_DCDI  5
#define SPSWC_SPMOD 0
#define SPMOD_MASK 7
#define SPMOD_MODE1 0
#define SPMOD_MODE2 1
#define SPMOD_MODE3 2
#define SPMOD_MODE4 3
#define SPMOD_MODE5 4
#define SPMOD_MODE6 5
#define SPMOD_MODE7 6

/* I2C Segment Pin Select Register (I2CSEGSEL) */
#define I2CSEGSEL_S0DECFG			3
#define I2CSEGSEL_S4DECFG			17

/* I2C Segment Control Register (I2CSEGCTL) */
#define I2CSEGCTL_S0DEN				20
#define I2CSEGCTL_S0DWE				21
#define I2CSEGCTL_S4DEN				24
#define I2CSEGCTL_S4DWE				25
#define I2CSEGCTL_INIT_VAL		0x9333F000

struct npcm_gcr {
	unsigned int  pdid;
	unsigned int  pwron;
	unsigned char res1[0x4];
	unsigned int  mfsel1;
	unsigned int  mfsel2;
	unsigned int  miscpe;
	unsigned char res2[0x20];
	unsigned int  spswc;
	unsigned int  intcr;
	unsigned int  intsr;
	unsigned char res3[0xc];
	unsigned int  hifcr;
	unsigned int  sd1irv1;
	unsigned int  sd1irv2;
	unsigned char res4[0x4];
	unsigned int  intcr2;
	unsigned int  mfsel3;
	unsigned int  srcnt;
	unsigned int  ressr;
	unsigned int  rlockr1;
	unsigned int  flockr1;
	unsigned int  dscnt;
	unsigned int  mdlr;
	unsigned char res5[0x18];
	unsigned int  davclvlr;
	unsigned int  intcr3;
	unsigned char res6[0xc];
	unsigned int  vsintr;
	unsigned int  mfsel4;
	unsigned int  sd2irv1;
	unsigned int  sd2irv2;
	unsigned char res7[0x8];
	unsigned int  cpbpntr;
	unsigned char res8[0x8];
	unsigned int  cpctl;
	unsigned int  cp2bst;
	unsigned int  b2cpnt;
	unsigned int  cppctl;
	unsigned int  i2csegsel;
	unsigned int  i2csegctl;
	unsigned int  vsrcr;
	unsigned int  mlockr;
	unsigned char res9[0x4c];
	unsigned int  scrpad;
	unsigned int  usb1phyctl;
	unsigned int  usb2phyctl;
};

#endif
