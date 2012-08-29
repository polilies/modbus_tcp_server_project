/*********************************************************************
 *
 *                  DHCP Module for Microchip TCP/IP Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        DHCP.c
 * Dependencies:    StackTsk.h
 *                  UDP.h
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
 * Nilesh Rajbharti     3/21/01  Original        (Rev 1.0)
 * Nilesh Rajbharti     7/10/02  Explicitly initialized tempIPAddress
 *                                               (Rev 2.11)
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 * Darren Rook (CCS)    06/14/04 DHCP will force set IP Address to 0.0.0.0 during init.
 * Nilesh Rajbharti     5/16/03 Increased DHCP_TIMEOUT to 2 seconds.
 * Nilesh Rajbharti     5/16/03 Fixed SM_DHCP_BROADCAST logic
 *                              where UDPPut was called before setting
 *                              active socket.
 * Robert Sloan         5/29/03 Improved DHCP State machine to handle
 *                              NAK and renew existing IP address.
 * Nilesh Rajbharti     8/15/03 Modified _DHCPRecieve() to check for
 *                              chaddr field before accpting the packet.
 *                              Fixed DHCPTask() where it would not
 *                              reply to first OFFER.
 * Darren Rook (CCS)    6/28/04 CCS Port now uses 2.20 (see above)
 * Darren Rook (CCS)    6/29/04 DHCPServerID, DHCPLeaseTime, tempIPAddress,
 *                              tempGateway, tempMask, _DHCPReceive()
 *                              and _DHCPSend() no longer static.
 ********************************************************************/

#include "drivers\stacktsk.h"
#include "drivers\dhcp.h"
#include "drivers\udp.h"
#include "drivers\tick.h"

#if defined(STACK_USE_SIIP)
    #error DHCP module is not available when SLIP is used.
#endif

#define DHCP_TIMEOUT                    (2 * TICKS_PER_SECOND)


#define DHCP_CLIENT_PORT                (68)
#define DHCP_SERVER_PORT                (67)

#define BOOT_REQUEST                    (1)
#define BOOT_REPLY                      (2)
#define HW_TYPE                         (1)
#define LEN_OF_HW_TYPE                  (6)

#define DHCP_MESSAGE_TYPE               (53)
#define DHCP_MESSAGE_TYPE_LEN           (1)

#define DHCP_UNKNOWN_MESSAGE            (0)

#define DHCP_DISCOVER_MESSAGE           (1)
#define DHCP_OFFER_MESSAGE              (2)
#define DHCP_REQUEST_MESSAGE            (3)
#define DHCP_DECLINE_MESSAGE            (4)
#define DHCP_ACK_MESSAGE                (5)
#define DHCP_NAK_MESSAGE                (6)
#define DHCP_RELEASE_MESSAGE            (7)

#define DHCP_SERVER_IDENTIFIER          (54)
#define DHCP_SERVER_IDENTIFIER_LEN      (4)

#define DHCP_PARAM_REQUEST_LIST         (55)
#define DHCP_PARAM_REQUEST_LIST_LEN     (2)
#define DHCP_PARAM_REQUEST_IP_ADDRESS       (50)
#define DHCP_PARAM_REQUEST_IP_ADDRESS_LEN   (4)
#define DHCP_SUBNET_MASK                (1)
#define DHCP_ROUTER                     (3)
#define DHCP_IP_LEASE_TIME              (51)
#define DHCP_END_OPTION                 (255)

#define DHCP_INIT_IPADDR                0xffffffff //255.255.255.255

#define HALF_HOUR                       (60 * 30)

DWORD_VAL DHCPServerID;
DWORD_VAL DHCPLeaseTime;

IP_ADDR tempIPAddress;
IP_ADDR tempGateway;
IP_ADDR tempMask;

int8 _DHCPReceive(void);
void _DHCPSend(int8 messageType);

int8 DHCPBindCount = 0;



#define USARTPut(a)

void DHCPReset(void)
{
    // Do not reset DHCP if it was previously disabled.
    if ( smDHCPState == SM_DHCP_DISABLED )
        return;

    if ( DHCPSocket != INVALID_UDP_SOCKET )
        UDPClose(DHCPSocket);
        
    DHCPSocket = INVALID_UDP_SOCKET;

    smDHCPState = SM_DHCP_INIT;
    DHCPBindCount = 0;
    DHCPState.Val=0;
}

