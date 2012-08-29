/*********************************************************************
 *
 *               Microchip TCP/IP Stack FSM Implementation on PIC18
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        StackTsk.c
 * Dependencies:    StackTsk.H
 *                  ARPTsk.h
 *                  MAC.h
 *                  IP.h
 *                  ICMP.h
 *                  Tcp.h
 *                  http.h
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
 * Nilesh Rajbharti     8/14/01  Original (Rev. 1.0)
 * Nilesh Rajbharti     2/9/02   Cleanup
 * Nilesh Rajbharti     5/22/02  Rev 2.0 (See version.log for detail)
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 * Darren Rook (CCS)    06/11/04 A break; added to StackTask() after handling an ARP, else it would goto IP handler.
 * Darren Rook (CCS)    06/28/04 Added 2.20 improvement that resets DHCP after unlink of ethernet
 * Darren Rook (CCS)    06/29/04 A fix for 2.20 improvement (see above) if DHCP was dynamically disabled
 * Darren Rook (CCS)    06/29/04 smStack no longer static
 * Darren Rook (CCS)    04/08/05 Added http.c and http.h.
 * Darren Rook (CCS)    04/08/05 Task() and Init() execute any needed HTTP code
 ********************************************************************/

#define STACK_INCLUDE
#include "drivers\stacktsk.h"
#include <string.h>
#include <stdlib.h>
#include "drivers\helpers.c"
#include "drivers\tick.c"


#define debug(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t)  //this will ignore debug lines
#define debug_putc
#define do_debug_stack  FALSE


/*
   #define do_debug  do_debug_stack
   #use rs232(baud=115200, xmit=PIN_C6,rcv=PIN_C7, STREAM=DEBUGSTREAM)
//   #use rs232(baud=115200, xmit=PIN_G1,rcv=PIN_G2,STREAM=DEBUGSTREAM)
   #define debug printf
   void debug_putc(char c) {
      if (c=='\f') {fputc('\r',DEBUGSTREAM); fputc('\n',DEBUGSTREAM);}
      else {fputc(c,DEBUGSTREAM);}
   }
*/


#if STACK_USE_MAC
   #if STACK_USE_MCPENC
    #include "drivers\enc28j60.h"
    #include "drivers\enc28j60.c"
   #else
    #include "drivers\mac.h"
    #include "drivers\mac.c"
   #endif
#endif

#if STACK_USE_PPP
   #include "drivers\modem.c"
   #include "drivers\ppp.c"
   #include "drivers\pppwrap.c"
#endif

#if STACK_USE_SLIP
   #include "drivers\slip.c"
#ENDIF

#if STACK_USE_UDP
   #include "drivers\udp.h"
#endif

#if STACK_USE_DHCP
   #include "drivers\dhcp.h"
#endif

#if STACK_USE_SMTP
   #include "drivers\smtp.h"
#endif

#if STACK_USE_HTTP
   #include "drivers\http.h"
#endif

#include "drivers\ip.c"

#if STACK_USE_TCP
   #include "drivers\tcp.c"
#endif

#if STACK_USE_ICMP
   #include "drivers\icmp.c"
#endif

#if STACK_USE_UDP
   #include "drivers\udp.c"
#endif

#if STACK_USE_DHCP
   #include "drivers\dhcp.c"
#endif

#if STACK_USE_TELNET
   #include "drivers\telnet2.c"
#endif

#if STACK_USE_ARP
   #include "drivers\arptsk.c"
   #include "drivers\arp.c"
#endif

#if STACK_USE_HTTP
   #include "drivers\http.c"
#endif

#if STACK_USE_SMTP
   #include "drivers\smtp.c"
#endif

#define MAX_ICMP_DATA_LEN   64

/*
 * Stack FSM states.
 */
typedef enum _SM_STACK
{
    SM_STACK_IDLE=0,
    SM_STACK_MAC,
    SM_STACK_IP,
    SM_STACK_ICMP,
    SM_STACK_ICMP_REPLY,
    SM_STACK_ARP,
    SM_STACK_TCP,
    SM_STACK_UDP
} SM_STACK;

SM_STACK smStack;


