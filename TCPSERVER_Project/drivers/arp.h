/*********************************************************************
 *
 *                  ARP Module Defs for Microchip TCP/IP Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        ARP.h
 * Dependencies:    Stacktsk.h
 *                  MAC.h
 * Processor:       PIC18
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
 * Author               Date     Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Nilesh Rajbharti     5/1/01   Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02   Cleanup
 * Nilesh Rajbharti     5/22/02  Rev 2.0 (See version.log for detail)
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 ********************************************************************/
#ifndef ARP_H
#define ARP_H

#include "drivers\stacktsk.h"
#include "drivers\mac.h"

/*
 * ARP packet
 */
typedef struct _ARP_PACKET
{
    int16        HardwareType;
    int16        Protocol;
    int8        MACAddrLen;
    int8        ProtocolLen;
    int16        Operation;
    MAC_ADDR    SenderMACAddr;
    IP_ADDR     SenderIPAddr;
    MAC_ADDR    TargetMACAddr;
    IP_ADDR     TargetIPAddr;
} ARP_PACKET;

void SwapARPPacket(ARP_PACKET *p);


/*
 * Following codes are must be used with ARPGet/Put functions.
 */
#define ARP_REPLY       0x00
#define ARP_REQUEST     0x01
#define ARP_UNKNOWN     0x02


/*********************************************************************
 * Function:        BOOL ARPGet(NODE_INFO* remote, BYTE* opCode)
 *
 * PreCondition:    ARP packet is ready in MAC buffer.
 *
 * Input:           remote  - Remote node info
 *                  opCode  - Buffer to hold ARP op code.
 *
 * Output:          TRUE if a valid ARP packet was received.
 *                  FALSE otherwise.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int1    ARPGet(NODE_INFO *remote, int8 *opCode);


/*********************************************************************
 * Macro:           ARPIsRxReady()
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          TRUE if ARP receive buffer is full.
 *                  FALSE otherwise.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
#define ARPIsTxReady()      MACIsTxReady()



/*********************************************************************
 * Function:        void ARPPut(NODE_INFO* more, BYTE opCode)
 *
 * PreCondition:    MACIsTxReady() == TRUE
 *
 * Input:           remote  - Remote node info
 *                  opCode  - ARP op code to send
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void ARPPut(NODE_INFO *remote, int8 opCode);

#endif


