#ifndef GFXI_IF_h__
#define GFXI_IF_h__

/*----- System Global Control -----*/
#define GCR_BA				0xF0800000
#define MFSEL1				(GCR_BA+0x00C)		// Multiple Function Pin Select 1
#define MFSEL2				(GCR_BA+0x010)		// Multiple Function Pin Select 2
#define INTCR				(GCR_BA+0x03C)		// Integration Control
#define INTSR				(GCR_BA+0x040)		// Integration Status
#define INTCR2				(GCR_BA+0x060)		// Integration control Register 2
#define MFSEL3				(GCR_BA+0x064)		// Multiple function Pin Select 3
#define MDLR                          	(GCR_BA+0x07C)		// Module Disable Lock
#define INTCR3				(GCR_BA+0x09C)		// Integration Control 3
#define MFSEL4				(GCR_BA+0x0B0)		// Multiple function Pin Select 4


/*----- Clock Controller -----*/
#define CLK_BA				0xF0801000
#define IPSRST2 			(CLK_BA+0x24)			// IP software reset flag 2

/*----- BIOS Post Codes FIFO -----*/
#define BIOS_POST_BA		0xF0007000
#define BPCFMSTAT		(BIOS_POST_BA+0x4C)			// BPCF Miscellaneous Status

/*----- Graphics Core Information Interface (GFXI) -----*/
#define	GFXI_BA	    	0xF000E000
#define	DISPHDST	(GFXI_BA + 0x0)				//	Display Head Status (DISPHDST)
#define	FBADL 		(GFXI_BA + 0x4)				//	Frame Buffer Address Low (FBADL)
#define	FBADM 		(GFXI_BA + 0x8)				//	Frame Buffer Address Middle (FBADM)
#define	FBADH 		(GFXI_BA + 0xC)				//	Frame Buffer Address High (FBADH)
#define	HVCNTL		(GFXI_BA + 0x10)			//	Horizontal Visible Counter Low (HVCNTL)
#define	HVCNTH		(GFXI_BA + 0x14)			//	Horizontal Visible Counter High (HVCNTH)
#define	HBPCNTL		(GFXI_BA + 0x18)			//	 Horizontal Back-Porch Counter Low (HBPCNTL)
#define	HBPCNTH		(GFXI_BA + 0x1C)			//	 Horizontal Back-Porch Counter High (HBPCNTH)
#define	VVCNTL		(GFXI_BA + 0x20)			//	Vertical Visible Counter Low (VVCNTL)
#define	VVCNTH		(GFXI_BA + 0x24)			//	Vertical Visible Counter High (VVCNTH)
#define	VBPCNTL		(GFXI_BA + 0x28)			//	 Vertical Back-Porch Counter Low (VBPCNTL)
#define	VBPCNTH		(GFXI_BA + 0x2C)			//	 Vertical Back-Porch Counter High (VBPCNTH)
#define	CURPOSXL	(GFXI_BA + 0x30)			//	 Cursor Position X Low (CURPOSXL)
#define	CURPOSXH	(GFXI_BA + 0x34)			//	 Cursor Position X High (CURPOSXH)
#define	CURPOSYL	(GFXI_BA + 0x38)			//	 Cursor Position Y Low (CURPOSYL)
#define	CURPOSYH	(GFXI_BA + 0x3C)			//	 Cursor Position Y High (CURPOSYH)
#define	GPLLINDI	(GFXI_BA + 0x40)			//	V  Graphics PLL Input Divider (GPLLINDIV)
#define	GPLLFBDI	(GFXI_BA + 0x44)			//	V  Graphics PLL Feedback Divider (GPLLFBDIV)
#define	GPLLST		(GFXI_BA + 0x48)			//	Graphics PLL Status (GPLLST)
#define	KVMHDST		(GFXI_BA + 0x4C)			//	 KVM Head status
#define	COLDEP		(GFXI_BA + 0x50)			//	Color Depth (COLDEP)
#define	VDISPEND	(GFXI_BA + 0x54)			//	General Purpose register 0,Vertical Display End Low (VDISPEND)
#define	VBLANKSTR	(GFXI_BA + 0x58)			//	General Purpose register 1, Vertical Blank Start Low (VBLANKSTR)
#define	VBLANKEND	(GFXI_BA + 0x5C)			//	General Purpose register 2, Vertical Blank End (VBLANKEND)
#define	VTOTAL		(GFXI_BA + 0x60)			//	General Purpose register 3, Vertical Total Low (VTOTAL)
#define	VHIGH		(GFXI_BA + 0x64)			//	General Purpose register 4, Vertical High (VHIGH)
#define	HDISPEND	(GFXI_BA + 0x68)			//	GPR5 RO General Purpose register 5,	Horizontal Display End (HDISPEND)
#define	HBLANKSTR	(GFXI_BA + 0x6C)			//	GPR6 RO General Purpose register 6,	Horizontal Blank Start Low (HBLANKSTR)
#define	HBLANKEND	(GFXI_BA + 0x70)			//	GPR7 RO General Purpose register 7,	Horizontal Blank End (HBLANKEND)
#define	HTOTAL		(GFXI_BA + 0x74)			//	GPR8 RO General Purpose register 8,	Horizontal Total (HTOTAL)
#define	CURWIDTH	(GFXI_BA + 0x78)			//	GPR9 RO General Purpose register 9	Cursor Width (CURWIDTH)
#define	CURHEIGHT	(GFXI_BA + 0x7C)			//	GPR10 RO General Purpose register 1	0 Cursor Height (CURHEIGHT)
#define	CURHSXL		(GFXI_BA + 0x80)			//	GPR11 RO General Purpose register 1	1 Cursor Hot Spot X Position Low (CURHSXL)
#define	GPR12		(GFXI_BA + 0x84)			//	GPR12 RO General Purpose register 1	2 PLL Reset Counter (PLLRSTCNT)
#define	GPR13		(GFXI_BA + 0x88)			//	GPR13 RO General Purpose register 1	3 Cursor Hot Spot High (CURHSH)
#define	GPR14		(GFXI_BA + 0x8C)			//	GPR14 RO General Purpose register 1	4 PLL Reset Counter (PLLRSTCNT)
#define	GPR15		(GFXI_BA + 0x90)			//	GPR15 RO General Purpose register 1	5 Cursor Hot Spot High (CURHSH)

