#include <asm/arch/poleg_gfx.h>   
#include <stdlib.h>
#include <string.h>
#include <command.h>
#include <common.h>
#include <environment.h>
#include <dm.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>

typedef enum CommandType
{
	PCI_IO_READ = 2,
	PCI_IO_WRITE = 3,
	PCI_MEM_READ = 6,
	PCI_MEM_WRITE = 7,
	PCI_CFG_READ = 10,
	PCI_CFG_WRITE = 11
} CommandType;

typedef enum GFX_MEMORY_MAP_E
{
	GMMAP_0700_000 = 0,
	GMMAP_0F00_000 = 1,
	GMMAP_1F00_000 = 2,
	GMMAP_3F00_000 = 3,
	GMMAP_7F00_000 = 4,
	NUM_OF_GMMAP_OPTIONS
}GFX_MEMORY_MAP_E;

typedef enum GFX_ColorDepth
{
	COLOR_DEPTH_8BIT,
	COLOR_DEPTH_15BIT,
	COLOR_DEPTH_16BIT,
	COLOR_DEPTH_24BIT,
	COLOR_DEPTH_32BIT
} GFX_ColorDepth;

typedef struct GFXI_RegInfo
{
	u32	address;
	char	Name[30];
} GFXI_RegInfo;

const GFXI_RegInfo GFXI_Regs[] =
{
	{ GFXI_DISPHDST		, "DISPHDST     "},
	{ GFXI_FBADL 		, "FBADL        "},
	{ GFXI_FBADM 		, "FBADM        "},
	{ GFXI_FBADH 		, "FBADH        "},
	{ GFXI_HVCNTL		, "HVCNTL       "},
	{ GFXI_HVCNTH		, "HVCNTH       "},
	{ GFXI_HBPCNTL		, "HBPCNTL      "},
	{ GFXI_HBPCNTH		, "HBPCNTH      "},
	{ GFXI_VVCNTL		, "VVCNTL       "},
	{ GFXI_VVCNTH		, "VVCNTH       "},
	{ GFXI_VBPCNTL		, "VBPCNTL      "},
	{ GFXI_VBPCNTH		, "VBPCNTH      "},
	{ GFXI_CURPOSXL		, "CURPOSXL     "},
	{ GFXI_CURPOSXH		, "CURPOSXH     "},
	{ GFXI_CURPOSYL		, "CURPOSYL     "},
	{ GFXI_CURPOSYH		, "CURPOSYH     "},
	{ GFXI_GPLLINDI		, "GPLLINDIV    "},
	{ GFXI_GPLLFBDI		, "GPLLFBDIV    "},
	{ GFXI_GPLLST		, "GPLLST       "},
	{ GFXI_KVMHDST		, "KVMHDST      "},
	{ GFXI_COLDEP		, "COLDEP       "},
	{ GFXI_VDISPEND		, "VDISPEND     "},
	{ GFXI_VBLANKSTR	, "VBLANKSTR    "},
	{ GFXI_VBLANKEND	, "VBLANKEND    "},
	{ GFXI_VTOTAL		, "VTOTAL       "},
	{ GFXI_VHIGH		, "VHIGH        "},
	{ GFXI_HDISPEND		, "HDISPEND     "},
	{ GFXI_HBLANKSTR	, "HBLANKSTR    "},
	{ GFXI_HBLANKEND	, "HBLANKEND    "},
	{ GFXI_HTOTAL		, "HTOTAL       "},
	{ GFXI_CURWIDTH		, "CURWIDTH     "},
	{ GFXI_CURHEIGHT	, "CURHEIGHT    "},
	{ GFXI_CURHSXL		, "CURHSXL      "},
	{ GFXI_GPR12		, "GPR12        "},
	{ GFXI_GPR13		, "GPR13        "},
	{ GFXI_GPR14		, "GPR14        "},
	{ GFXI_GPR15		, "GPR15        "}
};


#define NUM_OF_GFXI_REGS (sizeof(GFXI_Regs)/sizeof(GFXI_RegInfo))

u32 FB_BaseAddresses[NUM_OF_GMMAP_OPTIONS] = {0x07000000, 0x0F000000, 0x1F000000, 0x3F000000, 0x7F000000};
int GFX_IsPllDebug_g , AHB2PCI_IsLogEnable_g;
u32 FB_BA = 0;		// Frame buffer base address
u32 MGABASE1 = 0;	// GFX Memory space address

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  GENERAL FUNCTIONS  /////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------------*/
/* Function:     if (!AHB2PCI_Memory_Write											  */
/*                                                                           	  */
/* Parameters:                                                               	  */
/*               address  - The configuration register address, to write to.	  */
/*               size	  -	The size of the written data.                      	  */
/*               data	  -	The data to write, starting at the lower byte.     	  */
/*																			 	  */
/* Returns:      true if the transaction succeeded.							 	  */
/*				 If the size is less than 4 bytes, data will be in the lower byte.*/
/* Side effects: None                                                        	  */
/* Description:                                                              	  */
/*               Memory Write transaction through the AHB2PCI Bridge.  	          */
/* Notes:																		  */
/* In memory transactions, as opposed to configuration transactions, there is the */
/* regular "shifting" of the data, so the data can start at the lowest byte of	  */
/* the data register, and the address defines where to write the data (no need    */
/* for address alignment).														  */
/*--------------------------------------------------------------------------------*/

int AHB2PCI_Memory_Write(u32 address, u8 size, u32 data)
{
	u32 cnt = 0;

	if ((size > sizeof(u32)) || (size == 3))
	{
		printf("AHB2PCI_Memory_Write error : Illegal data size\n");
		return(false);
	}

	// Set the Initial Byte offset
	// A non-zero offset cannot be used in Memory Write transactions
	writel(0,WR_AHB_IBYTE);

	// Clear Status Register
	writel(readl(AHB_ISTATUS) , AHB_ISTATUS);
	
	// Write the configuration space address to read
	writel(address , WR_PCI_ADDR);

	// Write the data
	writel(data , DATA_REG);

	// Initiate the transaction
	writel((1 << 0) | (PCI_MEM_WRITE << 4) | (size << 8) , WR_CONTROL);
	
	// Check that transaction has ended
	while(!(readl(AHB_ISTATUS)&0x1))
	{
		cnt++;
		if(cnt > 1000)
		{
			printf("AHB2PCI_Memory_Write: time out! AHB_ISTATUS = 0x%04lx\n", readl((AHB2PCI_BA+0x44)));
			return(false);
		}
	}

	if (AHB2PCI_IsLogEnable_g == 1)
		printf("AHB2PCI_Memory_Write: Addr=0x%lX, Size=%d, Data=0x%lX\n", address, size, data);

	return(true);
} // *End of AHB2PCI_Memory_Write*
/*--------------------------------------------------------------------------------*/
/* Function:     AHB2PCI_Memory_Read											  */
/*                                                                           	  */
/* Parameters:                                                               	  */
/*               address  - The configuration register address, to read from.	  */
/*               size	  -	The size of the read data.                       	  */
/*																			 	  */
/* Returns:      The read data.												 	  */
/*				 If the size is less than 4 bytes, data will be in the lower byte.*/
/* Side effects: None                                                        	  */
/* Description:                                                              	  */
/*               Memory Read transaction through the AHB2PCI Bridge.        	  */
/*--------------------------------------------------------------------------------*/

