/*********************************************************************
 *
 *                  ICMP Module for Microchip TCP/IP Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        ICMP.C
 * Dependencies:    ICMP.h
 *                  string.h
 *                  StackTsk.h
 *                  Helpers.h
 *                  IP.h
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
 * Nilesh Rajbharti     4/30/01  Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02   Cleanup
 * Nilesh Rajbharti     5/22/02  Rev 2.0 (See version.log for detail)
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 * Darren Rook (CCS)    06/29/04 SwapICMPPacket() no longer static
 ********************************************************************/

#include <string.h>
#include "drivers\stacktsk.h"
#include "drivers\helpers.h"
#include "drivers\icmp.h"
#include "drivers\ip.h"

//hahaha, c18 and hitech sucks
/*
#if !defined(STACK_USE_ICMP)
#error You have selected to not include ICMP.  Remove this file from \
       your project to reduce code size.
#endif
*/

#define MAX_ICMP_DATA       32

/*
 * ICMP packet definition
 */
typedef struct _ICMP_PACKET
{
    int8    Type;
    int8    Code;
    int16    Checksum;
    int16    Identifier;
    int16    SequenceNumber;
    int8    Data[MAX_ICMP_DATA];
} ICMP_PACKET;
#define ICMP_HEADER_SIZE    (sizeof(ICMP_PACKET) - MAX_ICMP_DATA)

void SwapICMPPacket(ICMP_PACKET* p);


/*********************************************************************
 * Function:        BOOL ICMPGet(ICMP_CODE *code,
 *                              BYTE *data,
 *                              BYTE *len,
 *                              WORD *id,
 *                              WORD *seq)
 *
 * PreCondition:    MAC buffer contains ICMP type packet.
 *
 * Input:           code    - Buffer to hold ICMP code value
 *                  data    - Buffer to hold ICMP data
 *                  len     - Buffer to hold ICMP data length
 *                  id      - Buffer to hold ICMP id
 *                  seq     - Buffer to hold ICMP seq
 *
 * Output:          TRUE if valid ICMP packet was received
 *                  FALSE otherwise.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int1 ICMPGet(ICMP_CODE *code,
             int8 *data,
             int8 *len,
             int16 *id,
             int16 *seq)
{
    ICMP_PACKET packet;
    int16 checksums[2];
    int16 CalcChecksum;
    int16 ReceivedChecksum;

    //MACGetArray((int8*)&packet, ICMP_HEADER_SIZE);
    MACGetArray(&packet, ICMP_HEADER_SIZE);

    ReceivedChecksum = packet.Checksum;
    packet.Checksum = 0;

    //checksums[0] = ~CalcIPChecksum((int8*)&packet, ICMP_HEADER_SIZE);
    checksums[0] = ~CalcIPChecksum(&packet, ICMP_HEADER_SIZE);

    *len -= ICMP_HEADER_SIZE;

    MACGetArray(data, *len);
    checksums[1] = ~CalcIPChecksum(data, *len);

    //CalcChecksum = CalcIPChecksum((int8*)checksums, 2 * sizeof(int16));
    CalcChecksum = CalcIPChecksum(&checksums[0], 2 * sizeof(int16));

    SwapICMPPacket(&packet);

    *code = packet.Type;
    *id = packet.Identifier;
    *seq = packet.SequenceNumber;

    return ( CalcChecksum == ReceivedChecksum );
}

/*********************************************************************
 * Function:        void ICMPPut(NODE_INFO *remote,
 *                               ICMP_CODE code,
 *                               BYTE *data,
 *                               BYTE len,
 *                               WORD id,
 *                               WORD seq)
 *
 * PreCondition:    ICMPIsTxReady() == TRUE
 *
 * Input:           remote      - Remote node info
 *                  code        - ICMP_ECHO_REPLY or ICMP_ECHO_REQUEST
 *                  data        - Data bytes
 *                  len         - Number of bytes to send
 *                  id          - ICMP identifier
 *                  seq         - ICMP sequence number
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Note:            A ICMP packet is created and put on MAC.
 *
 ********************************************************************/
void ICMPPut(NODE_INFO *remote,
             ICMP_CODE code,
             int8 *data,
             int8 len,
             int16 id,
             int16 seq)
{
    ICMP_PACKET packet;

    packet.Code             = 0;
    packet.Type             = code;
    packet.Checksum         = 0;
    packet.Identifier       = id;
    packet.SequenceNumber   = seq;

//DSR DID: converted to int8
//    memcpy((void*)packet.Data, (void*)data, len);
    memcpy((int8*)packet.Data, (int8*)data, len);

    SwapICMPPacket(&packet);

    packet.Checksum         = CalcIPChecksum((int8*)&packet,
                                    (int16)(ICMP_HEADER_SIZE + len));

    IPPutHeader(remote,
                IP_PROT_ICMP,
                (int16)(ICMP_HEADER_SIZE + len));

    IPPutArray((int8*)&packet, (int16)(ICMP_HEADER_SIZE + len));

    MACFlush();
}

/*********************************************************************
 * Function:        void SwapICMPPacket(ICMP_PACKET* p)
 *
 * PreCondition:    None
 *
 * Input:           p - ICMP packet header
 *
 * Output:          ICMP packet is swapped
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void SwapICMPPacket(ICMP_PACKET* p)
{
    p->Identifier           = swaps(p->Identifier);
    p->SequenceNumber       = swaps(p->SequenceNumber);
    p->Checksum             = swaps(p->Checksum);
}