#define	GFXI_BA_FI	    	0xF000E000
#define	GFXI_DISPHDST		(GFXI_BA_FI + 0x0)			//	Display Head Status (DISPHDST)
#define	GFXI_FBADL 		(GFXI_BA_FI + 0x4)			//	Frame Buffer Address Low (FBADL)
#define	GFXI_FBADM 		(GFXI_BA_FI + 0x8)			//	Frame Buffer Address Middle (FBADM)
#define	GFXI_FBADH 		(GFXI_BA_FI + 0xC)			//	Frame Buffer Address High (FBADH)
#define	GFXI_HVCNTL		(GFXI_BA_FI + 0x10)			//	Horizontal Visible Counter Low (HVCNTL)
#define	GFXI_HVCNTH		(GFXI_BA_FI + 0x14)			//	Horizontal Visible Counter High (HVCNTH)
#define	GFXI_HBPCNTL		(GFXI_BA_FI + 0x18)			//	 Horizontal Back-Porch Counter Low (HBPCNTL)
#define	GFXI_HBPCNTH		(GFXI_BA_FI + 0x1C)			//	 Horizontal Back-Porch Counter High (HBPCNTH)
#define	GFXI_VVCNTL		(GFXI_BA_FI + 0x20)			//	Vertical Visible Counter Low (VVCNTL)
#define	GFXI_VVCNTH		(GFXI_BA_FI + 0x24)			//	Vertical Visible Counter High (VVCNTH)
#define	GFXI_VBPCNTL		(GFXI_BA_FI + 0x28)			//	 Vertical Back-Porch Counter Low (VBPCNTL)
#define	GFXI_VBPCNTH		(GFXI_BA_FI + 0x2C)			//	 Vertical Back-Porch Counter High (VBPCNTH)
#define	GFXI_CURPOSXL		(GFXI_BA_FI + 0x30)			//	 Cursor Position X Low (CURPOSXL)
#define	GFXI_CURPOSXH		(GFXI_BA_FI + 0x34)			//	 Cursor Position X High (CURPOSXH)
#define	GFXI_CURPOSYL		(GFXI_BA_FI + 0x38)			//	 Cursor Position Y Low (CURPOSYL)
#define	GFXI_CURPOSYH		(GFXI_BA_FI + 0x3C)			//	 Cursor Position Y High (CURPOSYH)
#define	GFXI_GPLLINDI		(GFXI_BA_FI + 0x40)			//	V  Graphics PLL Input Divider (GPLLINDIV)
#define	GFXI_GPLLFBDI		(GFXI_BA_FI + 0x44)			//	V  Graphics PLL Feedback Divider (GPLLFBDIV)
#define	GFXI_GPLLST		(GFXI_BA_FI + 0x48)			//	Graphics PLL Status (GPLLST)
#define	GFXI_KVMHDST		(GFXI_BA_FI + 0x4C)			//	 KVM Head status
#define	GFXI_COLDEP		(GFXI_BA_FI + 0x50)			//	Color Depth (COLDEP)
#define	GFXI_VDISPEND		(GFXI_BA_FI + 0x54)			//	General Purpose register 0,Vertical Display End Low (VDISPEND)
#define	GFXI_VBLANKSTR		(GFXI_BA_FI + 0x58)			//	General Purpose register 1, Vertical Blank Start Low (VBLANKSTR)
#define	GFXI_VBLANKEND		(GFXI_BA_FI + 0x5C)			//	General Purpose register 2, Vertical Blank End (VBLANKEND)
#define	GFXI_VTOTAL		(GFXI_BA_FI + 0x60)			//	General Purpose register 3, Vertical Total Low (VTOTAL)
#define	GFXI_VHIGH		(GFXI_BA_FI + 0x64)			//	General Purpose register 4, Vertical High (VHIGH)
#define	GFXI_HDISPEND		(GFXI_BA_FI + 0x68)			//	GPR5 RO General Purpose register 5,	Horizontal Display End (HDISPEND)
#define	GFXI_HBLANKSTR		(GFXI_BA_FI + 0x6C)			//	GPR6 RO General Purpose register 6,	Horizontal Blank Start Low (HBLANKSTR)
#define	GFXI_HBLANKEND		(GFXI_BA_FI + 0x70)			//	GPR7 RO General Purpose register 7,	Horizontal Blank End (HBLANKEND)
#define	GFXI_HTOTAL		(GFXI_BA_FI + 0x74)			//	GPR8 RO General Purpose register 8,	Horizontal Total (HTOTAL)
#define	GFXI_CURWIDTH		(GFXI_BA_FI + 0x78)			//	GPR9 RO General Purpose register 9	Cursor Width (CURWIDTH)
#define	GFXI_CURHEIGHT		(GFXI_BA_FI + 0x7C)			//	GPR10 RO General Purpose register 1	0 Cursor Height (CURHEIGHT)
#define	GFXI_CURHSXL		(GFXI_BA_FI + 0x80)			//	GPR11 RO General Purpose register 1	1 Cursor Hot Spot X Position Low (CURHSXL)
#define	GFXI_GPR12		(GFXI_BA_FI + 0x84)			//	GPR12 RO General Purpose register 1	2 PLL Reset Counter (PLLRSTCNT)
#define	GFXI_GPR13		(GFXI_BA_FI + 0x88)			//	GPR13 RO General Purpose register 1	3 Cursor Hot Spot High (CURHSH)
#define	GFXI_GPR14		(GFXI_BA_FI + 0x8C)			//	GPR14 RO General Purpose register 1	4 PLL Reset Counter (PLLRSTCNT)
#define	GFXI_GPR15		(GFXI_BA_FI + 0x90)			//	GPR15 RO General Purpose register 1	5 Cursor Hot Spot High (CURHSH)


