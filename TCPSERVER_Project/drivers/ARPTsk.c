/*********************************************************************
 *
 *                  ARP Server Module for Microchip TCP/IP Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        ARPTsk.c
 * Dependencies:    compiler.h
 *                  string.h
 *                  ARP.h
 *                  ARPTsk.h
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
 * Nilesh Rajbharti     8/20/01  Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02   Cleanup
 * Nilesh Rajbharti     5/22/02  Rev 2.0 (See version.log for detail)
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 * Darren Rook (CCS)    06/11/04 A macdiscardrx() added to arpprocess() to handle times when an eth packet with arp has padding bytes at the end
 * Darren Rook (CCS)    06/28/04 ArpInit clears cache like in 2.20
 * Darren Rook (CCS)    06/29/04 smArp, Cache no longer static
 ********************************************************************/

#include <string.h>

#include "drivers\arp.h"
#include "drivers\arptsk.h"

/*
 * ARP Task FSM States
 */
typedef enum _ARP_STATE
{
    SM_ARP_IDLE,
    SM_ARP_REPLY
} ARP_STATE;


/*
 * This ARP task caches one ARP response.
 */
ARP_STATE smARP;

NODE_INFO Cache;


/*********************************************************************
 * Function:        void ARPInit(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          ARP Cache is initialized.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void ARPInit(void)
{
    smARP = SM_ARP_IDLE;

    Cache.MACAddr.v[0] = 0xff;
    Cache.MACAddr.v[1] = 0xff;
    Cache.MACAddr.v[2] = 0xff;
    Cache.MACAddr.v[3] = 0xff;
    Cache.MACAddr.v[4] = 0xff;
    Cache.MACAddr.v[5] = 0xff;

    Cache.IPAddr.Val = 0x0;
}



/*********************************************************************
 * Function:        BOOL ARPProcess(void)
 *
 * PreCondition:    ARP packet is ready in MAC buffer.
 *
 * Input:           None
 *
 * Output:          ARP FSM is executed.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int1 ARPProcess(void)
{
    NODE_INFO remoteNode;
    int8 opCode;

    switch(smARP)
    {
    case SM_ARP_IDLE:
        if ( !ARPGet(&remoteNode, &opCode) )
            break;

         //dsr add 071104
         MacDiscardRx();

        if ( opCode == ARP_REPLY )
        {
            Cache.MACAddr.v[0] = remoteNode.MACAddr.v[0];
            Cache.MACAddr.v[1] = remoteNode.MACAddr.v[1];
            Cache.MACAddr.v[2] = remoteNode.MACAddr.v[2];
            Cache.MACAddr.v[3] = remoteNode.MACAddr.v[3];
            Cache.MACAddr.v[4] = remoteNode.MACAddr.v[4];
            Cache.MACAddr.v[5] = remoteNode.MACAddr.v[5];

            Cache.IPAddr.Val = remoteNode.IPAddr.Val;
            break;
        }
        else
            smARP = SM_ARP_REPLY;

    default:
        if ( ARPIsTxReady() )
        {
            debug("\r\nPUTTING ARP REPLY");
            ARPPut(&remoteNode, ARP_REPLY);
            smARP = SM_ARP_IDLE;
        }
        else {
            return FALSE;
        }
        break;
    }
    return TRUE;
}

/*********************************************************************
 * Function:        void ARPResolve(IP_ADDR* IPAddr)
 *
 * PreCondition:    None
 *
 * Input:           IPAddr  - IP Address to be resolved.
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        An ARP request is sent.
 *
 ********************************************************************/
void ARPResolve(IP_ADDR *IPAddr)
{
    NODE_INFO remoteNode;

    remoteNode.IPAddr = *IPAddr;

    ARPPut(&remoteNode, ARP_REQUEST);
}



/*********************************************************************
 * Function:        BOOL ARPIsResolved(IP_ADDR* IPAddr,
 *                                      MAC_ADDR *MACAddr)
 *
 * PreCondition:    None
 *
 * Input:           IPAddr      - IPAddress to be resolved.
 *                  MACAddr     - Buffer to hold corresponding
 *                                MAC Address.
 *
 * Output:          TRUE if given IP Address has been resolved.
 *                  FALSE otherwise.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 ********************************************************************/
int1 ARPIsResolved(IP_ADDR *IPAddr, MAC_ADDR *MACAddr)
{
    if ( (Cache.IPAddr.Val == IPAddr->Val) ||
         (Cache.IPAddr.v[0] == MY_GATE_BYTE1 &&
          Cache.IPAddr.v[1] == MY_GATE_BYTE2 &&
          Cache.IPAddr.v[2] == MY_GATE_BYTE3 &&
          Cache.IPAddr.v[3] == MY_GATE_BYTE4) )
    {
        *MACAddr = Cache.MACAddr;
        return TRUE;
    }
    return FALSE;
}