u32 AHB2PCI_Memory_Read(u32 address, u8 size)
{
	u32 readData, aligned_addr;
	u8 cnt = 0;

	if (size != 4)
	{
		printf("AHB2PCI_Memory_Read error : Illegal data size (%d). Read transfer must be 4 bytes.\n", size);
		return(0xDEADBEEF);
	}

	if (address % 4 != 0)
	{
		printf("AHB2PCI_Memory_Read error : Illegal address (0x%X) must be aligned to DWORD.\n", size);
		return(0xDEADBEEF);
	}

	// Set the Initial Byte offset
	// A non-zero offset cannot be used in Memory Read transactions
	writel(0,RD_AHB_IBYTE);

	// Clear Status Register
	writel(readl(AHB_ISTATUS) , AHB_ISTATUS);

	// Write the configuration space address to read
	aligned_addr = (address & ~0x3); // Bits 0,1 must be 0

	writel(aligned_addr , RD_PCI_ADDR);

	// Initiate the transaction
	writel((1 << 0) | (PCI_MEM_READ << 4) | (size << 8), RD_CONTROL);

	// Check that the transaction has ended
	while(!((readl(AHB_ISTATUS)&0x100)>>8))
	{
		cnt++;
		if(cnt > 1000)
		{
			printf("AHB2PCI_Memory_Read: Read transaction time out! AHB_ISTATUS = 0x%04lx\n", readl(AHB_ISTATUS));
			return(0xDEADBEEF);
		}
	}
	readData = readl(DATA_REG);

	if (AHB2PCI_IsLogEnable_g == 1)
		printf("AHB2PCI_Memory_Read: Addr=0x%lX, Size=%d, Data=0x%lX\n", address, size, readData);

	return(readData);

} // *End of AHB2PCI_Memory_Read*
/*--------------------------------------------------------------------------------*/
/* Function:     AHB2PCI_Config_Write											  */
/*                                                                           	  */
/* Parameters:                                                               	  */
/*               address  - The configuration register address, to write to.	  */
/*               size	  -	The size of the written data.                      	  */
/*               data	  -	The data to write, starting at the lower byte.     	  */
/*               iByte	  -	The offset (Initial Byte)							  */
/*																			 	  */
/* Returns:      The read data.												 	  */
/*				 If the size is less than 4 bytes, data will be in the lower byte.*/
/* Side effects: None                                                        	  */
/* Description:                                                              	  */
/*               Configuration Write transaction through the AHB2PCI Bridge.  	  */
/* Important:																	  */
/* The two lower bits of the address are ignored (alignment to DWORD), and the    */
/* offset is taken from	iByte register.											  */
/* This function can get either a non-aligned address OR aligned address, and iByte.*/
/* The function takes care of "Data" - and puts it in the right place in the data */
/* reg(with iByte offset).														  */
/*--------------------------------------------------------------------------------*/

int AHB2PCI_Config_Write(u32 address, u8 size, u32 data, u8 iByte)
{
	u32 offset, cnt=0;

	if ((size > sizeof(u32)) || (size == 3))
	{
		printf("AHB2PCI_Config_Write error: Illegal data size\n");
		return(false);
	}

	offset = (address % 4) + iByte;
	if (offset > 3)
	{
		printf("Config_Write: Illegal offset(iByte): %ld. Must be in range 0-3\n", offset);
		return(false);
	}
	// Verify single data phase
	if((offset + size) > 4)
	{
		printf("Config_Write: Offset(%ld) + size exceeding 4 byte size.\n", offset);
		return(false);
	}

	// Set the Initial Byte offset
	// A non-zero offset can be used only here, in Configuration Write transactions
	writel(offset,WR_AHB_IBYTE);

	// Clear Status Register
	writel(readl(AHB_ISTATUS) , AHB_ISTATUS);

	// Write the configuration space address to read (aligned to DWORD)
	writel((address & 0xFFFFFFFC) , WR_PCI_ADDR);

	// Write the data
	// The data must start at the right byte in the Data Register, according to the offset
	writel(((data & ((((1 << (size*8 - 0 + 1)) - 1)) << 0)) << (offset * 8)),DATA_REG);

	// Initiate the transaction
	writel((1 << 0) | (PCI_CFG_WRITE << 4) | (size << 8) , WR_CONTROL);

	// Check that the transaction has ended
	while(!(readl(AHB_ISTATUS)&0x1))
	{
		cnt++;
		if(cnt > 1000)
		{
			printf("AHB2PCI_Config_Write: time out! AHB_ISTATUS = 0x%04lx\n", readl(AHB_ISTATUS));
			return(false);
		}
	}

	if (AHB2PCI_IsLogEnable_g == 1)
		printf("AHB2PCI_Config_Write: wrote to address=0x%lx, size=%d, data=0x%lx, ibyte=%ld\n", address, size, data, offset);

	return(true);
} // *End of AHB2PCI_Config_Write*
/*--------------------------------------------------------------------------------*/
/* Function:     AHB2PCI_Config_Read											  */
/*                                                                           	  */
/* Parameters:                                                               	  */
/*               address  - The configuration register address, to read from.	  */
/*               size	  -	The size of the read data.                       	  */
/*																			 	  */
/* Returns:      The read data.												 	  */
/*				 If the size is less than 4 bytes, data will be in the lower byte.*/
/* Side effects: None                                                        	  */
/* Description:                                                              	  */
/*               Configuration Read transaction through the AHB2PCI Bridge.  	  */
/*--------------------------------------------------------------------------------*/
u32 AHB2PCI_Config_Read(u32 address, u8 size)
{
	u32 readData, aligned_addr, aligned_data, offset = 0, cnt=0;

	if ((size > sizeof(u32)) || (size == 3))
	{
		printf("AHB2PCI_Memory_Read error : Illegal data size (%d).\n", size);
		return(0xDEADBEEF);
	}

	// Set the Initial Byte offset
	// A non-zero offset cannot be used in Read transactions.
	writel(0,RD_AHB_IBYTE);
	
	// Clear Status Register
	writel(readl(AHB_ISTATUS) , AHB_ISTATUS);

	// Write the configuration space address to read
	aligned_addr = (address & ~0x3); // Bits 0,1 must be 0

	writel(aligned_addr , RD_PCI_ADDR);

	// Initiate the transaction
	writel(((1 << 0) | (PCI_CFG_READ << 4) | (4 << 8)),(AHB2PCI_BA+0x28)); // read size must be 4

	// Check that the transaction has ended
	while(!((readl(AHB_ISTATUS)&0x100)>>8))
	{
		cnt++;
		if(cnt > 1000)
		{
			printf("AHB2PCI_Config_Read: Read transaction time out! AHB_ISTATUS = 0x%04lx\n", readl(AHB_ISTATUS));
			return(0xDEADBEEF);
		}
	}
	// Read the data (the whole DWORD is read)
	readData = readl(DATA_REG);

	offset = (address % 4);			 // the original address includes the offset in the lower two bits

	aligned_data = (readData >> (offset * 8)) & ((((1 << (size*8 - 0 + 1)) - 1)) << 0);

	if (AHB2PCI_IsLogEnable_g == 1)
		printf("AHB2PCI_Config_Read: read from address=0x%lx, size=%d, data=0x%lx (DW data=0x%lx)\n", address, size, aligned_data, readData);

	return(aligned_data);
} // *End of AHB2PCI_Config_Read*