/*********************************************************************
 * Function:        void DHCPTask(void)
 *
 * PreCondition:    DHCPInit() is already called AND
 *                  IPGetHeader() is called with
 *                  IPFrameType == IP_PROT_UDP
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Fetches pending UDP packet from MAC receive buffer
 *                  and dispatches it appropriate UDP socket.
 *                  If not UDP socket is matched, UDP packet is
 *                  silently discarded.
 *
 * Note:            Caller must make sure that MAC receive buffer
 *                  access pointer is set to begining of UDP packet.
 *                  Required steps before calling this function is:
 *
 *                  If ( MACIsRxReady() )
 *                  {
 *                      MACGetHeader()
 *                      If MACFrameType == IP
 *                          IPGetHeader()
 *                          if ( IPFrameType == IP_PROT_UDP )
 *                              Call DHCPTask()
 *                  ...
 ********************************************************************/
void DHCPTask(void)
{
    NODE_INFO DHCPServerNode;
    static TICKTYPE lastTryTick;
    int8 DHCPRecvReturnValue;

    switch(smDHCPState)
    {
    case SM_DHCP_INIT:
        //debug(debug_putc,"\r\n\r\nDHCP: INIT");

        //dsr add 061404
        MY_IP=0;

        DHCPServerNode.MACAddr.v[0] = 0xff;
        DHCPServerNode.MACAddr.v[1] = 0xff;
        DHCPServerNode.MACAddr.v[2] = 0xff;
        DHCPServerNode.MACAddr.v[3] = 0xff;
        DHCPServerNode.MACAddr.v[4] = 0xff;
        DHCPServerNode.MACAddr.v[5] = 0xff;
        DHCPServerNode.IPAddr.Val = DHCP_INIT_IPADDR;
        tempIPAddress.Val = 0x0;
        DHCPSocket = UDPOpen(DHCP_CLIENT_PORT,
                             &DHCPServerNode,
                             DHCP_SERVER_PORT);
        lastTryTick = TickGet();
        smDHCPState = SM_DHCP_RESET_WAIT;
        /* No break */

    case SM_DHCP_RESET_WAIT:
        if ( TickGetDiff(TickGet(), lastTryTick) >= (TICKS_PER_SECOND/5) )
            //debug(debug_putc,"\r\n\r\nDHCP: RESET_WAIT");
            smDHCPState = SM_DHCP_BROADCAST;
        break;

    case SM_DHCP_BROADCAST:
        /*
         * If we have already obtained some IP address, renew it.
         */
        if (tempIPAddress.Val )
        {
            //debug(debug_putc,"\r\n\r\nDHCP: BROADCAST BACK TO REQUEST");
            smDHCPState = SM_DHCP_REQUEST;
        }
        else if ( UDPIsPutReady(DHCPSocket) )
        {
            /*
             * To minimize code requirement, user must make sure that
             * above call will be successful by making at least one
             * UDP socket available.
             * Usually this will be the case, given that DHCP will be
             * the first one to use UDP socket.
             *
             * Also, we will not check for transmitter readiness,
             * we assume it to be ready.
             */


               _DHCPSend(DHCP_DISCOVER_MESSAGE);
               lastTryTick = TickGet();
               smDHCPState = SM_DHCP_DISCOVER;
        }

        break;


    case SM_DHCP_DISCOVER:
        if ( TickGetDiff(TickGet(), lastTryTick) >= DHCP_TIMEOUT ) {
            //debug(debug_putc,"\r\n\r\nDHCP: DISCOVER TO BROADCAST");
            smDHCPState = SM_DHCP_BROADCAST;
        }

        if ( UDPIsGetReady(DHCPSocket) )
        {

            if ( _DHCPReceive() == DHCP_OFFER_MESSAGE )
            {
                //debug(debug_putc,"\r\n\r\nDHCP: DISCOVER BACK TO REQUEST");
                smDHCPState = SM_DHCP_REQUEST;
            }
            else
               break;
        }
        else
           break;


    case SM_DHCP_REQUEST:
        if ( UDPIsPutReady(DHCPSocket) )
        {
            _DHCPSend(DHCP_REQUEST_MESSAGE);
            lastTryTick = TickGet();
            smDHCPState = SM_DHCP_BIND;
            //debug(debug_putc,"\r\n\r\nDHCP: REQUEST TO BIND");
        }
        break;

    case SM_DHCP_BIND:
        if ( UDPIsGetReady(DHCPSocket) )
        {
            DHCPRecvReturnValue = _DHCPReceive();

            if ( DHCPRecvReturnValue == DHCP_NAK_MESSAGE )
            {
               // (RSS) NAK recieved.  DHCP server didn't like our DHCP Request format
                smDHCPState = SM_DHCP_REQUEST;   // Request again
            }
            else if ( DHCPRecvReturnValue == DHCP_ACK_MESSAGE )
            {

                /*
                 * Once DCHP is successful, release the UDP socket
                 * This will ensure that UDP layer discards any further
                 * DHCP related packets.
                 */
                UDPClose(DHCPSocket);
                DHCPSocket = INVALID_UDP_SOCKET;

                lastTryTick = TickGet();
                smDHCPState = SM_DHCP_BOUND;


                MY_IP_BYTE1     = tempIPAddress.v[0];
                MY_IP_BYTE2     = tempIPAddress.v[1];
                MY_IP_BYTE3     = tempIPAddress.v[2];
                MY_IP_BYTE4     = tempIPAddress.v[3];

                MY_MASK_BYTE1   = tempMask.v[0];
                MY_MASK_BYTE2   = tempMask.v[1];
                MY_MASK_BYTE3   = tempMask.v[2];
                MY_MASK_BYTE4   = tempMask.v[3];

                MY_GATE_BYTE1   = tempGateway.v[0];
                MY_GATE_BYTE2   = tempGateway.v[1];
                MY_GATE_BYTE3   = tempGateway.v[2];
                MY_GATE_BYTE4   = tempGateway.v[3];

                DHCPState.bits.bIsBound = TRUE;

                DHCPBindCount++;

                //debug(debug_putc,"\r\n\r\nDHCP: BOUND IP:%U.%U.%U.%U MASK:%U.%U.%U.%U",
                //  MY_IP_BYTE1,MY_IP_BYTE2,MY_IP_BYTE3,MY_IP_BYTE4,
                //  MY_MASK_BYTE1,MY_MASK_BYTE2,MY_MASK_BYTE3,MY_MASK_BYTE4);
                //debug(debug_putc,"\r\    GW:%U.%U.%U.%U",
                //  MY_GATE_BYTE1,MY_GATE_BYTE2,MY_GATE_BYTE3,MY_GATE_BYTE4);
                return;
            }
        }
        else if ( TickGetDiff(TickGet(), lastTryTick) >= DHCP_TIMEOUT )
            smDHCPState = SM_DHCP_BROADCAST;
        break;

    case SM_DHCP_BOUND:
        /*
         * Keep track of how long we use this IP configuration.
         * When lease period expires, renew the configuration.
         */
        if ( TickGetDiff(TickGet(), lastTryTick) >= TICKS_PER_SECOND )
        {
            DHCPLeaseTime.Val -= 1;
            if (!DHCPLeaseTime.Val) {
                smDHCPState = SM_DHCP_INIT;
                //debug(debug_putc,"\r\n\r\nDHCP: LEASE TIME EXPIRED");
            }
            lastTryTick = TickGet();
        }
    }

}



