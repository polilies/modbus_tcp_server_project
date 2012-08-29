/*********************************************************************
 *
 *                  MAC Module Defs for Microchip Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        MAC.h
 * Dependencies:    StackTsk.h
 * Processor:       PIC18C
 * Complier:        CCS PCH 3.181 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the “Company”) for its PICmicro® Microcontroller is intended and
 * supplied to you, the Company’s customer, for use solely and
 * exclusively on Microchip PICmicro Microcontroller products. The
 * software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 * Author               Date      Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Nilesh Rajbharti     4/27/01   Original        (Rev 1.0)
 * Nilesh Rajbharti     11/27/01  Added SLIP
 * Nilesh Rajbharti     2/9/02    Cleanup
 * Nilesh Rajbharti     5/22/02   Rev 2.0 (See version.log for detail)
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 ********************************************************************/

#ifndef MAC_H
#define MAC_H

#include "drivers\stacktsk.h"

#define MAC_IP      0
#define MAC_ARP     0x6
#define MAC_UNKNOWN 0x0ff

#define INVALID_BUFFER  0xff

void    MACInit(void);
int1    MACIsTxReady(void);
int1    MACIsLinked(void);
int1    MACGetHeader(MAC_ADDR *remote, int8* type);
int8    MACGet(void);
int16    MACGetArray(int8 *val, int16 len);
void    MACDiscardRx(void);

void    MACPutHeader(MAC_ADDR *remote,
                     int8 type,
                     int16 dataLen);
void    MACPut(int8 val);
void    MACPutArray(int8 *val, int16 len);
void    MACFlush(void);
void    MACDiscardTx(BUFFER buff);

void    MACSetRxBuffer(int16 offset);
void    MACSetTxBuffer(BUFFER buff, int16 offset);
void    MACReserveTxBuffer(BUFFER buff);

int16    MACGetFreeRxSize(void);
int16 MACGetOffset(void);


#define MACGetRxBuffer()        (NICCurrentRdPtr)
#define MACGetTxBuffer()        (NICCurrentTxBuffer)

#if     !STACK_USE_SLIP
int8 NICReadPtr;                // Next page that will be used by NIC to load new packet.
int8 NICCurrentRdPtr;           // Page that is being read...
int8 NICCurrentTxBuffer;
#else
#define NICCurrentTxBuffer      (0)
#define NICCurrentRdPtr         (0)
#endif



#endif