/*--------------------------------------------------------------------------------*/
/* Function:     GFX_FB_Initialize												  */
/*                                                                           	  */
/* Parameters:                                                               	  */
/*               None															  */
/*																			 	  */
/* Returns:		 None															  */
/* Side effects: Set the global FB_BA with the Frame Buffer Base Address       	  */
/* Description:                                                              	  */
/*               Find the Frame Buffer base physical address		         	  */
/*--------------------------------------------------------------------------------*/

int FB_Initialize(void)
{

	u8 gmmapVal = ((readl(INTCR3)&0x700)>>8);

	if (gmmapVal >= NUM_OF_GMMAP_OPTIONS)
	{
		printf("Error: Invalid value for Graphics Memory Map (GMMAP = 0x%u) \n", gmmapVal);
		return 0;
	}
	FB_BA = FB_BaseAddresses[gmmapVal];

	printf("FB_Initialize(): Frame-Buffer address = %08lxh\n", FB_BA);
	return 1;
} // *End of FB_Initialize*

//----------------------------------------------------------------
int PLL_Configure (u8 _GPLLINDIV, u8 _GPLLFBDIV, u8 _GPLLST)
{
	printf("> Pixel Clock Configuration start\n");

	// Configure PLL (write to Pixel clock control - power down pixel clock)
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x1A))  {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x8D)) {return (false);}

	// Configure PLL C only
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0xB6)) {return (false);} // _GPLLFBDIV (FBDV)
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), _GPLLFBDIV)) {return (false);}
	printf("  * _GPLLFBDIV (FBDV): %08lxh\n", AHB2PCI_Memory_Read((MGABASE1 + 0x3C08), sizeof(u32)));

	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0xB7)) {return (false);} // _GPLLINDIV (INDV + FBDV8)
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), _GPLLINDIV)) {return (false);}
	printf("  * _GPLLINDIV (INDV + FBDV8): %08lxh\n", AHB2PCI_Memory_Read((MGABASE1 + 0x3C08), sizeof(u32)));

	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0xB8)) {return (false);}// _GPLLST (OTDV1/OTDV2)
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), _GPLLST)) {return (false);}
	printf("  * _GPLLST (OTDV1/OTDV2): %08lxh\n", AHB2PCI_Memory_Read((MGABASE1 + 0x3C08), sizeof(u32)));


	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x1A)) {return (false);}// XPIXCLKCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x09)) {return (false);}
	printf("  * XPIXCLKCTRL: %08lxh\n", AHB2PCI_Memory_Read((MGABASE1 + 0x3C08), sizeof(u32)));

	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x18)) {return (false);}// XVREFCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x0F)) {return (false);}
	printf("  * XVREFCTRL: %08lxh\n", AHB2PCI_Memory_Read((MGABASE1 + 0x3C08), sizeof(u32)));

	printf("> Pixel Clock Configuration done.\n");

	return (true);
}

void GFX_ClearMemorySpace(u8 presetValue)
{
	if(FB_Initialize() == 1)
		memset((void *)FB_BA, presetValue, (16 * 1024 * 1024));
	else
		printf("> FB_Initialize failed.\n");
}