#if STACK_USE_IP_GLEANING || STACK_USE_DHCP
STACK_FLAGS stackFlags;
#endif



/*********************************************************************
 * Function:        void StackInit(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          Stack and its componentns are initialized
 *
 * Side Effects:    None
 *
 * Note:            This function must be called before any of the
 *                  stack or its component routines be used.
 *
 ********************************************************************/
void StackInit(void)
{
    smStack                     = SM_STACK_IDLE;

#if STACK_USE_IP_GLEANING || STACK_USE_DHCP
    /*
     * If DHCP or IP Gleaning is enabled,
     * startup in Config Mode.
     */
    stackFlags.bits.bInConfigMode = TRUE;
#endif

   TickInit();

#if STACK_USE_MAC
    MACInit();
#endif

#if STACK_USE_ARP
    ARPInit();
#endif

#if STACK_USE_UDP
    UDPInit();
#endif

#if STACK_USE_TCP
    TCPInit();
#endif

#if STACK_USE_DHCP
   DHCPReset();
#endif

#if STACK_USE_PPP
   ppp_init();
#endif

#if STACK_USE_TELNET
   TelnetInit();
#endif

#if STACK_USE_SMTP
   SMTPInit();
#endif

#if STACK_USE_HTTP
   HTTP_Init();
#endif
}


/*********************************************************************
 * Function:        void StackTask(void)
 *
 * PreCondition:    StackInit() is already called.
 *
 * Input:           None
 *
 * Output:          Stack FSM is executed.
 *
 * Side Effects:    None
 *
 * Note:            This FSM checks for new incoming packets,
 *                  and routes it to appropriate stack components.
 *                  It also performs timed operations.
 *
 *                  This function must be called periodically called
 *                  to make sure that timely response.
 *
 ********************************************************************/