// GFX PCI registers (GFX Configuration space registers):
#define GFX_VENDOR_ID			0x0
#define GFX_DEVICE_ID			0x2
#define GFX_COMMAND_REG			0x4
#define GFX_STATUS_REG			0x6
#define GFX_DEV_REV_ID			0x8
#define GFX_PCI_CLASS_CODE		0x9
#define GFX_PCI_CACHE_LINE		0xC
#define GFX_PCI_LATENCY_TMR		0xD
#define GFX_PCI_HEADER_TYPE		0xE
#define GFX_BAR2			0x10
#define GFX_BAR1			0x14
#define GFX_SUB_VENDOR_ID		0x2C
#define GFX_ROMBASE			0x30
#define GFX_CAP_PTR			0x34
#define GFX_INT_LINE			0x3C
#define GFX_INT_PIN			0x3D
#define GFX_MIN_GRANT			0x3E
#define GFX_OPT1			0x40
#define GFX_MGA_INDEX			0x44
#define GFX_MGA_DATA			0x48
#define GFX_SIDW			0x4C
#define GFX_OPT2			0x50
#define GFX_PM_ID			0xDC
#define GFX_PM_CSR			0xE0

// Graphics Configuration register reset values
#define	GFX_VEN_DEV_ID_RV		0x0536102B		// 102Bh - Matrox; 0536h - Ge200W3