int GFX_ConfigureDisplayTo1920x1200(GFX_ColorDepth colorDepth)
{
	printf("GFX Display Configuration to 1920x1200\n");
	u32	data;
	int		i , cnt = 0;

	if(FB_Initialize() == 0)
		return(false);
	// Wait until GFX Device/Vendor ID is read correctly
	while (AHB2PCI_Config_Read(GFX_IDSEL + GFX_VENDOR_ID, sizeof(u32)) != GFX_VEN_DEV_ID_RV)
	{
		printf("Waiting until PCI is out of reset;\n");
		if (cnt == 10)
		{
			printf("Test Aborted .\n");
			return (false);
		}
		mdelay (1000);
		cnt++;
	}

	printf("> Found correct GFX Device/Vendor ID 0x%08x. \n", GFX_VEN_DEV_ID_RV);
	// Get the MGA Registers base address (MGABASE1)
	AHB2PCI_Config_Write(GFX_IDSEL + GFX_BAR1, sizeof(u32), MGABASE1 = 0x10000000, 0);
	MGABASE1 = AHB2PCI_Config_Read(GFX_IDSEL + GFX_BAR1, sizeof(u32));
	if (MGABASE1 == 0)
	{
		printf("TEST_ConfigureDisplay failed : MGABASE1 is invalid = %08lxh\n", MGABASE1);
	}
	else
	{
		printf("TEST_ConfigureDisplay : MGABASE1 = %08lxh\n", MGABASE1);
	}
	AHB2PCI_Config_Write(GFX_IDSEL + GFX_BAR2, sizeof(u32), 0xf0000000, 0);

	printf("MGABASE1 = %08lxh, MGABASE2 = %08lxh\n",
		AHB2PCI_Config_Read(GFX_IDSEL + GFX_BAR1, sizeof(u32)),
		AHB2PCI_Config_Read(GFX_IDSEL + GFX_BAR2, sizeof(u32)));

	// Enable Bus Master and Memory Space
	data = AHB2PCI_Config_Read(GFX_IDSEL + GFX_COMMAND_REG, sizeof(u32));
	AHB2PCI_Config_Write(GFX_IDSEL + GFX_COMMAND_REG, sizeof(u32), data | 0x06, 0);

	/*----------------------------------------------------------------------------------------------*/
	/* Graphics PLL Configuration																	*/
	/*----------------------------------------------------------------------------------------------*/

	u8 _GPLLINDIV,_GPLLFBDIV,_GPLLST;
	_GPLLINDIV = 0x09; // _GPLLINDIV (INDV + FBDV8)
	_GPLLFBDIV = 0xDE; // _GPLLFBDIV (FBDV)
	_GPLLST    = 0x0C; // _GPLLST (OTDV1/OTDV2)
	if(!PLL_Configure(_GPLLINDIV, _GPLLFBDIV, _GPLLST)) {return (false);}
		/*----------------------------------------------------------------------------------------------*/
	/* Sequencers Configuration																		*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0100)){return (false);}	// Reset sequencer
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0101)){return (false);}	// 8-dot character clock
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0302)){return (false);}	// Map 3 write enable
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0003)){return (false);}	// another mapping of VGA (??)
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0204)){return (false);}	// 256K memory is installed
	if (colorDepth == COLOR_DEPTH_24BIT)
	{
		if (!AHB2PCI_Memory_Write(MISC, sizeof(u8), 0x09))
			return (false);		// Configure CRTC addresses, Select MGA pixel clock, VSYNC and HSYNC active LOW
	}
	else
	{
		if (!AHB2PCI_Memory_Write(MISC, sizeof(u8), 0xC9))
			return (false);		// Configure CRTC addresses, Select MGA pixel clock, VSYNC and HSYNC active LOW
	}
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0300)){return (false);}	// Reset sequencer

	printf("> Sequencers Configuration done.\n");
	/*----------------------------------------------------------------------------------------------*/
	/* Graphics Controller Configuration															*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0000)){return (false);}	// Reset bytes in the VGA memory map
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0001)){return (false);}	//  (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0002)){return (false);}	//  (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0003)){return (false);}	//  (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0004)){return (false);}	//  (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x1005)){return (false);}	// Selects Odd/Even addressing mode
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0E06)){return (false);}	// VGA mode select, Memory map is '0b11'
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0007)){return (false);}	// ?? (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0xFF08)){return (false);}	// Write mask to 0xFF

	printf("> Graphics Controller Configuration done.\n");
	/*----------------------------------------------------------------------------------------------*/
	/* CRTC Controller Configuration																*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xFF00)){return (false);} // HTOTAL[7:0] = 0xA3 (HTOTAL[8] appears in CRTCEXT1[0])
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xEF01)){return (false);} // HDISP_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xEF02)){return (false);} // HBLANK_START HBLKSTR[8] appears in CRTCEXT1[1]
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x8303)){return (false);} // HBLANK_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xF504)){return (false);} // HRETRACE_START
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x1905)){return (false);} // HRETRACE_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xD106)){return (false);} // VTOTAL		// was 24
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x1007)){return (false);} // OVERFLOW (overflow of other fields)
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x0008)){return (false);} // PRE_ROW_SCAN
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x4009)){return (false);} // MAX_SCAN_LINE
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x200A)){return (false);} // CUR_START
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x000B)){return (false);} // CUR_END

	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x000C)){return (false);} // ST_ADDR_HI
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x800D)){return (false);} // ST_ADDR_LOW - check where should FB_BA be
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xFF00)){return (false);} // HTOTAL again?

	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x000E)){return (false);} // CUR_POS_HI
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x000F)){return (false);} // CUR_POS_LOW
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xB210)){return (false);} // VRETRACE_START
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x2811)){return (false);} // VRETRACE_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xAF12)){return (false);} // VDISP_END
	if ((colorDepth == COLOR_DEPTH_32BIT)|| (colorDepth == COLOR_DEPTH_24BIT)) {
		if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x0013))
			return (false); // OFFSET
	} else {
		if (colorDepth == COLOR_DEPTH_16BIT)
		{
			if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xF013))
				return (false); // OFFSET - note FB should be also moved
			FB_BA += 0x100*4;
		}
	}
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x0014)){return (false);} // UND_LOC
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xAF15)){return (false);} // VBLANK_START
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xD216)){return (false);} // VBLANK_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xC317)){return (false);} // MODE_CTRL
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xFF18)){return (false);} // LINE_COMP
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x0022)){return (false);} // CPU_LATCH_RD
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x8024)){return (false);} // DATA_SEL
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x3426)){return (false);} // ATTRIB_ADDR


	printf("> CRTC Configuration done.\n");

	/*----------------------------------------------------------------------------------------------*/
	/* CRTC Controller Extension Configuration														*/
	/*----------------------------------------------------------------------------------------------*/

	if (colorDepth == COLOR_DEPTH_32BIT) {
		if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x1000))
			return (false); // ADDR_GEN
	} else {
		if ((colorDepth == COLOR_DEPTH_16BIT) || (colorDepth == COLOR_DEPTH_24BIT)) {
			if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0000))
				return (false); // ADDR_GEN
		}
	}

	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0801)){return (false);} // H_COUNT
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0xAD02)){return (false);} // V_COUNT

	if (colorDepth == COLOR_DEPTH_32BIT) {
		if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x8303))
			return (false); //MISC
	} else if (colorDepth == COLOR_DEPTH_16BIT) {
		if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x8103))
			return (false); // MISC
	} else if (colorDepth == COLOR_DEPTH_24BIT) {
		if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x8203))
			return (false); // MISC
	}

	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0004)){return (false);} // MEM_PAGE
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0005)){return (false);} // HVIDMID
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x2006)){return (false);} // P_REQ_CTL
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0007)){return (false);} // REQ_CTL

	// Poleg - New registers
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x1030)){return (false);} //MGABURSTSIZE
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0534)){return (false);} //MGAREQCTL
	printf("> CRTC Extension configuration done.\n");

	/*----------------------------------------------------------------------------------------------*/
	/* DAC Eclipse Configuration																	*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x04)) {return (false);} // XCURADDL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x4F)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x05)) {return (false);} // XCURADDH
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x3A)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x06)) {return (false);} // XCURCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x08)) {return (false);} // XCURCOL0RED
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x82)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x09)) {return (false);} // XCURCOL0GREEN
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xFB)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x0A)) {return (false);} // XCURCOL0BLUE
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x44)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x0C)) {return (false);} // XCURCOL1RED
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x2C)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x0D)) {return (false);} // XCURCOL1GREE
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x75)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x0E)) {return (false);} // XCURCOL1BLUE
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x8B)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x10)) {return (false);} // XCURCOL2RED
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x14)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x11)) {return (false);} // XCURCOL2GREEN
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x2E)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x12)) {return (false);} // XCURCOL2BLUE
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x4A)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x19)) {return (false);} // XMULCTRL
	if (colorDepth == COLOR_DEPTH_32BIT) {
		if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x07))
			return (false);
	} else if (colorDepth == COLOR_DEPTH_16BIT) {
		if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x02))
			return (false);
	} else if (colorDepth == COLOR_DEPTH_24BIT) {
		if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x03))
			return (false);
	}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x1A)) {return (false);}		// XPIXCLKCTRL (should be 9?)
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x09)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x1D)) {return (false);}		// XGENCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x20)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x1E)) {return (false);}		// XMISCCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x1F)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2A)) {return (false);}		// XGENIOCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x01)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2B)) {return (false);}		// XGENIODATA
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x02)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2C)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x14)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2D)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x71)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2E)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x01)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2F)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x40)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x38)) {return (false);}		// XZOOMCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x3A)) {return (false);}		// XSENSETEST (RO)
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x3C)) {return (false);}		// XCRCREML
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x3D)) {return (false);}		// XCRCREMH (RO)
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xB5)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x3E)) {return (false);}		// XCRCBITSEL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x0B)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x40)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xFD)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x41)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x42)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xB6)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x43)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x44)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x11)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x45)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x10)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x46)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x07)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x48)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x0C)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x49)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x75)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4A)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x07)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4C)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x13)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4D)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x5D)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4E)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x05)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4F)) {return (false);}		// XPIXPLLSTAT
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x51)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x01)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x18)) {return (false);}	// XVREFCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x04)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x52)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x61)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x53)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x7D)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x54)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xD1)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x55)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xFE)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x56)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x7D)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x57)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x3A)){return (false);}

	printf("> DAC Eclipse Configuration done.\n");

	/*----------------------------------------------------------------------------------------------*/
	/* palette Configuration																		*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(PALWTADD, sizeof(u8), 0)){return (false);}

	if ((colorDepth == COLOR_DEPTH_32BIT) || (colorDepth == COLOR_DEPTH_24BIT))
	{
		// palette for 8:8:8 color format
		for (i = 0; i <= 255; i++)
		{
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i)){return (false);}
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i)){return (false);}
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i)){return (false);}
		}
	}
	else if (colorDepth == COLOR_DEPTH_16BIT) {
		// palette for 5:6:5 color format
		for (i = 0; i <= 63; i++)
		{
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i * 8)){return (false);}
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i * 4)){return (false);}
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i * 8)){return (false);}
		}
	}

	printf("> Palette Configuration done.\n");

		/*----------------------------------------------------------------------------------------------*/
		/* Configuration Done																			*/
		/*----------------------------------------------------------------------------------------------*/

		// fill frame buffer with random lines
		u32 lineNum = 0;
		const u32 numOfBars=16;
		u32 Address = FB_BA;
		u32 Index;
		u32 Color;

		if (colorDepth == COLOR_DEPTH_32BIT)
			for (lineNum = 0; lineNum < numOfBars; ++lineNum)
			{
				Color = (u32)rand() ^ ((u32)rand()<<16);
				for (Index =0; Index<((1920*1200)/numOfBars); Index++, Address=Address+4)
					writel(Color,Address);
			}
		else if (colorDepth == COLOR_DEPTH_16BIT)
			for (lineNum = 0; lineNum < numOfBars; ++lineNum)
			{
				Color = (u32)rand() ^ ((u32)rand()<<16);
				for (Index =0; Index<((1920*1200)/numOfBars); Index++, Address=Address+2)
					(*((u16 *)(Address))) = ((u16) Color);
			}
		else if (colorDepth == COLOR_DEPTH_24BIT)
		{
			printf("\n Using 32 bit frame buffer method for 24bit mode ! \n");
			for (lineNum = 0; lineNum < numOfBars; ++lineNum)
			{
				Color = (u32)rand() ^ ((u32)rand()<<16);
				for (Index =0; Index<((1920*1200)/numOfBars); Index++, Address=Address+4)
					writel(Color,Address);
			}
		}


	printf("> Filling Frame Buffer done.\n");
	return (true);
}

