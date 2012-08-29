/*********************************************************************
 *
 *                  TCP Module Defs for Microchip TCP/IP Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        TCP.h
 * Dependencies:    StackTsk.h
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
 * Nilesh Rajbharti     5/8/01   Original        (Rev 1.0)
 * Nilesh Rajbharti     5/22/02  Rev 2.0 (See version.log for detail)
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 ********************************************************************/

#ifndef TCP_H
#define TCP_H

#include "drivers\stacktsk.h"
#include "drivers\tick.h"

typedef int8 TCP_SOCKET;
typedef int16 TCP_PORT;

#ifndef TCP_NO_WAIT_FOR_ACK
   #define TCP_NO_WAIT_FOR_ACK   TRUE
#endif

/*
 * Maximum number of times a connection be retried before
 * closing it down.
 */
#define MAX_RETRY_COUNTS    3

#define INVALID_SOCKET      0xfe
#define UNKNOWN_SOCKET      0xff

#define REMOTE_HOST(s)      TCB[s].remote

/*
 * TCP States as defined by rfc793
 */
typedef enum _TCP_STATE
{
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_RCVD,
    TCP_SYN_SENT,
    TCP_EST,
    TCP_FIN_WAIT_1,
    TCP_CLOSING,
    TCP_TIMED_WAIT,
    TCP_SEND_RST,
    TCP_DATA_READY,
    TCP_LAST_ACK,
    TCP_CLOSE,
    TCP_INVALID,
    TCP_CLOSE_WAIT
} TCP_STATE;

/*
 * Socket info.
 * Union is used to create anonymous structure members.
 */
typedef struct _SOCKET_INFO
{
    TCP_STATE smState;

    NODE_INFO remote;
    TCP_PORT localPort;
    TCP_PORT remotePort;

    BUFFER TxBuffer;
    int16 TxCount;
    int16 RxCount;

    int32 SND_SEQ;
    int32 SND_ACK;

    int8 RetryCount;
    TICKTYPE startTick;
    TICKTYPE TimeOut;

    struct
    {
        int1 bServer        : 1;
        int1 bIsPutReady    : 1;
        int1 bFirstRead     : 1;
        int1 bIsGetReady    : 1;
        int1 bIsTxInProgress : 1;
    } Flags;

} SOCKET_INFO;

/*********************************************************************
 * Function:        void TCPInit(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          TCP is initialized.
 *
 * Side Effects:    None
 *
 * Overview:        Initialize all socket info.
 *
 * Note:            This function is called only one during lifetime
 *                  of the application.
 ********************************************************************/
void        TCPInit(void);



/*********************************************************************
 * Function:        TCP_SOCKET TCPListen(TCP_PORT port)
 *
 * PreCondition:    TCPInit() is already called.
 *
 * Input:           port    - A TCP port to be opened.
 *
 * Output:          Given port is opened and returned on success
 *                  INVALID_SOCKET if no more sockets left.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
TCP_SOCKET  TCPListen(TCP_PORT port);



/*********************************************************************
 * Function:        TCP_SOCKET TCPConnect(NODE_INFO* remote,
 *                                      TCP_PORT remotePort)
 *
 * PreCondition:    TCPInit() is already called.
 *
 * Input:           remote      - Remote node address info
 *                  remotePort  - remote port to be connected.
 *
 * Output:          A new socket is created, connection request is
 *                  sent and socket handle is returned.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 ********************************************************************/
TCP_SOCKET TCPConnect(NODE_INFO *remote, TCP_PORT remotePort);


/*********************************************************************
 * Function:        BOOL TCPIsConnected(TCP_SOCKET s)
 *
 * PreCondition:    TCPInit() is already called.
 *
 * Input:           s       - Socket to be checked for connection.
 *
 * Output:          TRUE    if given socket is connected
 *                  FALSE   if given socket is not connected.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            A socket is said to be connected if it is not
 *                  in LISTEN and CLOSED mode.  Socket may be in
 *                  SYN_RCVD or FIN_WAIT_1 and may contain socket
 *                  data.
 ********************************************************************/
int1        TCPIsConnected(TCP_SOCKET s);