void StackTask(void)
{
    static NODE_INFO remoteNode;
    static int16 dataCount;

#if STACK_USE_ICMP
    static int8 data[MAX_ICMP_DATA_LEN];
    static int16 ICMPId;
    static int16 ICMPSeq;
#endif
    IP_ADDR tempLocalIP;


    union
    {
        int8 MACFrameType;
        int8 IPFrameType;
#if STACK_USE_ICMP
        ICMP_CODE ICMPCode;
#endif
    } type;


    int1 lbContinue;

    lbContinue = TRUE;
    while( lbContinue )
    {
        lbContinue = FALSE;
        switch(smStack)
        {
        case SM_STACK_IDLE:
        case SM_STACK_MAC:
            if ( !MACGetHeader(&remoteNode.MACAddr, &type.MACFrameType) )
            {
                #if STACK_USE_DHCP
                    if ( !MACIsLinked()  && !DHCPIsDisabled())
                    {
                        debug(debug_putc,"\fDHCP RESET");
                        MY_IP_BYTE1 = 0;
                        MY_IP_BYTE2 = 0;
                        MY_IP_BYTE3 = 0;
                        MY_IP_BYTE4 = 0;

                        stackFlags.bits.bInConfigMode = TRUE;
                        DHCPReset();
                    }
                #endif
                break;
            }

            debug(debug_putc,"\fMACGETHDR: ");

            lbContinue = TRUE;
            if ( type.MACFrameType == MAC_IP ) {
                smStack = SM_STACK_IP;
            }
            else if ( type.MACFrameType == MAC_ARP ) {
                smStack = SM_STACK_ARP;
                //debug(debug_putc,"ARP %X",smARP);
            }
            else {
                MACDiscardRx();
                debug(debug_putc,"DISCARD %X", type.MACFrameType);
            }
            break;

        case SM_STACK_ARP:
            lbContinue = FALSE;
#if STACK_USE_ARP
            if ( ARPProcess() ) {
                smStack = SM_STACK_IDLE;
            }
#else
            smStack = SM_STACK_IDLE;
#endif
            break;

        case SM_STACK_IP:
            if ( IPGetHeader(&tempLocalIP,
                             &remoteNode,
                             &type.IPFrameType,
                             &dataCount) )
            {
                lbContinue = TRUE;
                if ( type.IPFrameType == IP_PROT_ICMP )
                {
                    smStack = SM_STACK_ICMP;
                    debug(debug_putc,"ICMP");
#if STACK_USE_IP_GLEANING
                    if ( stackFlags.bits.bInConfigMode )
                    {
                        /*
                         * Accoriding to "IP Gleaning" procedure,
                         * when we receive an ICMP packet with a valid
                         * IP address while we are still in configuration
                         * mode, accept that address as ours and conclude
                         * configuration mode.
                         */
                        if ( tempLocalIP.Val != 0xffffffff )
                        {
                            stackFlags.bits.bInConfigMode    = FALSE;
                            MY_IP_BYTE1                 = tempLocalIP.v[0];
                            MY_IP_BYTE2                 = tempLocalIP.v[1];
                            MY_IP_BYTE3                 = tempLocalIP.v[2];
                            MY_IP_BYTE4                 = tempLocalIP.v[3];

#if STACK_USE_DHCP
                            /*
                             * If DHCP and IP gleaning is enabled at the
                             * same time, we must ensuer that once we have
                             * IP address through IP gleaning, we abort
                             * any pending DHCP requests and do not renew
                             * any new DHCP configuration.
                             */
                            DHCPAbort();
#endif
                        }
                    }
#endif
                }

#if STACK_USE_TCP
                else if ( type.IPFrameType == IP_PROT_TCP ) {
                    smStack = SM_STACK_TCP;
                    debug(debug_putc,"TCP");
                }
#endif

#if STACK_USE_UDP
                else if ( type.IPFrameType == IP_PROT_UDP ) {
                    smStack = SM_STACK_UDP;
                    debug(debug_putc,"UDP");
                }
#endif

                else
                {
                    lbContinue = FALSE;
                    MACDiscardRx();

                    smStack = SM_STACK_IDLE;
                    debug(debug_putc,"IDLE");
                }
            }
            else
            {
                MACDiscardRx();
                smStack = SM_STACK_IDLE;
                debug(debug_putc,"NONE");
            }
            break;

#if STACK_USE_UDP
        case SM_STACK_UDP:
            if ( UDPProcess(&remoteNode, dataCount) )
                smStack = SM_STACK_IDLE;
            lbContinue = FALSE;
            break;
#endif

#if STACK_USE_TCP
        case SM_STACK_TCP:
            if ( TCPProcess(&remoteNode, dataCount) )
                smStack = SM_STACK_IDLE;
            lbContinue = FALSE;
            break;
#endif

        case SM_STACK_ICMP:
            smStack = SM_STACK_IDLE;

#if STACK_USE_ICMP
            if ( dataCount <= (MAX_ICMP_DATA_LEN+9) )
            {
                if ( ICMPGet(&type.ICMPCode,
                             data,
                             (int8*)&dataCount,
                             &ICMPId,
                             &ICMPSeq) )
                {
                    if ( type.ICMPCode == ICMP_ECHO_REQUEST )
                    {
                        lbContinue = TRUE;
                        smStack = SM_STACK_ICMP_REPLY;
                    }
                    else
                    {
                        smStack = SM_STACK_IDLE;
                    }
                }
                else
                {
                    smStack = SM_STACK_IDLE;
                }
            }
#endif
            MACDiscardRx();
            break;

#if STACK_USE_ICMP
        case SM_STACK_ICMP_REPLY:
            if ( ICMPIsTxReady() )
            {
                ICMPPut(&remoteNode,
                        ICMP_ECHO_REPLY,
                        data,
                        (int8)dataCount,
                        ICMPId,
                        ICMPSeq);

                smStack = SM_STACK_IDLE;
            }
            break;
#endif

        }

    }

#if STACK_USE_SMTP
   SMTPTask();
#endif

#if STACK_USE_TCP
    // Perform timed TCP FSM.
    TCPTick();
#endif

#if STACK_USE_TELNET
   TelnetTask();
#endif

#if STACK_USE_HTTP
   HTTP_Task();
#endif


#if STACK_USE_DHCP
    /*
     * DHCP must be called all the time even after IP configuration is
     * discovered.
     * DHCP has to account lease expiration time and renew the configuration
     * time.
     */
    DHCPTask();

    if ( DHCPIsBound() )
        stackFlags.bits.bInConfigMode = FALSE;

#endif
}

