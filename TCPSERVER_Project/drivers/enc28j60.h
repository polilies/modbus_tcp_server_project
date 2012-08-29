/*********************************************************************
 *
 *                  MAC Module Defs for Microchip Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *                 (Modified to work with CCS's ENC28J60 driver)
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

#define MAC_IP      0
#define MAC_ARP     0x6
#define MAC_UNKNOWN 0x0ff

#define ETHER_IP        0x00
#define ETHER_ARP       0x06

#define INVALID_BUFFER  0xff

void    MACInit(void);
int NICIsTransmitting(void);
int NICIsLinked(void);
int MACGetHeader(MAC_ADDR *remote, int8 *type);
int8 NICGet(void);
int16 NICGetArray(char *ptr, int16 size);
void NICDiscardRx(void);
void MACPutHeader(MAC_ADDR *remote, int8 type, int16 len);
void NICPut(char c);
void enc_mac_write_bytes(int8 oa, int8 *ptr, int16 len);  //write command (oa) then bytes to SPI
void MACFlush(void);
void NICDiscardTx(int8 buffer);
void NICSetRxBuffer(int16 offset);
void __NICSetTxBuffer(int8 buffer, int16 offset);
void NICReserveTxBuffer(int8 buffer);
int16 NICGetFreeRxSize(void);

#define NICPutArray(ptr,size)  enc_mac_write_bytes(0x7A, ptr, size)
#define NICSetTxBuffer(b,o) __NICSetTxBuffer(b,o+1)

#define MACGetFreeRxSize   NICGetFreeRxSize
#define MACReserveTxBuffer NICReserveTxBuffer
#define MACSetTxBuffer(b,a)  NICSetTxBuffer(b,a+sizeof(ETHERNET_HEADER))
#define MACSetRxBuffer(a)  NICSetRxBuffer(a+sizeof(ETHERNET_HEADER))
#define MACDiscardTx    NICDiscardTx
#define MACPutArray     NICPutArray
#define MACPut          NICPut
#define MACDiscardRx    NICDiscardRx
#define MACGetArray     NICGetArray
#define MACGet          NICGet
#define MACIsTxReady    !NICIsTransmitting
#define MACIsLinked     NICIsLinked


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
