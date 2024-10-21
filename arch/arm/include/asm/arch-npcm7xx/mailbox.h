/*----------------------------------------------------------------------------*/
/* SPDX-License-Identifier: GPL-2.0                                           */
/*                                                                            */
/* Copyright (c) 2010-2019 by Nuvoton Technology Corporation                  */
/* All rights reserved                                                        */
/*                                                                            */
/*----------------------------------------------------------------------------*/
/* File Contents:                                                             */
/*   mailbox.h                                                                */
/*            This file contains API of routines for handling the PCI MailBox */
/*  Project:                                                                  */
/*            Poleg Bootblock and ROM Code (shared header)                    */
/*----------------------------------------------------------------------------*/

#ifndef _MAILBOX_H
#define _MAILBOX_H

#include "shared_defs.h"

#define ROM_STATUS_MSG_ADDR                          0xF084BFE8
#define BOOTBLK_STATUS_MSG_ADDR                      0xF084BFD0

/*---------------------------------------------------------------------------------------------------------*/
/* Image states                                                                                            */
/*---------------------------------------------------------------------------------------------------------*/
#define IMAGE_NOT_TESTED                             0x00
#define IMAGE_WRONG_START_TAG                        0x01
#define IMAGE_DEST_ADDRESS_UNALIGNED                 0x02
#define IMAGE_BAD_SIGNATURE                          0x04
#define IMAGE_MEMORY_OVERLAP                         0x08
#define IMAGE_HEADER_OK_COPY_IMAGE                   0x10
#define IMAGE_OK_RUN_FROM_FLASH                      0x40
#define IMAGE_OK                                     0x80
#define IMAGE_REJECTED_BY_BB                         0x11
#define IMAGE_NEW_COPY                               0x12
#define IMAGE_NOT_IN_USE                             0xFF    /* image is not selected - depends on FUSE_FUSTRAP_oAltImgLoc */

/*---------------------------------------------------------------------------------------------------------*/
/* Rom status                                                                                              */
/*---------------------------------------------------------------------------------------------------------*/
#define ST_ROM_BASIC_USE_IMAGE_SPI0_CS0_OFFSET0      0x21    // Select image at SPI0 CS0 offset 0	   ( address 0x80000000)
#define ST_ROM_BASIC_USE_IMAGE_SPI0_CS0_OFFSET80000  0x22    // Select image at SPI0 CS0 offset 8000   ( address 0x80080000)
#define ST_ROM_BASIC_USE_IMAGE_SPI0_CS1_OFFSET0      0x23    // Select image at SPI0 CS1 offset 0	   ( address 0x88000000)

#define ST_ROM_USE_KEY0_IMAGE0                       0x27    // checking image 0, select Pk0 according to fuses for signature varification.
#define ST_ROM_USE_KEY1_IMAGE0                       0x28    // checking image 0, select Pk1 according to fuses for signature varification.
#define ST_ROM_USE_KEY2_IMAGE0                       0x29    // checking image 0, select Pk2 according to fuses for signature varification.

#define ST_ROM_USE_KEY0_IMAGE1                       0x2A    // checking image 1, select Pk0 according to fuses for signature varification.
#define ST_ROM_USE_KEY1_IMAGE1                       0x2B    // checking image 1, select Pk1 according to fuses for signature varification.
#define ST_ROM_USE_KEY2_IMAGE1                       0x2C    // checking image 1, select Pk2 according to fuses for signature varification.

#define ST_ROM_USE_KEY0_IMAGE2                       0x2D    // checking image 2, select Pk0 according to fuses for signature varification.
#define ST_ROM_USE_KEY1_IMAGE2                       0x2E    // checking image 2, select Pk1 according to fuses for signature varification.
#define ST_ROM_USE_KEY2_IMAGE2                       0x2F    // checking image 2, select Pk2 according to fuses for signature varification.

/*---------------------------------------------------------------------------------------------------------*/
/* MailBox module internal structure definitions                                                           */
/*---------------------------------------------------------------------------------------------------------*/
#pragma pack(1)
typedef struct _ROM_STATUS_MSG
{
	union
	{
		struct
		{
			u8  image0State;
			u8  image1State;
			u8  image2state;

			union
			{
				struct
				{
					u8  image0_pk0;
					u8  image0_pk1;
					u8  image0_pk2;
					u8  image1_pk0;
					u8  image1_pk1;
					u8  image1_pk2;
					u8  image2_pk0;
					u8  image2_pk1;
					u8  image2_pk2;
				} pk_states;
				u8 pk_bytes[9];
			} pk;
		} state ;
		u8 bytes[12];
	} imageState ;

	u8   startTag[START_TAG_SIZE];
	u32  status;

} ROM_STATUS_MSG;
#pragma pack()

#endif