int GFX_ConfigureDisplayTo1024x768(GFX_ColorDepth colorDepth)
{
	printf("GFX Display Configuration to 1024x768\n");
	u32	data;
	int		i , cnt=0;

	if(FB_Initialize() == 0)
		return(false);
	/*----------------------------------------------------------------------------------------------*/
	/* GFX PCI Configuration Space - Read and Configure												*/
	/*----------------------------------------------------------------------------------------------*/
	// Wait until GFX Device/Vendor ID is read correctly
	while (AHB2PCI_Config_Read(GFX_IDSEL + GFX_VENDOR_ID, sizeof(u32)) != GFX_VEN_DEV_ID_RV)
	{
		printf("Waiting until PCI is out of reset; \n");
		if (cnt == 10)
		{
			printf ("Test Aborted .\n");
			return (false);
		}
		mdelay (1000);
		cnt++;
	}
	printf("> Found correct GFX Device/Vendor ID 0x%08x. \n", GFX_VEN_DEV_ID_RV);
	// Get the MGA Registers base address (MGABASE1)
	AHB2PCI_Config_Write(GFX_IDSEL + GFX_BAR1, sizeof(u32), MGABASE1 = 0x10000000, 0);
	MGABASE1 = AHB2PCI_Config_Read(GFX_IDSEL + GFX_BAR1, sizeof(u32));
	if (MGABASE1 == 0)
	{
		printf("TEST_ConfigureDisplay failed : MGABASE1 is invalid = %08lxh\n", MGABASE1);
	}
	else
	{
		printf("TEST_ConfigureDisplay : MGABASE1 = %08lxh\n", MGABASE1);
	}
	AHB2PCI_Config_Write(GFX_IDSEL + GFX_BAR2, sizeof(u32), 0xf0000000, 0);
	printf("MGABASE1 = %08lxh, MGABASE2 = %08lxh\n",
		AHB2PCI_Config_Read(GFX_IDSEL + GFX_BAR1, sizeof(u32)),
		AHB2PCI_Config_Read(GFX_IDSEL + GFX_BAR2, sizeof(u32)));

	// Enable Bus Master and Memory Space
	data = AHB2PCI_Config_Read(GFX_IDSEL + GFX_COMMAND_REG, sizeof(u32));
	AHB2PCI_Config_Write(GFX_IDSEL + GFX_COMMAND_REG, sizeof(u32), data | 0x06, 0);

		/*----------------------------------------------------------------------------------------------*/
	/* Graphics PLL Configuration																	*/
	/*----------------------------------------------------------------------------------------------*/

	u8 _GPLLINDIV,_GPLLFBDIV,_GPLLST;

	if (GFX_IsPllDebug_g == 0)
	{ // the correct values
		_GPLLINDIV = 0x03; // _GPLLINDIV (INDV + FBDV8)
		_GPLLFBDIV = 0x4E; // _GPLLFBDIV (FBDV)
		_GPLLST    = 0x15; // _GPLLST (OTDV1/OTDV2)
	}
	else
	{ // debug Dell values. - no video issue
		_GPLLINDIV = 0x1A;  // _GPLLINDIV (INDV + FBDV8)
		_GPLLFBDIV = 0x40;  // _GPLLFBDIV (FBDV)
		_GPLLST    = 0x00;  // _GPLLST (OTDV1/OTDV2)
	}

	if(!PLL_Configure(_GPLLINDIV, _GPLLFBDIV, _GPLLST)) {return (false);}

	/*----------------------------------------------------------------------------------------------*/
	/* Sequencers Configuration																		*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0100)){return (false);}	// Reset sequencer
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0101)){return (false);}	// 8-dot character clock
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0302)){return (false);}	// Map 3 write enable
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0003)){return (false);}	// another mapping of VGA (??)
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0204)){return (false);}	// 256K memory is installed
	if (!AHB2PCI_Memory_Write(MISC, sizeof(u8), 0xC9)){return (false);}		// Configure CRTC addresses, Select MGA pixel clock, VSYNC and HSYNC active LOW
	if (!AHB2PCI_Memory_Write(SEQ, sizeof(u16), 0x0300)){return (false);}	// Reset sequencer

	printf("> Sequencers Configuration done.\n");

	/*----------------------------------------------------------------------------------------------*/
	/* Graphics Controller Configuration															*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0000)){return (false);}	// Reset bytes in the VGA memory map
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0001)){return (false);}	//  (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0002)){return (false);}	//  (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0003)){return (false);}	//  (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0004)){return (false);}	//  (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x1005)){return (false);}	// Selects Odd/Even addressing mode
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0E06)){return (false);}	// VGA mode select, Memory map is '0b11'
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0x0007)){return (false);}	//  (probably VGA - keep reset value)
	if (!AHB2PCI_Memory_Write(GCTL, sizeof(u16), 0xFF08)){return (false);}	// Write mask to 0xFF

	printf("> Graphics Controller Configuration done.\n");
	/*----------------------------------------------------------------------------------------------*/
	/* CRTC Controller Configuration																*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xA300)){return (false);} // HTOTAL[7:0] = 0xA3 (HTOTAL[8] appears in CRTCEXT1[0])
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x7F01)){return (false);} // HDISP_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x7F02)){return (false);} // HBLANK_START HBLKSTR[8] appears in CRTCEXT1[1]
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x8703)){return (false);} // HBLANK_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x8304)){return (false);} // HRETRACE_START
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x9305)){return (false);} // HRETRACE_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x2606)){return (false);} // VTOTAL		// was 24
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xF507)){return (false);} // OVERFLOW (overflow of other fields)
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x0008)){return (false);} // PRE_ROW_SCAN
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x6009)){return (false);} // MAX_SCAN_LINE
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x200A)){return (false);} // CUR_START
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x000B)){return (false);} // CUR_END

	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x000C)){return (false);} // ST_ADDR_HI
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x000D)){return (false);} // ST_ADDR_LOW
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xA300)){return (false);} // HTOTAL again?

	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x000E)){return (false);} // CUR_POS_HI
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x000F)){return (false);} // CUR_POS_LOW
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x0210)){return (false);} // VRETRACE_START
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x2811)){return (false);} // VRETRACE_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xFF12)){return (false);} // VDISP_END
	if (colorDepth == COLOR_DEPTH_32BIT) {
		if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x0013))
			return (false); // OFFSET
	} else {
		if (colorDepth == COLOR_DEPTH_16BIT)
		{
			if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x8013))
				return (false); // OFFSET
		}
	}
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x0014)){return (false);} // UND_LOC
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xFF15)){return (false);} // VBLANK_START
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x2516)){return (false);} // VBLANK_END
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xC317)){return (false);} // MODE_CTRL
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0xFF18)){return (false);} // LINE_COMP
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x0022)){return (false);} // CPU_LATCH_RD
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x8024)){return (false);} // DATA_SEL
	if (!AHB2PCI_Memory_Write(CRTC, sizeof(u16), 0x3426)){return (false);} // ATTRIB_ADDR (34 in diag 20 in Yarkon?)
	printf("> CRTC Configuration done.\n");	
	/*----------------------------------------------------------------------------------------------*/
	/* CRTC Controller Extension Configuration														*/
	/*----------------------------------------------------------------------------------------------*/

	if (colorDepth == COLOR_DEPTH_32BIT) {
		if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x1000))
			return (false); // ADDR_GEN
	} else {
		if ((colorDepth == COLOR_DEPTH_16BIT) || (colorDepth == COLOR_DEPTH_24BIT)) {
			if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0000))
				return (false); // ADDR_GEN
		}
	}

	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0801)){return (false);} // H_COUNT
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x8002)){return (false);} // V_COUNT

	if (colorDepth == COLOR_DEPTH_32BIT) {
		if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x8303))
			return (false); //MISC
	} else if (colorDepth == COLOR_DEPTH_16BIT) {
			if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x8103))
				return (false); // MISC
	} else if (colorDepth == COLOR_DEPTH_24BIT) {
			if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x8203))
				return (false); // MISC
	}

	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0004)){return (false);} // MEM_PAGE
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0005)){return (false);} // HVIDMID
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x2006)){return (false);} // P_REQ_CTL
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0007)){return (false);} // REQ_CTL

	// Poleg - New registers
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x1030)){return (false);} //MGABURSTSIZE
	if (!AHB2PCI_Memory_Write(CRTCEXT, sizeof(u16), 0x0534)){return (false);} //MGAREQCTL
	printf("> CRTC Extension configuration done.\n");
	/*----------------------------------------------------------------------------------------------*/
	/* DAC Eclipse Configuration																	*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x04)) {return (false);} // XCURADDL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x4F)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x05)) {return (false);} // XCURADDH
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x3A)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x06)) {return (false);} // XCURCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x08)) {return (false);} // XCURCOL0RED
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x82)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x09)) {return (false);} // XCURCOL0GREEN
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xFB)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x0A)) {return (false);} // XCURCOL0BLUE
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x44)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x0C)) {return (false);} // XCURCOL1RED
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x2C)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x0D)) {return (false);} // XCURCOL1GREE
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x75)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x0E)) {return (false);} // XCURCOL1BLUE
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x8B)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x10)) {return (false);} // XCURCOL2RED
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x14)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x11)) {return (false);} // XCURCOL2GREEN
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x2E)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x12)) {return (false);} // XCURCOL2BLUE
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x4A)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x19)) {return (false);} // XMULCTRL
	if (colorDepth == COLOR_DEPTH_32BIT) {
		if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x07))
			return (false);
	} else if (colorDepth == COLOR_DEPTH_16BIT) {
			if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x02))
				return (false);
	} else if (colorDepth == COLOR_DEPTH_24BIT) {
			if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x03))
				return (false);
	}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x1A)) {return (false);}		// XPIXCLKCTRL (should be 9?)
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x09)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x1D)) {return (false);}		// XGENCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x20)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x1E)) {return (false);}		// XMISCCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x1F)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2A)) {return (false);}		// XGENIOCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x01)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2B)) {return (false);}		// XGENIODATA
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x02)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2C)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x14)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2D)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x71)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2E)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x01)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x2F)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x40)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x38)) {return (false);}		// XZOOMCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x3A)) {return (false);}		// XSENSETEST (RO)
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x3C)) {return (false);}		// XCRCREML
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x3D)) {return (false);}		// XCRCREMH (RO)
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xB5)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x3E)) {return (false);}	// XCRCBITSEL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x0B)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x40)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xFD)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x41)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x42)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xB6)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x43)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x44)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x11)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x45)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x10)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x46)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x07)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x48)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x0C)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x49)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x75)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4A)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x07)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4C)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x13)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4D)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x5D)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4E)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x05)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x4F)) {return (false);}		// XPIXPLLSTAT
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x00)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x51)) {return (false);}		//  rsvd.
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x01)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x18)) {return (false);}	// XVREFCTRL
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x04)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x52)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x61)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x53)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x7D)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x54)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xD1)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x55)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0xFE)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x56)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x7D)){return (false);}
	if (!AHB2PCI_Memory_Write(PALWTADD,	sizeof(u8), 0x57)) {return (false);}
	if (!AHB2PCI_Memory_Write(X_DATAREG, sizeof(u8), 0x3A)){return (false);}

	printf("> DAC Eclipse Configuration done.\n");
	/*----------------------------------------------------------------------------------------------*/
	/* palette Configuration																		*/
	/*----------------------------------------------------------------------------------------------*/

	if (!AHB2PCI_Memory_Write(PALWTADD, sizeof(u8), 0)){return (false);}

	if ((colorDepth == COLOR_DEPTH_32BIT) || (colorDepth == COLOR_DEPTH_24BIT))
	{
		// palette for 8:8:8 color format
		for (i = 0; i <= 255; i++)
		{
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i)){return (false);}
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i)){return (false);}
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i)){return (false);}
		}
	} else if (colorDepth == COLOR_DEPTH_16BIT) {
		// palette for 5:6:5 color format
		for (i = 0; i <= 63; i++)
		{
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i * 8)){return (false);}
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i * 4)){return (false);}
			if (!AHB2PCI_Memory_Write(PALDATA, sizeof(u8), i * 8)){return (false);}
		}
	}

	printf("> Palette Configuration done.\n");
	/*----------------------------------------------------------------------------------------------*/
	/* Configuration Done																			*/
	/*----------------------------------------------------------------------------------------------*/

	// fill frame buffer with random lines
	printf("> Filling Frame Buffer...\n");

	// fill frame buffer with random lines
	u32 lineNum = 0;
	const u32 numOfBars=16;
	u32 Address = FB_BA;
	u32 Index;
	u32 Color;

	if (colorDepth == COLOR_DEPTH_32BIT)
		for (lineNum = 0; lineNum < numOfBars; ++lineNum)
		{
			Color = (u32)rand() ^ ((u32)rand()<<16);
			for (Index =0; Index<((1024*768)/numOfBars); Index++, Address=Address+4)
				writel(Color,Address);
		}
	else if (colorDepth == COLOR_DEPTH_16BIT)
		for (lineNum = 0; lineNum < numOfBars; ++lineNum)
		{
			Color = (u32)rand() ^ ((u32)rand()<<16);
			for (Index =0; Index<((1024*768)/numOfBars); Index++, Address=Address+2)
				(*((u16 *)(Address))) = ((u16) Color);
		}
	else if (colorDepth == COLOR_DEPTH_24BIT)
	{
		printf("\n Using 32 bit frame buffer method for 24bit mode ! \n");
		for (lineNum = 0; lineNum < numOfBars; ++lineNum)
		{
			Color = (u32)rand() ^ ((u32)rand()<<16);
			for (Index =0; Index<((1024*768)/numOfBars); Index++, Address=Address+4)
				writel(Color,Address);	
		}
	}
	printf("> Filling Frame Buffer done.\n");
	return (true);
}


