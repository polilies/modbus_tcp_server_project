/*********************************************************************
 *
 *                  IP Defs for Microchip TCP/IP Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        IP.h
 * Dependencies:    StackTsk.h
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
 * Nilesh Rajbharti     4/27/01  Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02   Cleanup
 * Nilesh Rajbharti     5/22/02  Rev 2.0 (See version.log for detail)
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 ********************************************************************/

#ifndef IP_H
#define IP_H

#include "drivers\stacktsk.h"


#define IP_PROT_ICMP    (1)
#define IP_PROT_TCP     (6)
#define IP_PROT_UDP     (17)


/*
 * IP packet header definition
 */
typedef struct _IP_HEADER
{
    int8    VersionIHL;
    int8    TypeOfService;
    int16    TotalLength;
    int16    Identification;
    int16    FragmentInfo;
    int8    TimeToLive;
    int8    Protocol;
    int16    HeaderChecksum;
    IP_ADDR SourceAddress;
    IP_ADDR DestAddress;

} IP_HEADER;


/*********************************************************************
 * Function:        BOOL IPIsTxReady()
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          TRUE if transmit buffer is empty
 *                  FALSE if transmit buffer is not empty
 *
 * Side Effects:    None
 *
 * Note:            None
 *
 ********************************************************************/
#define IPIsTxReady()       MACIsTxReady()


/*********************************************************************
 * Macro:           IPSetTxBuffer(a, b)
 *
 * PreCondition:    None
 *
 * Input:           a       - Buffer identifier
 *                  b       - Offset
 *
 * Output:          Next Read/Write access to transmit buffer 'a'
 *                  set to offset 'b'
 *
 * Side Effects:    None
 *
 * Note:            None
 *
 ********************************************************************/
#define IPSetTxBuffer(a, b) MACSetTxBuffer(a, b+sizeof(IP_HEADER))



/*********************************************************************
 * Function:        WORD IPPutHeader(   IP_ADDR *Dest,
 *                                      BYTE    Protocol,
 *                                      WORD    Identifier,
 *                                      WORD    DataLen)
 *
 * PreCondition:    IPIsTxReady() == TRUE
 *
 * Input:           Src         - Destination node address
 *                  Protocol    - Current packet protocol
 *                  Identifier  - Current packet identifier
 *                  DataLen     - Current packet data length
 *
 * Output:          Handle to current packet - For use by
 *                  IPSendByte() function.
 *
 * Side Effects:    None
 *
 * Note:            Only one IP message can be transmitted at any
 *                  time.
 *                  Caller may not transmit and receive a message
 *                  at the same time.
 *
 ********************************************************************/
int16    IPPutHeader(NODE_INFO *remote,
                    int8 protocol,
                    int16 len);


/*********************************************************************
 * Macro:           IPPutArray(a, b)
 *
 * PreCondition:    IPIsTxReady() == TRUE
 *
 * Input:           a       - Data buffer
 *                  b       - Buffer length
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Note:            Data is copied to IP data area.
 *
 ********************************************************************/
#define IPPutArray(a, b)    MACPutArray(a, b)




/*********************************************************************
 * Function:        BOOL IPGetHeader( IP_ADDR    *localIP,
 *                                    NODE_INFO  *remote,
 *                                    BYTE        *Protocol,
 *                                    WORD        *len)
 *
 * PreCondition:    MACGetHeader() == TRUE
 *
 * Input:           localIP     - Local node IP Address as received
 *                                in current IP header.
 *                                If this information is not required
 *                                caller may pass NULL value.
 *                  remote      - Remote node info
 *                  Protocol    - Current packet protocol
 *                  len         - Current packet data length
 *
 * Output:          TRUE, if valid packet was received
 *                  FALSE otherwise
 *
 * Side Effects:    None
 *
 * Note:            Only one IP message can be received.
 *                  Caller may not transmit and receive a message
 *                  at the same time.
 *
 ********************************************************************/
int1 IPGetHeader(IP_ADDR *localIP,
                 NODE_INFO *remote,
                 int8 *protocol,
                 int16 *len);


/*********************************************************************
 * Macro:           IPDiscard()
 *
 * PreCondition:    MACGetHeader() == TRUE
 *
 * Input:           None
 *
 * Output:          Current packet is discarded and buffer is
 *                  freed-up
 *
 * Side Effects:    None
 *
 * Note:            None
 *
 ********************************************************************/
#define IPDiscard()         MACDiscard()



/*********************************************************************
 * Macro:           IPGetArray(a, b)
 *
 * PreCondition:    MACGetHeader() == TRUE
 *
 * Input:           a       - Data buffer
 *                  b       - Buffer length
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Note:            Data is copied from IP data to given buffer
 *
 ********************************************************************/
#define IPGetArray(a, b)    MACGetArray(a, b)




/*********************************************************************
 * Macro:           IPSetRxBuffer(a)
 *
 * PreCondition:    None
 *
 * Input:           a       - Offset
 *
 * Output:          Next Read/Write access to receive buffer is
 *                  set to offset 'b'
 *
 * Side Effects:    None
 *
 * Note:            None
 *
 ********************************************************************/
#define IPSetRxBuffer(a) MACSetRxBuffer(a+sizeof(IP_HEADER))





#endif



