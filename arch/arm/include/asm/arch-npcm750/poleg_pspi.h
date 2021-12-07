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

#ifndef __POLEG_PSPI_H_
#define __POLEG_PSPI_H_


/* PSPI_CTL1 fields */
#define PSPI_CTL1_SCDV6_0	9
#define PSPI_CTL1_SCIDL		8
#define PSPI_CTL1_SCM		7
#define PSPI_CTL1_EIW		6
#define PSPI_CTL1_EIR		5
#define PSPI_CTL1_SPIEN		0

/* PSPI_STAT fields */
#define PSPI_STAT_RBF		1
#define PSPI_STAT_BSY		0

#endif
