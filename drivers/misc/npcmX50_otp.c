/*
 * NUVOTON Poleg OTP driver
 *
 * Copyright (C) 2019, NUVOTON, Incorporated
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fuse.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/otp.h>
#include <clk.h>
#include <linux/delay.h>

struct npcmX50_otp_priv {
#if defined (CONFIG_ARCH_NPCM8XX)
	struct npcm_otp_regs *regs[1];
#else
	struct npcm_otp_regs *regs[2];
#endif
};

static struct npcmX50_otp_priv *otp_priv;

/*----------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_check_inputs                                  */
/*                                                                            */
/* Parameters:      arr - fuse array number to check                          */
/*                  word - fuse word (offset) to chcek                        */
/* Returns:         int                                                       */
/* Side effects:                                                              */
/* Description:     Checks is arr and word are illegal and do not exceed      */
/*                  their range. Return 0 if they are legal, -1 if not        */
/*----------------------------------------------------------------------------*/
static int npcmX50_otp_check_inputs(npcm_otp_storage_array arr, u32 word)
{

	if (arr >= NPCMX50_NUM_OF_SA) {
#if defined (CONFIG_ARCH_NPCM8XX)
		printf("\nError: npcm8XX otp includs only one bank: 0\n");
#else
		printf("\nError: npcm7XX otp includs only two banks: 0 and 1\n");
#endif
		return -1;
	}

	if (word >= NPCMX50_OTP_ARR_BYTE_SIZE) {
		printf("\nError: npcmX50 otp array comprises only %d bytes, numbered from 0 to %d\n",NPCMX50_OTP_ARR_BYTE_SIZE,NPCMX50_OTP_ARR_BYTE_SIZE-1);
		return -1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_wait_for_otp_ready                            */
/*                                                                            */
/* Parameters:      array - fuse array to wait for                            */
/* Returns:         int                                                       */
/* Side effects:                                                              */
/* Description:     Initialize the Fuse HW module.                            */
/*----------------------------------------------------------------------------*/
static int npcmX50_otp_wait_for_otp_ready(npcm_otp_storage_array arr,
										  u32 timeout)
{
	struct npcm_otp_regs *regs = otp_priv->regs[arr];
	volatile u32 time = timeout;

	/*------------------------------------------------------------------------*/
	/* check parameters validity                                              */
	/*------------------------------------------------------------------------*/
	if (arr > NPCMX50_FUSE_SA)
		return -EINVAL;

	while (--time > 1) {
		if (readl(&regs->fst) & FST_RDY) {
			/* fuse is ready, clear the status. */
			writel(readl(&regs->fst) | FST_RDST, &regs->fst);
			return 0;
		}
	}

	/* try to clear the status in case it was set */
	writel(readl(&regs->fst) | FST_RDST, &regs->fst);

	return -EINVAL;
}

/*----------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_read_byte                                     */
/*                                                                            */
/* Parameters:      arr  - Storage Array type [input].                        */
/*                  addr - Byte-address to read from [input].                 */
/*                  data - Pointer to result [output].                        */
/* Returns:         none                                                      */
/* Side effects:                                                              */
/* Description:     Read 8-bit data from an OTP storage array.                */
/*----------------------------------------------------------------------------*/
static void npcmX50_otp_read_byte(npcm_otp_storage_array arr, u32 addr,
								  u8 *data)
{
	struct npcm_otp_regs *regs = otp_priv->regs[arr];

	/* Wait for the Fuse Box Idle */
	npcmX50_otp_wait_for_otp_ready(arr, 0xDEADBEEF);

	/* Configure the byte address in the fuse array for read operation */
	writel(FADDR_VAL(addr, 0), &regs->faddr);

	/* Initiate a read cycle */
	writel(READ_INIT, &regs->fctl);

	/* Wait for read operation completion */
	npcmX50_otp_wait_for_otp_ready(arr, 0xDEADBEEF);

	/* Read the result */
	*data = readl(&regs->fdata) & FDATA_MASK;

	/* Clean FDATA contents to prevent unauthorized software from reading
	   sensitive information */
	writel(FDATA_CLEAN_VALUE, &regs->fdata);
}

/*----------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_bit_is_programmed                             */
/*                                                                            */
/* Parameters:      arr     - Storage Array type [input].                     */
/*                  byteNum - Byte offset in array [input].                   */
/*                  bitNum  - Bit offset in byte [input].                     */
/* Returns:         Nonzero if bit is programmed, zero otherwise.             */
/* Side effects:                                                              */
/* Description:     Check if a bit is programmed in an OTP storage array.     */
/*----------------------------------------------------------------------------*/
static bool npcmX50_otp_bit_is_programmed(npcm_otp_storage_array  arr,
										  u32 byteNum, u8 bitNum)
{
	u32 data = 0;

	/* Read the entire byte you wish to program */
	npcmX50_otp_read_byte(arr, byteNum, (u8*)&data);

	/* Check whether the bit is already programmed */
	if (data & (1 << bitNum))
		return true;

	return false;
}

/*----------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_program_bit                                   */
/*                                                                            */
/* Parameters:      arr     - Storage Array type [input].                     */
/*                  byteNum - Byte offset in array [input].                   */
/*                  bitNum  - Bit offset in byte [input].                     */
/* Returns:         int                                                       */
/* Side effects:                                                              */
/* Description:     Program (set to 1) a bit in an OTP storage array.         */
/*----------------------------------------------------------------------------*/
static int npcmX50_otp_program_bit(npcm_otp_storage_array arr, u32 byteNum,
								   u8 bitNum)
{
	struct npcm_otp_regs *regs = otp_priv->regs[arr];
	int count;
	u8 read_data;

	/* Wait for the Fuse Box Idle */
	npcmX50_otp_wait_for_otp_ready(arr, 0xDEADBEEF);

	/* Make sure the bit is not already programmed */
	if (npcmX50_otp_bit_is_programmed(arr, byteNum, bitNum))
		return 0;

	/* Configure the bit address in the fuse array for program operation */
	writel(FADDR_VAL(byteNum, bitNum), &regs->faddr);
	writel(readl(&regs->faddr) | FADDR_IN_PROG, &regs->faddr);

	// program up to MAX_PROGRAM_PULSES
	for (count = 1; count <= MAX_PROGRAM_PULSES; count++) {
		/* Initiate a program cycle */
		writel(PROGRAM_ARM, &regs->fctl);
		writel(PROGRAM_INIT, &regs->fctl);

		/* Wait for program operation completion */
		npcmX50_otp_wait_for_otp_ready(arr, 0xDEADBEEF);

		// after MIN_PROGRAM_PULSES start verifying the result
		if (count >= MIN_PROGRAM_PULSES) {
			/* Initiate a read cycle */
			writel(READ_INIT, &regs->fctl);

			/* Wait for read operation completion */
			npcmX50_otp_wait_for_otp_ready(arr, 0xDEADBEEF);

			/* Read the result */
			read_data = readl(&regs->fdata) & FDATA_MASK;

			/* If the bit is set the sequence ended correctly */
			if (read_data & (1 << bitNum))
				break;
		}
	}

	// check if programmking failed
	if (count > MAX_PROGRAM_PULSES)
	{
		printf("program fail\n");
		return -EINVAL;
	}

	/* Clean FDATA contents to prevent unauthorized software from reading
	   sensitive information
	*/
	writel(FDATA_CLEAN_VALUE, &regs->fdata);

	return 0;
}

/*----------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_ProgramByte                                   */
/*                                                                            */
/* Parameters:      arr     - Storage Array type [input].                     */
/*                  byteNum - Byte offset in array [input].                   */
/*                  value   - Byte to program [input].                        */
/* Returns:         int                                                       */
/* Side effects:                                                              */
/* Description:     Program (set to 1) a given byte's relevant bits in an     */
/*                  OTP storage array.                                        */
/*----------------------------------------------------------------------------*/
static int npcmX50_otp_ProgramByte(npcm_otp_storage_array arr, u32 byteNum,
								   u8 value)
{
	int status = 0;
	unsigned int i;
	u8 data = 0;
	int rc;

	rc = npcmX50_otp_check_inputs(arr, byteNum);
	if (rc != 0)
		return rc;

	/* Wait for the Fuse Box Idle */
	npcmX50_otp_wait_for_otp_ready(arr, 0xDEADBEEF);

	/* Read the entire byte you wish to program */
	npcmX50_otp_read_byte(arr, byteNum, &data);

	/* In case all relevant bits are already programmed - nothing to do */
	if ((~data & value) == 0)
		return status;

	/* Program unprogrammed bits. */
	for (i = 0; i < 8; i++) {
		if (value & (1 << i)) {
			/* Program (set to 1) the relevant bit */
			int last_status = npcmX50_otp_program_bit(arr, byteNum, (u8)i);

			if (last_status != 0)
				status = last_status;
		}
	}
	return status;
}

/*----------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_is_fuse_array_disabled                        */
/*                                                                            */
/* Parameters:      arr - Storage Array type [input].                         */
/* Returns:         bool                                                      */
/* Side effects:                                                              */
/* Description:     Return true if access to the first 2048 bits of the       */
/*                  specified fuse array is disabled, false if not            */
/*----------------------------------------------------------------------------*/
bool npcmX50_otp_is_fuse_array_disabled(npcm_otp_storage_array arr)
{
	struct npcm_otp_regs *regs = otp_priv->regs[arr];

	return (readl(&regs->fcfg) & FCFG_FDIS) != 0;
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_select_key                                                                 */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*                  key_index - AES key index in the key array (in 128-bit steps) [input]                  */
/* Returns:         int                                                                                    */
/* Side effects:                                                                                           */
/* Description:     Returns 0 on successful selection, -1 otherwise                                        */
/*---------------------------------------------------------------------------------------------------------*/
int npcmX50_otp_select_key(u8 key_index)
{
	struct npcm_otp_regs *regs = otp_priv->regs[NPCMX50_KEY_SA];
	u32 fKeyInd = 0;
	volatile u32 time = 0xDAEDBEEF;

	if (key_index >= 4)
        return -1;

	/* Do not destroy ECCDIS bit */
	fKeyInd = readl(&regs->fustrap_fkeyind);

	/* Configure the key size */
	fKeyInd &= ~FKEYIND_KSIZE_MASK;
	fKeyInd |= FKEYIND_KSIZE_256;

	/* Configure the key index (0 to 3) */
	fKeyInd &= ~FKEYIND_KIND_MASK;
	fKeyInd |= FKEYIND_KIND_KEY(key_index);

	writel(fKeyInd, &regs->fustrap_fkeyind);

	/*-----------------------------------------------------------------------------------------------------*/
	/* Wait for selection completetion                                                                     */
	/*-----------------------------------------------------------------------------------------------------*/
	while (--time > 1) {
        if (readl(&regs->fustrap_fkeyind) & FKEYIND_KVAL)
            return 0;

		udelay(1);
	}

	return -1;
}

/*----------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_nibble_parity_ecc_encode                      */
/*                                                                            */
/* Parameters:      datain - pointer to decoded data buffer                   */
/*                  dataout - pointer to encoded data buffer (buffer size     */
/*                            should be 2 x dataout)                          */
/*                  size - size of encoded data (decoded data x 2)            */
/* Returns:         none                                                      */
/* Side effects:                                                              */
/* Description:     Decodes the data according to nibble parity ECC scheme.   */
/*                  Size specifies the encoded data size.                     */
/*                  Decodes whole bytes only                                  */
/*----------------------------------------------------------------------------*/
void npcmX50_otp_nibble_parity_ecc_encode(u8 *datain, u8 *dataout, u32 size)
{
	u32 i;
	u8 E0, E1, E2, E3;

	for (i = 0; i < (size / 2); i++) {

		E0 = (datain[i] >> 0) & 0x01;
		E1 = (datain[i] >> 1) & 0x01;
		E2 = (datain[i] >> 2) & 0x01;
		E3 = (datain[i] >> 3) & 0x01;

		dataout[i*2] = datain[i] & 0x0f;
		dataout[i*2] |= (E0 ^ E1) << 4;
		dataout[i*2] |= (E2 ^ E3) << 5;
		dataout[i*2] |= (E0 ^ E2) << 6;
		dataout[i*2] |= (E1 ^ E3) << 7;

		E0 = (datain[i] >> 4) & 0x01;
		E1 = (datain[i] >> 5) & 0x01;
		E2 = (datain[i] >> 6) & 0x01;
		E3 = (datain[i] >> 7) & 0x01;

		dataout[i*2+1] = (datain[i] & 0xf0) >> 4;
		dataout[i*2+1] |= (E0 ^ E1) << 4;
		dataout[i*2+1] |= (E2 ^ E3) << 5;
		dataout[i*2+1] |= (E0 ^ E2) << 6;
		dataout[i*2+1] |= (E1 ^ E3) << 7;
	}
}

/*----------------------------------------------------------------------------*/
/* Function:        npcmX50_otp_majority_rule_ecc_encode                      */
/*                                                                            */
/* Parameters:      datain - pointer to decoded data buffer                   */
/*                  dataout - pointer to encoded data buffer (buffer size     */
/*                            should be 3 x dataout)                          */
/*                  size - size of encoded data (decoded data x 3)            */
/* Returns:         none                                                      */
/* Side effects:                                                              */
/* Description:     Decodes the data according to Major Rule ECC scheme.      */
/*                  Size specifies the encoded data size.                     */
/*                  Decodes whole bytes only                                  */
/*----------------------------------------------------------------------------*/
void npcmX50_otp_majority_rule_ecc_encode(u8 *datain, u8 *dataout, u32 size)
{
	u32 byte;
	u32 bit;
	u8 bit_val;
	u32 decoded_size = size / 3;

	for (byte = 0; byte < decoded_size; byte++) {
		for (bit = 0; bit < 8; bit++) {

			bit_val = (datain[byte] >> bit) & 0x01;

			if (bit_val) {
				dataout[decoded_size*0+byte] |= (1 << bit);
				dataout[decoded_size*1+byte] |= (1 << bit);
				dataout[decoded_size*2+byte] |= (1 << bit);
			} else {
				dataout[decoded_size*0+byte] &= ~(1 << bit);
				dataout[decoded_size*1+byte] &= ~(1 << bit);
				dataout[decoded_size*2+byte] &= ~(1 << bit);
			}
		}
	}
}

/*----------------------------------------------------------------------------*/
/* Function:        fuse_program_data                                         */
/*                                                                            */
/* Parameters:      bank - Storage Array type [input].                        */
/*                  word - Byte offset in array [input].                      */
/*                  data - Pointer to data buffer to program.                 */
/*                  size - Number of bytes to program.                        */
/* Returns:         none                                                      */
/* Side effects:                                                              */
/* Description:     Programs the given byte array (size bytes) to the given   */
/*                  OTP storage array, starting from offset word.             */
/*----------------------------------------------------------------------------*/
int fuse_program_data(u32 bank, u32 word, u8 *data, u32 size)
{
	npcm_otp_storage_array arr = (npcm_otp_storage_array)bank;
	u32 byte;
	int rc;

	rc = npcmX50_otp_check_inputs(bank, word + size - 1);
	if (rc != 0)
		return rc;

	for (byte = 0; byte < size; byte++) {
		u8 val;

		val = data[byte];
		if (val == 0) // optimization
			continue;

		rc = npcmX50_otp_ProgramByte(arr, word + byte, data[byte]);
		if (rc != 0)
			return rc;

		// verify programming of every '1' bit
		val = 0;
		npcmX50_otp_read_byte((npcm_otp_storage_array)bank, byte, &val);
		if ((data[byte] & ~val) != 0)
			return -1;
	}

	return 0;
}

int fuse_prog_image(u32 bank, u32 address)
{
	return fuse_program_data(bank, 0, (u8*)(uintptr_t)address, NPCMX50_OTP_ARR_BYTE_SIZE);
}

int fuse_read(u32 bank, u32 word, u32 *val)
{
	int rc = npcmX50_otp_check_inputs(bank, word);
	if (rc != 0)
		return rc;

	*val = 0;
	npcmX50_otp_read_byte((npcm_otp_storage_array)bank, word, (u8*)val);

	return 0;
}

int fuse_sense(u32 bank, u32 word, u32 *val)
{
	/* We do not support overriding */
	return -EINVAL;
}

int fuse_prog(u32 bank, u32 word, u32 val)
{
	int rc;

	rc = npcmX50_otp_check_inputs(bank, word);
	if (rc != 0)
		return rc;

	return npcmX50_otp_ProgramByte((npcm_otp_storage_array)bank, word,
								   (u8)val);
}

int fuse_override(u32 bank, u32 word, u32 val)
{
	/* We do not support overriding */
	return -EINVAL;
}

static int npcmX50_otp_bind(struct udevice *dev)
{
	struct npcm_otp_regs *pRegs;

	otp_priv = calloc(1, sizeof(struct npcmX50_otp_priv));
	if (!otp_priv)
		return -ENOMEM;

	pRegs = NULL;
	pRegs = dev_remap_addr_index(dev, 0);
	if (!pRegs) {
		printf("Cannot find reg address (arr #0), binding failed\n");
		return -EINVAL;
	}
	otp_priv->regs[0] = pRegs;
#if defined (CONFIG_TARGET_POLEG)
	pRegs = NULL;
	pRegs = dev_remap_addr_index(dev, 1);
	if (!pRegs) {
		printf("Cannot find reg address (arr #1), binding failed\n");
		return -EINVAL;
	}
	otp_priv->regs[1] = pRegs;
#endif
	printk(KERN_INFO "OTP: NPCMX50 OTP module bind OK\n");

	return 0;
}

static const struct udevice_id npcmX50_otp_ids[] = {
	{ .compatible = "nuvoton,npcmX50-otp" },
	{ }
};

U_BOOT_DRIVER(npcmX50_otp) = {
	.name = "npcmX50_otp",
	.id = UCLASS_MISC,
	.of_match = npcmX50_otp_ids,
	.priv_auto = sizeof(struct npcmX50_otp_priv),
	.bind = npcmX50_otp_bind,
};
