/*********************************************************************
 *
 *                  Announce Module Header
 *
 *********************************************************************
 * FileName:        announce.h
 * Dependencies:    None
 * Processor:       PIC18
 * Complier:        MCC18 v1.00.50 or higher
 *                  HITECH PICC-18 V8.10PL1 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * This software is owned by Microchip Technology Inc. ("Microchip")
 * and is supplied to you for use exclusively as described in the
 * associated software agreement.  This software is protected by
 * software and other intellectual property laws.  Any use in
 * violation of the software license may subject the user to criminal
 * sanctions as well as civil liability.  Copyright 2006 Microchip
 * Technology Inc.  All rights reserved.
 *
 * This software is provided "AS IS."  MICROCHIP DISCLAIMS ALL
 * WARRANTIES, EXPRESS, IMPLIED, STATUTORY OR OTHERWISE, NOT LIMITED
 * TO MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * INFRINGEMENT.  Microchip shall in no event be liable for special,
 * incidental, or consequential damages.
 *
 *
 * Author               Date    Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Howard Schlunder     10/7/04  Original
 * Darren Rook (CCS)    07/13/06 In synch with Microchip's V3.02 stack
 * Darren Rook (CCS)    08/15/06 AnnounceIP() returns TRUE if success
 * Darren Rook (CCS)    10/24/06 In synch with Microchips's V3.75 stack 
 ********************************************************************/
#ifndef ANNONCE_H
#define ANNONCE_H

int	AnnounceIP(void);
void	DiscoveryTask(void);

#endif