/*********************************************************************
 * Function:        void DHCPAbort(void)
 *
 * PreCondition:    DHCPTask() must have been called at least once.
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Forgets about any previous DHCP attempts and
 *                  closes DHCPSocket.
 *
 * Note:
 ********************************************************************/
void DHCPAbort(void)
{
    smDHCPState = SM_DHCP_ABORTED;
    UDPClose(DHCPSocket);
}





/*********************************************************************
        DHCP PACKET FORMAT AS PER RFC 1541

   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     op (1)    |   htype (1)   |   hlen (1)    |   hops (1)    |
   +---------------+---------------+---------------+---------------+
   |                            xid (4)                            |
   +-------------------------------+-------------------------------+
   |           secs (2)            |           flags (2)           |
   +-------------------------------+-------------------------------+
   |                          ciaddr  (4)                          |
   +---------------------------------------------------------------+
   |                          yiaddr  (4)                          |
   +---------------------------------------------------------------+
   |                          siaddr  (4)                          |
   +---------------------------------------------------------------+
   |                          giaddr  (4)                          |
   +---------------------------------------------------------------+
   |                                                               |
   |                          chaddr  (16)                         |
   |                                                               |
   |                                                               |
   +---------------------------------------------------------------+
   |                                                               |
   |                          sname   (64)                         |
   +---------------------------------------------------------------+
   |                                                               |
   |                          file    (128)                        |
   +---------------------------------------------------------------+
   |                                                               |
   |                          options (312)                        |
   +---------------------------------------------------------------+

 ********************************************************************/