#define GFX_VENDOR_ID_RV		0x102B
#define GFX_DEVICE_ID_RV		0x0536
#define GFX_COMMAND_REG_RV		0
#define GFX_STATUS_REG_RV		0x2B0
#define GFX_DEV_REV_ID_RV		0
//#define GFX_PCI_CLASS_CODE_RV	//not final
#define GFX_PCI_CACHE_LINE_RV	0
#define GFX_PCI_LATENCY_TMR_RV	0
#define GFX_PCI_HEADER_TYPE_RV	0
#define GFX_BAR2_RV				0x8
#define GFX_BAR1_RV				0
//#define GFX_SUB_VENDOR_ID_RV	//not final
#define GFX_ROMBASE_RV			0
#define GFX_CAP_PTR_RV			0xDC
#define GFX_INT_LINE_RV			0xFF
#define GFX_INT_PIN_RV			0x1
#define GFX_MIN_GRANT_RV		0x10
// #define GFX_OPT_RV			//strap-dependent
#define GFX_MGA_INDEX_RV		0
//#define GFX_MGA_DATA_RV		//undefined
#define GFX_SIDW_RV			0xD0031050
#define GFX_OPT2_RV			0xB000
#define GFX_PM_ID_RV			0x230001
#define GFX_PMCSR_RV			0

/*------------------------------------------------------------------------*/
/*----------------			AHB2PCI Registers		   -------------------*/
/*------------------------------------------------------------------------*/
#define		AHB2PCI_BA		0xf0400000
#define		WR_PCI_ADDR		(AHB2PCI_BA+0x0)
#define		WR_AHB_IBYTE		(AHB2PCI_BA+0x4)
#define		WR_CONTROL		(AHB2PCI_BA+0x8)
#define		RD_PCI_ADDR		(AHB2PCI_BA+0x20)
#define		RD_AHB_IBYTE		(AHB2PCI_BA+0x24)
#define		RD_CONTROL		(AHB2PCI_BA+0x28)
#define		AHB_IMASK		(AHB2PCI_BA+0x40)
#define		AHB_ISTATUS		(AHB2PCI_BA+0x44)
#define		DATA_REG		(AHB2PCI_BA+0x400)

#define	GFX_IDSEL			0x00010000
#define	MB_IDSEL			0x00020000

// AHB_ISTATUS Register bits
#define	RAHB_ERR			11				// Read AHB Error
#define	RPAR_ERR			10				// Read Parity Error Detected
#define	RD_ABORT			9				// Read Transaction Aborted
#define	RD_END				8				// Read Transaction Ended
#define	WAHB_ERR			3				// Write AHB Error
#define	WPAR_ERR			2				// Write Parity Error Detected
#define	WR_ABORT			1				// Write Transaction Aborted
#define	WR_END				0				// Write Transaction Ended


/*------------------------------------------------------------------------*/
/*----------------------   Variable definitions   ------------------------*/
/*------------------------------------------------------------------------*/


// GFX Registers
#define PALWTADD			(MGABASE1 + 0x3C00)
#define PALDATA				(MGABASE1 + 0x3C01)
#define X_DATAREG			(MGABASE1 + 0x3C0A)
#define MISC				(MGABASE1 + 0x1FC2)
#define SEQ				(MGABASE1 + 0x1FC4)
#define GCTL				(MGABASE1 + 0x1FCE)
#define CRTC				(MGABASE1 + 0x1FD4)
#define CRTCEXT				(MGABASE1 + 0x1FDE)
#define INSTS1				(MGABASE1 + 0x1FDA)


#endif // GFXI_IF_h__
