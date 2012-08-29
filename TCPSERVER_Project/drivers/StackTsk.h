/*********************************************************************
 *
 *                  Microchip TCP/IP Stack Definations for PIC18
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        StackTsk.h
 * Dependencies:    compiler.h
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
 * Nilesh Rajbharti     8/10/01  Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02   Cleanup
 * Nilesh Rajbharti     5/22/02  Rev 2.0 (See version.log for detail)
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 * Darren Rook (CCS)    04/08/05 Added http.c and http.h.
 ********************************************************************/
#ifndef STACK_TSK_H
#define STACK_TSK_H

#include "drivers\pic18.h"

#ifndef STACK_USE_MAC
   #define  STACK_USE_MAC  TRUE
#endif

#ifndef STACK_USE_MCPENC
   #define STACK_USE_MCPENC FALSE
#endif

//using MCPENC chip requires MAC
#if STACK_USE_MCPENC
   #undef STACK_USE_MAC
   #define STACK_USE_MAC TRUE
#endif

#ifndef STACK_USE_PPP
   #define STACK_USE_PPP   FALSE
#endif

#ifndef STACK_USE_SLIP
   #define STACK_USE_SLIP FALSE
#endif

#if (STACK_USE_SLIP + STACK_USE_PPP + STACK_USE_MAC)>1
   #error ONLY SPECIFY ONE MAC DRIVER (SLIP, PPP or ETHERNET)
#endif

#if !(STACK_USE_SLIP || STACK_USE_PPP || STACK_USE_MAC)
   #error PLEASE SPECIFY A MAC DRIVER
#endif

#ifndef STACK_USE_DHCP
	#define STACK_USE_DHCP	FALSE
#endif

#ifndef STACK_USE_UDP
	#define STACK_USE_UDP	FALSE
#endif

#ifndef STACK_USE_ICMP
	#define STACK_USE_ICMP	FALSE
#endif

#ifndef STACK_USE_ARP
	#define	STACK_USE_ARP	FALSE
#endif

#ifndef STACK_USE_TELNET
	#define	STACK_USE_TELNET	FALSE
#endif

#ifndef STACK_USE_HTTP
   #define  STACK_USE_HTTP FALSE
#endif

#ifndef STACK_USE_SMTP
   #define STACK_USE_SMTP  FALSE
#endif

#if (STACK_USE_ARP && STACK_USE_PPP)
 #ERROR CANNOT USE ARP WITH PPP
#ENDIF


#ifndef	STACK_USE_TCP
	#define	STACK_USE_TCP	FALSE
#endif

#ifndef STACK_USE_IP_GLEANING
	#define STACK_USE_IP_GLEANING	FALSE
#endif

/*
 * When SLIP is used, DHCP is not supported.
 */
#if STACK_USE_SLIP
	#undef STACK_USE_DHCP
	#define STACK_USE_DHCP	FALSE
#endif

/*
 * When DHCP is enabled, UDP must also be enabled.
 */
#if STACK_USE_DHCP
    #if defined(STACK_USE_UDP)
       #undef STACK_USE_UDP
    #endif
    #define STACK_USE_UDP TRUE
#endif

/*
 * When IP Gleaning is enabled, ICMP must also be enabled.
 */
#if STACK_USE_IP_GLEANING
    #if defined(STACK_USE_ICMP)
    	#undef STACK_USE_ICMP
    #endif
        #define STACK_USE_ICMP	TRUE
#endif



/*
 * DHCP requires unfragmented packet size of at least 328 bytes,
 * and while in SLIP mode, our maximum packet size is less than
 * 255.  Hence disallow DHCP module while SLIP is in use.
 * If required, one can use DHCP while SLIP is in use by modifying
 * C18 linker scipt file such that C18 compiler can allocate
 * a static array larger than 255 bytes.
 * Due to very specific application that would require this,
 * sample stack does not provide such facility.  Interested users
 * must do this on their own.
 */
#if STACK_USE_SLIP
    #if STACK_USE_DHCP
        #error DHCP cannot be used when SLIP is enabled.
    #endif
#endif

#include "drivers\hardware.h"

#define MY_MAC_BYTE1                    AppConfig.MyMACAddr.v[0]
#define MY_MAC_BYTE2                    AppConfig.MyMACAddr.v[1]
#define MY_MAC_BYTE3                    AppConfig.MyMACAddr.v[2]
#define MY_MAC_BYTE4                    AppConfig.MyMACAddr.v[3]
#define MY_MAC_BYTE5                    AppConfig.MyMACAddr.v[4]
#define MY_MAC_BYTE6                    AppConfig.MyMACAddr.v[5]