int8 _DHCPReceive(void)
{
    int8 v;
    int8 i;
    int8 type;
    int1 lbDone;
    DWORD_VAL tempServerID;


    // Assume unknown message until proven otherwise.
    type = DHCP_UNKNOWN_MESSAGE;

    /*
     * Assume default IP Lease time of 60 seconds.
     * This should be minimum possible to make sure that if
     * server did not specify lease time, we try again after
     * this minimum time.
     */
    DHCPLeaseTime.Val = 60;

    UDPGet(&v);                             // op
    /*
     * Make sure this is BOOT_REPLY.
     */
    if ( v == BOOT_REPLY )
    {
        /*
         * Discard htype, hlen, hops, xid, secs, flags, ciaddr.
         */
        for ( i = 0; i < 15; i++ )
            UDPGet(&v);

        /*
         * Save offered IP address until we know for sure that
         * we have it.
         */
        UDPGet(&tempIPAddress.v[0]);
        UDPGet(&tempIPAddress.v[1]);
        UDPGet(&tempIPAddress.v[2]);
        UDPGet(&tempIPAddress.v[3]);

        /*
         * Ignore siaddr, giaddr
         */
        for ( i = 0; i < 8; i++ )
            UDPGet(&v);
        /*
         * Check to see if chaddr (Client Hardware Address) belongs to us.
         */
        for ( i = 0; i < 6; i++ )
        {
            UDPGet(&v);
            if ( v != AppConfig.MyMACAddr.v[i])
                goto UDPInvalid;
        }
        /*
         * Ignore part of chaddr, sname, file, magic cookie.
         */
        for ( i = 0; i < 206; i++ )
            UDPGet(&v);

        lbDone = FALSE;
        do
        {
            UDPGet(&v);

            switch(v)
            {
            case DHCP_MESSAGE_TYPE:
                UDPGet(&v);                         // Skip len
                // Len must be 1.
                if ( v == 1 )
                {
                    UDPGet(&type);                  // Get type
                }
                else
                    goto UDPInvalid;
                break;

            case DHCP_SUBNET_MASK:
                UDPGet(&v);                     // Skip len
                // Len must be 4.
                if ( v == 4 )
                {
                    UDPGet(&tempMask.v[0]);
                    UDPGet(&tempMask.v[1]);
                    UDPGet(&tempMask.v[2]);
                    UDPGet(&tempMask.v[3]);
                }
                else
                    goto UDPInvalid;
                break;

            case DHCP_ROUTER:
                UDPGet(&v);
                // Len must be >= 4.
                if ( v >= 4 )
                {
                    UDPGet(&tempGateway.v[0]);
                    UDPGet(&tempGateway.v[1]);
                    UDPGet(&tempGateway.v[2]);
                    UDPGet(&tempGateway.v[3]);
                }
                else
                    goto UDPInvalid;

                /*
                 * Discard any other router addresses.
                 */
                v -= 4;
                while(v--)
                    UDPGet(&i);
                break;

            case DHCP_SERVER_IDENTIFIER:
                UDPGet(&v);                         // Get len
                // Len must be 4.
                if ( v == 4 )
                {
                    UDPGet(&UPPER_MSB(tempServerID));   // Get the id
                    UDPGet(&UPPER_LSB(tempServerID));
                    UDPGet(&LOWER_MSB(tempServerID));
                    UDPGet(&LOWER_LSB(tempServerID));
                }
                else
                    goto UDPInvalid;
                break;

            case DHCP_END_OPTION:
                lbDone = TRUE;
                break;

            case DHCP_IP_LEASE_TIME:
                UDPGet(&v);                         // Get len
                // Len must be 4.
                if ( v == 4 )
                {
                    UDPGet(&UPPER_MSB(DHCPLeaseTime));
                    UDPGet(&UPPER_LSB(DHCPLeaseTime));
                    UDPGet(&LOWER_MSB(DHCPLeaseTime));
                    UDPGet(&LOWER_LSB(DHCPLeaseTime));

                    /*
                     * Due to possible timing delays, consider actual lease
                     * time less by half hour.
                     */
                    if ( DHCPLeaseTime.Val > HALF_HOUR )
                        DHCPLeaseTime.Val = DHCPLeaseTime.Val - HALF_HOUR;

                }
                else
                    goto UDPInvalid;
                break;

            default:
                // Ignore all unsupport tags.
                UDPGet(&v);                     // Get option len
                while( v-- )                    // Ignore option values
                    UDPGet(&i);
            }
        } while( !lbDone );
    }

    /*
     * If this is an OFFER message, remember current server id.
     */
    if ( type == DHCP_OFFER_MESSAGE )
    {
        DHCPServerID.Val = tempServerID.Val;
    }
    else
    {
        /*
         * For other types of messages, make sure that received
         * server id matches with our previous one.
         */
        if ( DHCPServerID.Val != tempServerID.Val )
            type = DHCP_UNKNOWN_MESSAGE;
    }

    UDPDiscard();                             // We are done with this packet
    return type;

UDPInvalid:
    UDPDiscard();
    return DHCP_UNKNOWN_MESSAGE;

}