int GFX_Draw_Pattern(int out_color)
{
	const u32 COLOR_DEPTH_MAPPING[8] = {8, 15, 16, 24, 0, 0, 0, 32};
	u32 color_format[4] = {0xFF0000 , 0xFF00 , 0xff , 0xFFFFFFFF};
	u32 color16bit_format[4] = {0xf800 , 0x07E0 , 0x001F , 0xFFFF};
	u32 hRes = 0, vRes = 0, colorDepth = 0 ,Address, Index, Color, lineNum=0;
	int cnt=0;
	u8 gmmapVal;
	const u32 numOfBars=16;

	/*----------------------------------------------------------------------------------------------*/
	/* GFX PCI Configuration Space - Read and Configure												*/
	/*----------------------------------------------------------------------------------------------*/
	// Wait until GFX Device/Vendor ID is read correctly
	while (AHB2PCI_Config_Read(GFX_IDSEL + GFX_VENDOR_ID, sizeof(u32)) != GFX_VEN_DEV_ID_RV)
	{
		printf("Waiting until PCI is out of reset; \n");
		if (cnt == 10)
		{
			printf ("Test Aborted .\n");
			return(false);
		}
		mdelay (100);
		cnt++;
	}
	printf("> Found correct GFX Device/Vendor ID 0x%08x. \n", GFX_VEN_DEV_ID_RV);

	if(FB_Initialize() == 0)
		return(false);

	hRes=( (((u8)(readl(HVCNTH)))<<8)  |  ((u8)(readl(HVCNTL))) )+1;
	vRes=( (((u8)(readl(VVCNTH)))<<8)  |  ((u8)(readl(VVCNTL))) );
	colorDepth=COLOR_DEPTH_MAPPING[((u8) readl(COLDEP))];
	
	if(hRes==0 || vRes==0 || colorDepth==0)
	{
		printf("can't draw pattern\n");
		printf("> Resolution: %lux%lux%lubpp\n", hRes, vRes, colorDepth);
		return(false);
	}

	Address = FB_BA;
	//4294967295 white  255 blue  65280 green 16711680 red
	printf("> Resolution: %lux%lux%lubpp  %lux \n", hRes, vRes, colorDepth,Address);

	Color=color_format[out_color];
	if (colorDepth == 32) {
		for (lineNum = 0; lineNum < numOfBars; ++lineNum)
		{
			//Color = //(DWORD)rand() ^ ((DWORD)rand()<<16);
			for (Index =0; Index<((hRes*vRes)/numOfBars); Index++, Address=Address+4)
				writel(Color,Address);
		}
		printf("draw pattern finish\n");
	} else if (colorDepth == 16) {
		Color=color16bit_format[out_color];
		for (lineNum = 0; lineNum < numOfBars; ++lineNum)
		{
			for (Index =0; Index<((hRes*vRes)/numOfBars); Index++, Address=Address+2) {
				(*((u16 *)(Address))) = ((u16) Color);
			}
		}
		printf("draw pattern finish\n");
	} else if (colorDepth == 24) {
		printf("\n Using 32 bit frame buffer method for 24bit mode ! \n");
		for (lineNum = 0; lineNum < numOfBars; ++lineNum)
		{
			for (Index =0; Index<((hRes*vRes)/numOfBars); Index++, Address=Address+4){
				writel(Color,Address);
			}
		}
		printf("draw pattern finish\n");
	}

	return (true);
}