/*
 * Subnet mask for this node.
 * Must not be all zero's or else this node will never transmit
 * anything !!
 */
#define MY_MASK_BYTE1                   AppConfig.MyMask.v[0]
#define MY_MASK_BYTE2                   AppConfig.MyMask.v[1]
#define MY_MASK_BYTE3                   AppConfig.MyMask.v[2]
#define MY_MASK_BYTE4                   AppConfig.MyMask.v[3]

/*
 * Hardcoded IP address of this node
 * My IP = 10.10.5.10
 *
 * Gateway = 10.10.5.10
 */

#define MY_IP                           AppConfig.MyIPAddr

#define MY_IP_BYTE1                     AppConfig.MyIPAddr.v[0]
#define MY_IP_BYTE2                     AppConfig.MyIPAddr.v[1]
#define MY_IP_BYTE3                     AppConfig.MyIPAddr.v[2]
#define MY_IP_BYTE4                     AppConfig.MyIPAddr.v[3]

/*
 * Harcoded Gateway address for this node.
 * This should be changed to match actual network environment.
 */
#define MY_GATE_BYTE1                   AppConfig.MyGateway.v[0]
#define MY_GATE_BYTE2                   AppConfig.MyGateway.v[1]
#define MY_GATE_BYTE3                   AppConfig.MyGateway.v[2]
#define MY_GATE_BYTE4                   AppConfig.MyGateway.v[3]

#ifndef MAX_SOCKETS
#define MAX_SOCKETS                     5
#endif

#ifndef MAX_UDP_SOCKETS
#define MAX_UDP_SOCKETS                 2
#endif

#if (MAX_SOCKETS <= 0 || MAX_SOCKETS > 255)
#error Invalid MAX_SOCKETS value specified.
#endif

#if (MAX_UDP_SOCKETS <= 0 || MAX_UDP_SOCKETS > 255 )&&STACK_USE_UDP
#error Invlaid MAX_UDP_SOCKETS value specified
#endif



#if (MAC_TX_BUFFER_SIZE <= 0 || MAC_TX_BUFFER_SIZE > 1500 )
#error Invalid MAC_TX_BUFFER_SIZE value specified.
#endif

#if ( (MAC_TX_BUFFER_SIZE * MAC_TX_BUFFER_COUNT) > (4* 1024) )
#error Not enough room for Receive buffer.
#endif


#if STACK_USE_DHCP
    #if (MAX_UDP_SOCKETS < 1)
        #error Set MAX_UDP_SOCKETS to at least one.
    #endif
#endif


typedef union _SWORD_VAL
{
    int32 Val;
    struct
    {
        int8 LSB;
        int8 MSB;
        int8 USB;
    } bytes;
} SWORD_VAL;


typedef union _WORD_VAL
{
    int16 Val;
    int8 v[2];
} WORD_VAL;

typedef union _DWORD_VAL
{
    int32 Val;
    int8 v[4];
} DWORD_VAL;
#define LOWER_LSB(a)    (a).v[0]
#define LOWER_MSB(a)   (a).v[1]
#define UPPER_LSB(a)    (a).v[2]
#define UPPER_MSB(a)    (a).v[3]

typedef BYTE BUFFER;

typedef struct _MAC_ADDR
{
    int8 v[6];
} MAC_ADDR;

typedef union _IP_ADDR
{
    int8        v[4];
    int32       Val;
} IP_ADDR;


typedef struct _NODE_INFO
{
    MAC_ADDR    MACAddr;
    IP_ADDR     IPAddr;
} NODE_INFO;

typedef struct _APP_CONFIG
{
    IP_ADDR     MyIPAddr;
    MAC_ADDR    MyMACAddr;
    IP_ADDR     MyMask;
    IP_ADDR     MyGateway;
    WORD_VAL    SerialNumber;
    IP_ADDR     SMTPServerAddr;     // Not used.
    struct
    {
        int1 bIsDHCPEnabled : 1;
    } Flags;
} APP_CONFIG;


typedef union _STACK_FLAGS
{
    struct
    {
        int1 bInConfigMode : 1;
    } bits;
    int8 Val;
} STACK_FLAGS;

APP_CONFIG AppConfig;

#if STACK_USE_IP_GLEANING || STACK_USE_DHCP
    #define StackIsInConfigMode()   (stackFlags.bits.bInConfigMode)
#else
    #define StackIsInConfigMode()   FALSE
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
void StackInit(void);


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
void StackTask(void);


#endif