void _DHCPSend(int8 messageType)
{
    int8 i;

    UDPPut(BOOT_REQUEST);                       // op
    UDPPut(HW_TYPE);                            // htype
    UDPPut(LEN_OF_HW_TYPE);                     // hlen
    UDPPut(0);                                  // hops
    UDPPut(0x12);                               // xid[0]
    UDPPut(0x23);                               // xid[1]
    UDPPut(0x34);                               // xid[2]
    UDPPut(0x56);                               // xid[3]
    UDPPut(0);                                  // secs[0]
    UDPPut(0);                                  // secs[1]
    UDPPut(0x80);                               // flags[0] with BF set
    UDPPut(0);                                  // flags[1]


    /*
     * If this is DHCP REQUEST message, use previously allocated
     * IP address.
     */
#if 0
    if ( messageType == DHCP_REQUEST_MESSAGE )
    {
        UDPPut(tempIPAddress.v[0]);
        UDPPut(tempIPAddress.v[1]);
        UDPPut(tempIPAddress.v[2]);
        UDPPut(tempIPAddress.v[3]);
    }
    else
#endif
    {
        UDPPut(0x00);
        UDPPut(0x00);
        UDPPut(0x00);
        UDPPut(0x00);
    }

    /*
     * Set yiaddr, siaddr, giaddr as zeros,
     */
    for ( i = 0; i < 12; i++ )
        UDPPut(0x00);



    /*
     * Load chaddr - Client hardware address.
     */
    UDPPut(MY_MAC_BYTE1);
    UDPPut(MY_MAC_BYTE2);
    UDPPut(MY_MAC_BYTE3);
    UDPPut(MY_MAC_BYTE4);
    UDPPut(MY_MAC_BYTE5);
    UDPPut(MY_MAC_BYTE6);

    /*
     * Set chaddr[6..15], sname and file as zeros.
     */
    for ( i = 0; i < 202; i++ )
        UDPPut(0);

    /*
     * Load magic cookie as per RFC 1533.
     */
    UDPPut(99);
    UDPPut(130);
    UDPPut(83);
    UDPPut(99);

    /*
     * Load message type.
     */
    UDPPut(DHCP_MESSAGE_TYPE);
    UDPPut(DHCP_MESSAGE_TYPE_LEN);
    UDPPut(messageType);

    if ( (messageType != DHCP_DISCOVER_MESSAGE) && tempIPAddress.Val)
    {
        /*
         * DHCP REQUEST message may include server identifier,
         * to identify the server we are talking to.
         * DHCP ACK may include it too.  To simplify logic,
         * we will include server identifier in DHCP ACK message
         * too.
         * _DHCPReceive() would populate "serverID" when it
         * receives DHCP OFFER message. We will simply use that
         * when we are replying to server.
         *
         * If this is a renwal request, do not include server id.
         */
         UDPPut(DHCP_SERVER_IDENTIFIER);
         UDPPut(DHCP_SERVER_IDENTIFIER_LEN);
         UDPPut(UPPER_MSB(DHCPServerID));
         UDPPut(UPPER_LSB(DHCPServerID));
         UDPPut(LOWER_MSB(DHCPServerID));
         UDPPut(LOWER_LSB(DHCPServerID));
     }

    /*
     * Load our interested parameters
     * This is hardcoded list.  If any new parameters are desired,
     * new lines must be added here.
     */
    UDPPut(DHCP_PARAM_REQUEST_LIST);
    UDPPut(DHCP_PARAM_REQUEST_LIST_LEN);
    UDPPut(DHCP_SUBNET_MASK);
    UDPPut(DHCP_ROUTER);

    if ( messageType == DHCP_REQUEST_MESSAGE )
    {
        UDPPut(DHCP_PARAM_REQUEST_IP_ADDRESS);
        UDPPut(DHCP_PARAM_REQUEST_IP_ADDRESS_LEN);

        UDPPut(tempIPAddress.v[0]);
        UDPPut(tempIPAddress.v[1]);
        UDPPut(tempIPAddress.v[2]);
        UDPPut(tempIPAddress.v[3]);
    }

    /*
     * Add any new paramter request here.
     */

    /*
     * End of Options.
     */
    UDPPut(DHCP_END_OPTION);

    UDPFlush();
}