void dumpGFX_InfoRegs (u32 *hRes, u32 *vRes, u32 *colorDepth, u32 *pll_freq)
{
	int i;
	const u32 COLOR_DEPTH_MAPPING[8] = {8, 15, 16, 24, 0, 0, 0, 32};
	u8 address;
	*hRes = 0; *vRes = 0; *colorDepth = 0; *pll_freq = 0;

	printf("Dump Graphics Core Information Registers (Read-Only)");
	for (i = 0; i < (NUM_OF_GFXI_REGS); ++i)
		printf("> %s = 0x%02X\n", GFXI_Regs[i].Name, ((u8) readl(GFXI_Regs[i].address)));

	// calculate pixel clock PLL:
	u32 FeedbackDivider;
	u8 InputDivider;
	u8 OutputDivider1;
	u8 OutputDivider2;

	FeedbackDivider=(((readl(GPLLST) & (0x1<<6))<<9) |  ((readl(GPLLINDI) & (0x1<<7))<<8) | ((u8) readl(GPLLFBDI)));
	InputDivider=(u8)(readl(GPLLINDI) & 0x1f);
	OutputDivider1=(u8)(readl(GPLLST) & 0x7);
	OutputDivider2=(u8)((readl(GPLLST) & (0x7<<3))>>3);

	if (FeedbackDivider && InputDivider && OutputDivider1 && OutputDivider2)
	{
		*pll_freq = (25 * (FeedbackDivider)) / ((u32) InputDivider * (u32) OutputDivider1 * (u32) OutputDivider2) ;
	}

	printf("> Calculated PLLG output: %lu[MHz] (rounded)\n", *pll_freq);

	*hRes=( (((u8)(readl(HVCNTH)))<<8)  |  ((u8)(readl(HVCNTL))) )+1;

	*vRes=( (((u8)(readl(VVCNTH)))<<8)  |  ((u8)(readl(VVCNTL))) );
	
	address=readl(COLDEP);
	*colorDepth=COLOR_DEPTH_MAPPING[address];
	printf("> Resolution: %lux%lux%lubpp\n", *hRes, *vRes, *colorDepth);

	//printf("> %x  %x \n", ((((u8) readl(BIOS_POST_BA+0x4C))&0x8)>>3) ,   (readl(BPCFMSTAT)) );
}

int Check_PWRGD_PS (void)
{
	if (((((u8) readl(BPCFMSTAT))&0x8)>>3) ==0)
	{
		printf(" PWRGD_PS signal is low. Set PWRGD_PS high and try again.");
		return (false);
	}
	printf("> PWRGD_PS signal is high.\n");
	return (true);
}