/*********************************************************************
 * Function:        void TCPDisconnect(TCP_SOCKET s)
 *
 * PreCondition:    TCPInit() is already called     AND
 *                  TCPIsPutReady(s) == TRUE
 *
 * Input:           s       - Socket to be disconnected.
 *
 * Output:          A disconnect request is sent for given socket.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void        TCPDisconnect(TCP_SOCKET s);


/*********************************************************************
 * Function:        BOOL TCPIsPutReady(TCP_SOCKET s)
 *
 * PreCondition:    TCPInit() is already called.
 *
 * Input:           s       - socket to test
 *
 * Output:          TRUE if socket 's' is free to transmit
 *                  FALSE if socket 's' is not free to transmit.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            Each socket maintains only transmit buffer.
 *                  Hence until a data packet is acknowledeged by
 *                  remote node, socket will not be ready for
 *                  next transmission.
 *                  All control transmission such as Connect,
 *                  Disconnect do not consume/reserve any transmit
 *                  buffer.
 ********************************************************************/
int1        TCPIsPutReady(TCP_SOCKET s);


/*********************************************************************
 * Function:        BOOL TCPPut(TCP_SOCKET s, BYTE byte)
 *
 * PreCondition:    TCPIsPutReady() == TRUE
 *
 * Input:           s       - socket to use
 *                  byte    - a data byte to send
 *
 * Output:          TRUE if given byte was put in transmit buffer
 *                  FALSE if transmit buffer is full.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int1        TCPPut(TCP_SOCKET s, int8 data);


/*********************************************************************
 * Function:        BOOL TCPFlush(TCP_SOCKET s)
 *
 * PreCondition:    TCPInit() is already called.
 *
 * Input:           s       - Socket whose data is to be transmitted.
 *
 * Output:          All and any data associated with this socket
 *                  is marked as ready for transmission.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int1        TCPFlush(TCP_SOCKET s);

/*********************************************************************
 * Function:        BOOL TCPIsGetReady(TCP_SOCKET s)
 *
 * PreCondition:    TCPInit() is already called.
 *
 * Input:           s       - socket to test
 *
 * Output:          TRUE if socket 's' contains any data.
 *                  FALSE if socket 's' does not contain any data.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int1        TCPIsGetReady(TCP_SOCKET s);


/*********************************************************************
 * Function:        BOOL TCPGet(TCP_SOCKET s, BYTE *byte)
 *
 * PreCondition:    TCPInit() is already called     AND
 *                  TCPIsGetReady(s) == TRUE
 *
 * Input:           s       - socket
 *                  byte    - Pointer to a byte.
 *
 * Output:          TRUE if a byte was read.
 *                  FALSE if byte was not read.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int1        TCPGet(TCP_SOCKET s, int8 *data);


/*********************************************************************
 * Function:        WORD TCPGetArray(TCP_SOCKET s, BYTE *buffer,
 *                                      WORD count)
 *
 * PreCondition:    TCPInit() is already called     AND
 *                  TCPIsGetReady(s) == TRUE
 *
 * Input:           s       - socket
 *                  buffer  - Buffer to hold received data.
 *                  count   - Buffer length
 *
 * Output:          Number of bytes loaded into buffer.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int16        TCPGetArray(TCP_SOCKET s, int8 *buff, int16 count);


/*********************************************************************
 * Function:        BOOL TCPDiscard(TCP_SOCKET s)
 *
 * PreCondition:    TCPInit() is already called.
 *
 * Input:           s       - socket
 *
 * Output:          TRUE if socket received data was discarded
 *                  FALSE if socket received data was already
 *                          discarded.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int1        TCPDiscard(TCP_SOCKET s);

/*********************************************************************
 * Function:        BOOL TCPProcess(NODE_INFO* remote,
 *                                  WORD len)
 *
 * PreCondition:    TCPInit() is already called     AND
 *                  TCP segment is ready in MAC buffer
 *
 * Input:           remote      - Remote node info
 *                  len         - Total length of TCP semgent.
 *
 * Output:          TRUE if this function has completed its task
 *                  FALSE otherwise
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
int1        TCPProcess(NODE_INFO *remote, int16 len);


/*********************************************************************
 * Function:        void TCPTick(void)
 *
 * PreCondition:    TCPInit() is already called.
 *
 * Input:           None
 *
 * Output:          Each socket FSM is executed for any timeout
 *                  situation.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void        TCPTick(void);


#endif