static int do_gfx_command(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int	command;
	int rc = 0, ret = 1;

	u32 hRes, vRes, colorDepth, pll_freq;

	if (argc < 2)
		return CMD_RET_USAGE;

	command=simple_strtoul(argv[1], NULL, 16);
	if(command==0 || command >15)
		return CMD_RET_USAGE;

	switch (command) {
	case 1:
			dumpGFX_InfoRegs(&hRes, &vRes, &colorDepth, &pll_freq);
			break;
	case 2:
			if (argc < 3)
				rc = CMD_RET_USAGE;
			if(!strcmp(argv[2], "on"))
			{
				if (Check_PWRGD_PS()!=true)
					break;
				//CLEAR_BIT_REG(INTCR3, 30);
				clrbits_le32(INTCR3,BIT(30));
				printf(">Reset scheme is not changed.\n");
			}
			else if(!strcmp(argv[2], "off"))
			{
				if (Check_PWRGD_PS()!=true)
					break;
				//SET_BIT_REG(INTCR3, 30);
				setbits_le32(INTCR3,BIT(30));
				printf("> Released internal nPCIRST. Reset is forced off. The input level of nPCIRST and the PCI Express reference clock doesn affect the operation of PCI devices.\n");
				break;
			}
			else
				rc = CMD_RET_USAGE;
			break;
	case 3:
			GFX_ConfigureDisplayTo1920x1200(COLOR_DEPTH_16BIT);
			dumpGFX_InfoRegs(&hRes, &vRes, &colorDepth, &pll_freq);
			if ((hRes!=1920) || (vRes!=1200) || (colorDepth!=16) || (pll_freq!=154))
				printf("Failed to configure GFX. Check register info.\n");
			else
				printf("GFX configuration succeed.\n");
			break;
	case 4:
			GFX_ConfigureDisplayTo1024x768(COLOR_DEPTH_16BIT);
			dumpGFX_InfoRegs(&hRes, &vRes, &colorDepth, &pll_freq);
			if ((hRes!=1024) || (vRes!=768) || (colorDepth!=16) || (pll_freq!=65))
				printf("Failed to configure GFX. Check register info.\n");
			else
				printf("GFX configuration succeed.\n");
			break;
	case 5:
			GFX_ConfigureDisplayTo1920x1200(COLOR_DEPTH_32BIT);
			dumpGFX_InfoRegs(&hRes, &vRes, &colorDepth, &pll_freq);
			if ((hRes!=1920) || (vRes!=1200) || (colorDepth!=32) || (pll_freq!=154))
				printf("Failed to configure GFX. Check register info.\n");
			else
				printf("GFX configuration succeed.\n");
			break;
	case 6:
			GFX_ConfigureDisplayTo1024x768(COLOR_DEPTH_32BIT);
			dumpGFX_InfoRegs(&hRes, &vRes, &colorDepth, &pll_freq);
			if ((hRes!=1024) || (vRes!=768) || (colorDepth!=32) || (pll_freq!=65))
				printf("Failed to configure GFX. Check register info.\n");
			else
				printf("GFX configuration succeed.\n");
			break;
	case 7:
			GFX_ClearMemorySpace(0);
			break;
	case 8:   //Toggle DACOFF bit
			printf("> INTCR = 0x%08lx \n", readl(INTCR));
			if ( (((readl(INTCR))&0x8000)>>15) !=1)
				setbits_le32(INTCR,BIT(15));
			else
				clrbits_le32(INTCR,BIT(15));
			printf("> After Set. INTCR = 0x%08lx \n", readl(INTCR));
			break;
	case 9:  //DVO
			setbits_le32(MFSEL3,BIT(21));   // set DVO DDR output MUXed on RGMII2
			setbits_le32(MFSEL1,BIT(20));
			setbits_le32(MFSEL1,BIT(19));
			clrbits_le32(MFSEL1,BIT(18));
			printf("> Enabled DVCK, DVVSYNC, DVHSYNC, DVD15-0 outputs (in DDR mode) on RGMII and TOCK pins; \n");
			break;
	case 10:  //DAC1
			if ((((readl(INTCR))&0x400)>>10) !=1)
			{
				printf("> Set DACOSOVR to 01b. DAC output is seclected by DACSEL bit. \n");
				setbits_le32(INTCR,BIT(10));
			}
			else
				printf("> DAC output is seclected by DACSEL bit. \n");

			if ( (((readl(INTCR))&0x4000)>>14) !=0)
			{
				printf("> Clear DACSEL bit. Select DAC1 output. \n");
				clrbits_le32(INTCR,BIT(14));
			}
			else
				printf("> DAC1 output is selected. \n");

			printf("> INTCR = 0x%08lx \n", readl(INTCR));
			break;
	case 11:  //DAC2
			if ( (((readl(MFSEL3))&0x400000)>>22) !=0)	
			{
				printf("> Clear GPOCSEL bit. Select VSYNC2, HSYNC2, DDC2SCL, DDC2SDA. \n");
				clrbits_le32(MFSEL3,BIT(22));
			}
			else
				printf("> VSYNC2, HSYNC2, DDC2SCL, DDC2SDA are selected. \n");

			if ( (((readl(INTCR))&0x400)>>10) !=1)
			{
				printf("> Set DACOSOVR to 01b. DAC output is seclected by DACSEL bit. \n");
				setbits_le32(INTCR,BIT(10));
			}
			else
				printf("> DAC output is seclected by DACSEL bit. \n");

			if ( (((readl(INTCR))&0x4000)>>14) !=1)
			{
				printf("> Set DACSEL bit. Select DAC2 output. \n");
				setbits_le32(INTCR,BIT(14));
			}
			else
				printf("> DAC2 output is selected. \n");

			printf("> INTCR = 0x%08lx \n", readl(INTCR));
			break;
	case 12:  //GFX IP reset
			setbits_le32(IPSRST2,BIT(10));
			printf(">IPSRST2  = 0x%08lx \n", readl(IPSRST2));
			mdelay (1000);
			clrbits_le32(IPSRST2,BIT(10));
			printf(">Reset graphics controller and PCIe to PCI bridge finish.\n");
			printf(">IPSRST2  = 0x%08lx \n", readl(IPSRST2));
			break;
	case 13:  //phy power down
			setbits_le32(INTCR2,BIT(0));
			printf(">Phy power down INTCR2= 0x%08lx \n", readl(INTCR2));
			break;
	case 14:  //phy power on
			clrbits_le32(INTCR2,BIT(0));
			printf(">Phy power down INTCR2= 0x%08lx \n", readl(INTCR2));
			break;
	case 15:
			if (argc < 3)
				rc = CMD_RET_USAGE;
			if(!strcmp(argv[2], "r"))
				ret = GFX_Draw_Pattern(0);
			else if(!strcmp(argv[2], "g"))
				ret = GFX_Draw_Pattern(1);
			else if(!strcmp(argv[2], "b"))
				ret = GFX_Draw_Pattern(2);
			else if(!strcmp(argv[2], "w"))
				ret = GFX_Draw_Pattern(3);
			else
				rc = CMD_RET_USAGE;
			if(ret==1)
				env_set("GFX_Draw_Pattern", "Pass");
			else
				env_set("GFX_Draw_Pattern", "Fail");
			env_save();
			break;
	default:
		    break;
	}
	return (rc);
}
/**************************************************/
U_BOOT_CMD(
	gfx ,	3,	1,	do_gfx_command,
	"gfx test",
	"\n1. Dump GFX info registers.\n"
	"2. PCI reset is forced off or on.\n"
	"3. Init GFX (using AHB2PCI bridge) to 1920x1200@16bit\n"
	"4. Init GFX (using AHB2PCI bridge) to 1024x768@16bit\n"
	"5. Init GFX (using AHB2PCI bridge) to 1920x1200@32bit\n"
	"6. Init GFX (using AHB2PCI bridge) to 1024x768@32bit\n"
	"7. Set GFX memory to 0\n"
	"8. Toggle DACOFF bit (DAC is ON).\n"
	"9. Enable DVO DDR pins.\n"
	"A. Select DAC1.\n"
	"B. Select DAC2.\n"
	"C. GFX IP reset.\n"
	"D. PCIE Phy power down.\n"
	"E. PCIE Phy power on.\n"
	"F. Draw Pattern without set resolution.\n"
	"gfx [2] [on/off]\n"
	"gfx [f] [r,g,b,w]\n"
);