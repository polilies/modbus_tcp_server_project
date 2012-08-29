/*********************************************************************
 *
 *                  TCP Module for Microchip TCP/IP Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        TCP.C
 * Dependencies:    string.h
 *                  StackTsk.h
 *                  Helpers.h
 *                  IP.h
 *                  MAC.h
 *                  ARP.h
 *                  Tick.h
 *                  TCP.h
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
 * Nilesh Rajbharti     11/1/02 Fixed TCPTick() SYN Retry bug.
 * Darren Rook (CCS)    06/28/04  Applied 2.20 fix (above)
 * Darren Rook (CCS)    06/30/04 TCPTick() will not time out an establisehd socket if that socket is in server mode.
 * Darren Rook (CCS)    07/02/04 A bug fix for change made on 06/28/04
 * Darren Rook (CCS)    07/12/04 TCPConnect() will set StartTick to fix a bug with timeout
 * Darren Rook (CCS)    07/12/04 TCPInit() attempts to make _NextPort a random number
 ********************************************************************/
#include <string.h>

#include "drivers\stacktsk.h"
#include "drivers\helpers.h"
#include "drivers\ip.h"
#include "drivers\tick.h"
#include "drivers\tcp.h"


/*
 * Max TCP data length is MAC_TX_BUFFER_SIZE - sizeof(TCP_HEADER) -
 * sizeof(IP_HEADER) - sizeof(ETHER_HEADER)
 */
#define MAX_TCP_DATA_LEN    (MAC_TX_BUFFER_SIZE - 54)


/*
 * TCP Timeout value to begin with.
 */
#define TCP_START_TIMEOUT_VAL   (TICKS_PER_SECOND * (TICKTYPE)60)

/*
 * TCP Flags as defined by rfc793
 */
#define FIN     0x01
#define SYN     0x02
#define RST     0x04
#define PSH     0x08
#define ACK     0x10
#define URG     0x20


/*
 * TCP Header def. as per rfc 793.
 */

typedef struct _TCP_HEADER
{
    int16    SourcePort;
    int16    DestPort;
    int32   SeqNumber;
    int32   AckNumber;

    struct
    {
       int8 Reserved3:4;
       int8 Val:4;
    } DataOffset;


    union
    {
        struct
        {
             int1 flagFIN    : 1;
             int1 flagSYN    : 1;
             int1 flagRST    : 1;
             int1 flagPSH    : 1;
             int1 flagACK    : 1;
             int1 flagURG    : 1;
             int1 Reserved2  : 2;
        } bits;
        int8 b;
    } Flags;

    int16    Window;
    int16    Checksum;
    int16    UrgentPointer;

} TCP_HEADER;


/*
 * TCP Options as defined by rfc 793
 */
#define TCP_OPTIONS_END_OF_LIST     0x00
#define TCP_OPTIONS_NO_OP           0x01
#define TCP_OPTIONS_MAX_SEG_SIZE    0x02
typedef struct _TCP_OPTIONS
{
    int8        Kind;
    int8        Length;
    WORD_VAL    MaxSegSize;
} TCP_OPTIONS;

#define SwapPseudoTCPHeader(h)  h.TCPLength = swaps(h.TCPLength)

/*
 * Pseudo header as defined by rfc 793.
 */
typedef struct _PSEUDO_HEADER
{
    IP_ADDR SourceAddress;
    IP_ADDR DestAddress;
    int8 Zero;
    int8 Protocol;
    int16 TCPLength;
} PSEUDO_HEADER;

/*
 * These are all sockets supported by this TCP.
 */
SOCKET_INFO TCB[MAX_SOCKETS];

/*
 * Local temp port numbers.
 */
int16 _NextPort;

/*
 * Starting segment sequence number for each new connection.
 */
int32 ISS;


void    HandleTCPSeg(TCP_SOCKET s,
                               NODE_INFO *remote,
                               TCP_HEADER *h,
                               int16 len);

void TransmitTCP(NODE_INFO *remote,
                        TCP_PORT localPort,
                        TCP_PORT remotePort,
                        int32 tseq,
                        int32 tack,
                        int8 flags,
                        BUFFER buff,
                        int16 len);

TCP_SOCKET FindMatching_TCP_Socket(TCP_HEADER *h,
                                    NODE_INFO *remote);
void    SwapTCPHeader(TCP_HEADER* header);
int16 CalcTCPChecksum(int16 len);
void CloseSocket(SOCKET_INFO* ps);

//#define SendTCP(remote, localPort, remotePort, seq, ack, flags)     \
//        TransmitTCP(remote, localPort, remotePort, seq, ack, flags, \
//                    INVALID_BUFFER, 0)

#define LOCAL_PORT_START_NUMBER 1024
#define LOCAL_PORT_END_NUMBER   5000



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
int16 TCPInit_RandSeed;
void TCPInit(void)
{
    TCP_SOCKET s;
    SOCKET_INFO* ps;


    // Initialize all sockets.
    for ( s = 0; s < MAX_SOCKETS; s++ )
    {
        ps = &TCB[s];

        ps->smState             = TCP_CLOSED;
        ps->Flags.bServer       = FALSE;
        ps->Flags.bIsPutReady   = TRUE;
        ps->Flags.bFirstRead    = TRUE;
        ps->Flags.bIsTxInProgress = FALSE;
        ps->Flags.bIsGetReady   = FALSE;
        ps->TxBuffer            = INVALID_BUFFER;
        ps->TimeOut             = TCP_START_TIMEOUT_VAL;
    }

    //_NextPort = LOCAL_PORT_START_NUMBER;
    #if getenv("TIMER0")
    TCPInit_RandSeed+=get_timer0();
    #endif
    #if getenv("TIMER1")
     TCPInit_RandSeed+=get_timer1();
    #endif
    #if getenv("TIMER2")
     TCPInit_RandSeed+=get_timer2();
    #endif
    #if getenv("TIMER3")
     TCPInit_RandSeed+=get_timer3();
    #endif
    #if getenv("TIMER4")
     TCPInit_RandSeed+=get_timer4();
    #endif
    #if getenv("TIMER5")
     TCPInit_RandSeed+=get_timer5();
    #endif
    srand(TCPInit_RandSeed);
    _NextPort=rand();
    _NextPort+=LOCAL_PORT_START_NUMBER;
    while (_NextPort >= LOCAL_PORT_END_NUMBER) {_NextPort-=LOCAL_PORT_END_NUMBER;}
    if (_NextPort < LOCAL_PORT_START_NUMBER) {_NextPort+=LOCAL_PORT_START_NUMBER;}

    ISS = 0;
}



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
TCP_SOCKET TCPListen(TCP_PORT port)
{
    TCP_SOCKET s;
    SOCKET_INFO* ps;

    for ( s = 0; s < MAX_SOCKETS; s++ )
    {
        ps = &TCB[s];

        if ( ps->smState == TCP_CLOSED )
        {
            /*
             * We have a CLOSED socket.
             * Initialize it with LISTENing state info.
             */
            ps->smState             = TCP_LISTEN;
            ps->localPort           = port;
            ps->remotePort          = 0;

            /*
             * There is no remote node IP address info yet.
             */
            ps->remote.IPAddr.Val   = 0x00;

            /*
             * If a socket is listened on, it is a SERVER.
             */
            ps->Flags.bServer       = TRUE;

            ps->Flags.bIsGetReady   = FALSE;
            ps->TxBuffer            = INVALID_BUFFER;
            ps->Flags.bIsPutReady   = TRUE;

            return s;
        }
    }
    return INVALID_SOCKET;
}



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
TCP_SOCKET TCPConnect(NODE_INFO *remote, TCP_PORT remotePort)
{
    TCP_SOCKET s;
    SOCKET_INFO* ps;
    int1 lbFound = FALSE;


    /*
     * Find an available socket
     */
    for ( s = 0; s < MAX_SOCKETS; s++ )
    {
        ps = &TCB[s];
        if ( ps->smState == TCP_CLOSED )
        {
            lbFound = TRUE;
            break;
        }
    }

    /*
     * If there is no socket available, return error.
     */
    if ( lbFound == FALSE )
        return INVALID_SOCKET;

    /*
     * Each new socket that is opened by this node, gets
     * next sequential port number.
     */
    ps->localPort = ++_NextPort;
    if ( _NextPort > LOCAL_PORT_END_NUMBER )
        _NextPort = LOCAL_PORT_START_NUMBER;

    /*
     * This is a client socket.
     */
    ps->Flags.bServer = FALSE;

    /*
     * This is the port, we are trying to connect to.
     */
    ps->remotePort = remotePort;

    /*
     * Each new socket that is opened by this node, will
     * start with next segment seqeuence number.
     */
    ps->SND_SEQ = ++ISS;
    ps->SND_ACK = 0;

//    memcpy((int8*)&ps->remote, (int8*)remote, sizeof(ps->remote));
    memcpy(&ps->remote, remote, sizeof(NODE_INFO));

   //dsr add 071204
   ps->StartTick=TickGet();

   debug(debug_putc,"\r\n%LX CONNECT ",TickCount);

    /*
     * Send SYN message.
     */
    TransmitTCP(&ps->remote,
            ps->localPort,
            ps->remotePort,
            ps->SND_SEQ,
            ps->SND_ACK,
            SYN,INVALID_BUFFER, 0);

    ps->smState = TCP_SYN_SENT;
    ps->SND_SEQ+=1;

    return s;
}



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
int1 TCPIsConnected(TCP_SOCKET s)
{
    return ( TCB[s].smState == TCP_EST );
}



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
void TCPDisconnect(TCP_SOCKET s)
{
    SOCKET_INFO *ps;

    ps = &TCB[s];

    /*
     * If socket is not connected, may be it is already closed
     * or in process of closing.  Since user has called this
     * explicitly, close it forcefully.
     */
    if ( ps->smState != TCP_EST )
    {
        CloseSocket(ps);
        return;
    }


    /*
     * Discard any outstand data that is to be read.
     */
    TCPDiscard(s);

    debug(debug_putc,"\r\nDISONNECT ");

    /*
     * Send FIN message.
     */
    TransmitTCP(&ps->remote,
            ps->localPort,
            ps->remotePort,
            ps->SND_SEQ,
            ps->SND_ACK,
            FIN | ACK,INVALID_BUFFER, 0);
            
        ps->SND_SEQ+=1;

    ps->smState = TCP_FIN_WAIT_1;

    return;
}

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
int16 tempTxCount;
int32 tempSND_SEQ;
int1 TCPFlush(TCP_SOCKET s)
{
    SOCKET_INFO *ps;
    int32 seq; int16 count;

    ps = &TCB[s];


    // Make sure that this already a TxBuffer assigned to this
    // socket.
    if ( ps->TxBuffer == INVALID_BUFFER ) {
        return FALSE;
    }

    if ( ps->Flags.bIsPutReady == FALSE ) {
        return FALSE;
    }

    //darren added - seems important.
    if (TCB[s].smState != TCP_EST) {
      return FALSE;
    }

   debug(debug_putc,"\r\nFLUSH ");

    TransmitTCP(&ps->remote,
                ps->localPort,
                ps->remotePort,
                ps->SND_SEQ,
                ps->SND_ACK,
                ACK,
                ps->TxBuffer,
                ps->TxCount);
    tempSND_SEQ = ps->SND_SEQ;
    tempTxCount = ps->TxCount;


    //ps->SND_SEQ += (int32)ps->TxCount;
      seq=ps->SND_SEQ;
      count=ps->TxCount;
      seq+=count;
      ps->SND_SEQ=seq;


    tempSND_SEQ = ps->SND_SEQ;
    ps->Flags.bIsPutReady       = FALSE;
    ps->Flags.bIsTxInProgress   = FALSE;

#if TCP_NO_WAIT_FOR_ACK
    MACDiscardTx(ps->TxBuffer);
    ps->TxBuffer                = INVALID_BUFFER;
    ps->Flags.bIsPutReady       = TRUE;
#endif

    return TRUE;
}



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
int1 TCPIsPutReady(TCP_SOCKET s)
{
    if ( TCB[s].TxBuffer == INVALID_BUFFER )
        return IPIsTxReady();
    else
        return TCB[s].Flags.bIsPutReady;
}




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
int1 TCPPut(TCP_SOCKET s, int8 data)
{
    int16 tempTxCount;       // This is a fix for HITECH bug
    SOCKET_INFO* ps;

    ps = &TCB[s];

    if ( ps->TxBuffer == INVALID_BUFFER )
    {
        ps->TxBuffer = MACGetTxBuffer();

        // This function is used to transmit data only.  And every data packet
        // must be ack'ed by remote node.  Until this packet is ack'ed by
        // remote node, we must preserve its content so that we can retransmit
        // if we need to.
        MACReserveTxBuffer(ps->TxBuffer);

        ps->TxCount = 0;

        IPSetTxBuffer(ps->TxBuffer, sizeof(TCP_HEADER));
    }

    /*
     * HITECH does not seem to compare ps->TxCount as it is.
     * Copying it to local variable and then comparing seems to work.
     */
    tempTxCount = ps->TxCount;
    if ( tempTxCount >= MAX_TCP_DATA_LEN )
        return FALSE;

    ps->Flags.bIsTxInProgress = TRUE;

    MACPut(data);

    // REMOVE
    //tempTxCount = ps->TxCount;
    tempTxCount++;
    ps->TxCount = tempTxCount;

    //ps->TxCount++;
    //tempTxCount = ps->TxCount;
    if ( tempTxCount >= MAX_TCP_DATA_LEN )
        return(TCPFlush(s));
    //if ( TCB[s].TxCount >= MAX_TCP_DATA_LEN )
    //    TCPFlush(s);

    return TRUE;
}

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
int1 TCPPutArray(TCP_SOCKET s, char *data, int16 size)
{
    int16 tempTxCount;
    SOCKET_INFO* ps;

    ps = &TCB[s];

    if ( ps->TxBuffer == INVALID_BUFFER )
    {
        ps->TxBuffer = MACGetTxBuffer();

        MACReserveTxBuffer(ps->TxBuffer);

        ps->TxCount = 0;

        IPSetTxBuffer(ps->TxBuffer, sizeof(TCP_HEADER));
    }

    tempTxCount = ps->TxCount;
    if ( tempTxCount >= MAX_TCP_DATA_LEN )
        return FALSE;

    ps->Flags.bIsTxInProgress = TRUE;

   while((size--)&&(tempTxCount < MAX_TCP_DATA_LEN)) {

    MACPut(*data);
    data++;

    tempTxCount++;
   }

    ps->TxCount = tempTxCount;

    if ( tempTxCount >= MAX_TCP_DATA_LEN )
        return(TCPFlush(s));

    return TRUE;
}



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
int1 TCPDiscard(TCP_SOCKET s)
{
    SOCKET_INFO* ps;

    ps = &TCB[s];

    /*
     * This socket must contain data for it to be discarded.
     */
    if ( !ps->Flags.bIsGetReady )
        return FALSE;

    MACDiscardRx();
    ps->Flags.bIsGetReady = FALSE;

    return TRUE;
}




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
int16 TCPGetArray(TCP_SOCKET s, int8 *buff, int16 count)
{
    SOCKET_INFO *ps;

    ps = &TCB[s];

    if ( ps->Flags.bIsGetReady )
    {
        if ( ps->Flags.bFirstRead )
        {
            // Position read pointer to begining of correct
            // buffer.
            IPSetRxBuffer(sizeof(TCP_HEADER));

            ps->Flags.bFirstRead = FALSE;
        }

        ps->Flags.bIsTxInProgress = TRUE;

        return MACGetArray(buff, count);
    }
    else
        return 0;
}



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
int1 TCPGet(TCP_SOCKET s, int8 *data)
{
    SOCKET_INFO* ps;

    ps = &TCB[s];

    if ( ps->Flags.bIsGetReady )
    {
        if ( ps->Flags.bFirstRead )
        {
            // Position read pointer to begining of correct
            // buffer.
            IPSetRxBuffer(sizeof(TCP_HEADER));

            ps->Flags.bFirstRead = FALSE;

        }

        if ( ps->RxCount == 0 )
        {
            MACDiscardRx();
            ps->Flags.bIsGetReady = FALSE;
            return FALSE;
        }

        ps->RxCount--;
        *data = MACGet();
        return TRUE;
    }
    return FALSE;
}



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
int1 TCPIsGetReady(TCP_SOCKET s)
{
    /*
     * A socket is said to be "Get" ready when it has already
     * received some data.  Sometime, a socket may be closed,
     * but it still may contain data.  Thus in order to ensure
     * reuse of a socket, caller must make sure that it reads
     * a socket, if is ready.
     */
    return (TCB[s].Flags.bIsGetReady );
}



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
void TCPTick(void)
{
    TCP_SOCKET s;
    TICKTYPE diffTicks;
    TICKTYPE tick;
    SOCKET_INFO* ps;
    int32 seq;
    int8 flags;

    flags = 0x00;
    /*
     * Periodically all "not closed" sockets must perform timed operation.
     */
    for ( s = 0; s < MAX_SOCKETS; s++ )
    {
        ps = &TCB[s];

        if ( ps->Flags.bIsGetReady || ps->Flags.bIsTxInProgress )
            continue;

        /*
         * Closed or Passively Listening socket do not care
         * about timeout conditions.
         */
        if ( (ps->smState == TCP_CLOSED) ||
             (ps->smState == TCP_LISTEN &&
              ps->Flags.bServer == TRUE) )
            continue;

         //DSR ADD 063004
         if ( (ps->smState == TCP_EST) && (ps->Flags.bServer == TRUE) )
            continue;

        tick = TickGet();

        debug(debug_putc,"\r\nTICK CHK t=%LX s=%LX o=%LX",tick,ps->startTick, ps->TimeOut);

        // Calculate timeout value for this socket.
        diffTicks = TickGetDiff(tick, ps->startTick);

        // If timeout has not occured, do not do anything.
        if ( diffTicks <= ps->TimeOut )
            continue;

        /*
         * All states require retransmission, so check for transmitter
         * availability right here - common for all.
         */
        if ( !IPIsTxReady() )
            return;

        // Restart timeout reference.
        ps->startTick = TickGet();

        // Update timeout value if there is need to wait longer.
        ps->TimeOut+=1;

        // This will be one more attempt.
        ps->RetryCount+=1;

        /*
         * A timeout has occured.  Respond to this timeout condition
         * depending on what state this socket is in.
         */
        switch(ps->smState)
        {
        case TCP_SYN_SENT:
            /*
             * Keep sending SYN until we hear from remote node.
             * This may be for infinite time, in that case
             * caller must detect it and do something.
             * Bug Fix: 11/1/02
             */
            flags = SYN;
            break;

        case TCP_SYN_RCVD:
            /*
             * We must receive ACK before timeout expires.
             * If not, resend SYN+ACK.
             * Abort, if maximum attempts counts are reached.
             */
            if ( ps->RetryCount < MAX_RETRY_COUNTS )
            {
                flags = SYN | ACK;
            }
            else
                CloseSocket(ps);
            break;

        case TCP_EST:
#if !TCP_NO_WAIT_FOR_ACK
            /*
             * Don't let this connection idle for very long time.
             * If we did not receive or send any message before timeout
             * expires, close this connection.
             */
            if ( ps->RetryCount <= MAX_RETRY_COUNTS )
            {
                if ( ps->TxBuffer != INVALID_BUFFER )
                {
                    MACSetTxBuffer(ps->TxBuffer, 0);
                    MACFlush();
                }
                else
                    flags = ACK;
            }
            else
            {
                // Forget about previous transmission.
                if ( ps->TxBuffer != INVALID_BUFFER )
                    MACDiscardTx(ps->TxBuffer);
                ps->TxBuffer = INVALID_BUFFER;

#endif
                // Request closure.
                flags = FIN | ACK;

                ps->smState = TCP_FIN_WAIT_1;
#if !TCP_NO_WAIT_FOR_ACK
            }
#endif
            break;

        case TCP_FIN_WAIT_1:
        case TCP_LAST_ACK:
            /*
             * If we do not receive any response to out close request,
             * close it outselves.
             */
            if ( ps->RetryCount > MAX_RETRY_COUNTS )
                CloseSocket(ps);
            else
                flags = FIN;
            break;

        case TCP_CLOSING:
        case TCP_TIMED_WAIT:
            /*
             * If we do not receive any response to out close request,
             * close it outselves.
             */
            if ( ps->RetryCount > MAX_RETRY_COUNTS )
                CloseSocket(ps);
            else
                flags = ACK;
            break;
        }

        if ( flags > 0x00 )
        {
            if ( flags != ACK ) {
                seq = ps->SND_SEQ;
                ps->SND_SEQ += 1;
            }
            else
                seq = ps->SND_SEQ;

            debug(debug_putc,"\r\n%LX TICK ",TickCount);

            TransmitTCP(&ps->remote,
                    ps->localPort,
                    ps->remotePort,
                    seq,
                    ps->SND_ACK,
                    flags,
                    INVALID_BUFFER,
                    0);
        }
    }
}



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
int1 TCPProcess(NODE_INFO *remote, int16 len)
{
    TCP_HEADER      TCPHeader;
    PSEUDO_HEADER   pseudoHeader;
    TCP_SOCKET      socket;
    WORD_VAL        checksum;
    int8            optionsSize;

    int8 i;
    SOCKET_INFO *ps;

    for (i=0;i<MAX_SOCKETS;i++) {
       ps = &TCB[i];
       debug(debug_putc,"\r\nS%U LP:%LU RP:%LU ST:%U TC:%LU RC:%LU ",
         i,
         ps->localPort,
         ps->remotePort,
         ps->smState,
         ps->TxCount,
         ps->RxCount);
       debug(debug_putc,"TB:%U RE:%U SS:%LX SA:%LX %U%U%U%U%U",
         ps->TxBuffer,
         ps->RetryCount,
         ps->SND_SEQ,
         ps->SND_ACK,
         ps->Flags.bServer,
         ps->Flags.bIsPutReady,
         ps->Flags.bFirstRead,
         ps->Flags.bIsGetReady,
         ps->Flags.bIsTxInProgress);
    }

    // Retrieve TCP header.
    //MACGetArray((int8*)&TCPHeader, sizeof(TCPHeader));
    MACGetArray(&TCPHeader, sizeof(TCP_HEADER));

    SwapTCPHeader(&TCPHeader);

    debug(debug_putc,"\r\n%LX TCP GOT: SP:%LU DP:%LU DO:%U %U%U%U%U%U%U ",TickCount,
    TCPHeader.SourcePort,TCPHeader.DestPort,TCPHeader.DataOffset.Val,
    TCPHeader.Flags.bits.flagFIN,TCPHeader.Flags.bits.flagSYN,
    TCPHeader.Flags.bits.flagRST,TCPHeader.Flags.bits.flagPSH,
    TCPHeader.Flags.bits.flagACK,TCPHeader.Flags.bits.flagURG);
    debug(debug_putc,"L:%LU W:%LU U:%LX SN:%LX AN:%LX CS:%LX",
    len,
    TCPHeader.Window,TCPHeader.UrgentPointer,
    TCPHeader.SeqNumber,
    TCPHeader.AckNumber,TCPHeader.Checksum);

    // Calculate IP pseudoheader checksum.
    pseudoHeader.SourceAddress      = remote->IPAddr;
    pseudoHeader.DestAddress.v[0]   = MY_IP_BYTE1;
    pseudoHeader.DestAddress.v[1]   = MY_IP_BYTE2;
    pseudoHeader.DestAddress.v[2]   = MY_IP_BYTE3;
    pseudoHeader.DestAddress.v[3]   = MY_IP_BYTE4;
    pseudoHeader.Zero               = 0x0;
    pseudoHeader.Protocol           = IP_PROT_TCP;
    pseudoHeader.TCPLength          = len;

    SwapPseudoTCPHeader(pseudoHeader);


    //checksum.Val = ~CalcIPChecksum((int8*)&pseudoHeader,sizeof(pseudoHeader));
    checksum.Val = ~CalcIPChecksum(&pseudoHeader,sizeof(PSEUDO_HEADER));


    // Set TCP packet checksum = pseudo header checksum in MAC RAM.
    IPSetRxBuffer(16);
    MACPut(checksum.v[0]);
    MACPut(checksum.v[1]);
    IPSetRxBuffer(0);

    // Now calculate TCP packet checksum in NIC RAM - including
    // pesudo header.
    checksum.Val = CalcTCPChecksum(len);

    if ( checksum.Val != TCPHeader.Checksum )
    {
        debug(debug_putc," BAD CS (%LX != %LX)", checksum.Val,TCPHeader.Checksum);
        MACDiscardRx();
        return TRUE;
    }

    // Skip over options and retrieve all data bytes.
    optionsSize = (int8)((TCPHeader.DataOffset.Val << 2)-sizeof(TCP_HEADER));
    len = len - optionsSize - sizeof(TCP_HEADER);

    // Position packet read pointer to start of data area.
    IPSetRxBuffer((TCPHeader.DataOffset.Val << 2));

    // Find matching socket.
    socket = FindMatching_TCP_Socket(&TCPHeader, remote);
    if ( socket < INVALID_SOCKET )
    {
        debug(debug_putc," MATCHES S%U",socket);
        HandleTCPSeg(socket, remote, &TCPHeader, len);
    }
    else
    {
        /*
         * If this is an unknown socket, discard it and
         * send RESET to remote node.
         */
        debug(debug_putc," NO MATCH",socket);
        MACDiscardRx();

        if ( socket == UNKNOWN_SOCKET )
        {

            TCPHeader.AckNumber += len;
            if ( TCPHeader.Flags.bits.flagSYN ||
                 TCPHeader.Flags.bits.flagFIN )
                TCPHeader.AckNumber++;

            debug(debug_putc,"\r\nPROCESS ");

            TransmitTCP(remote,
                    TCPHeader.DestPort,
                    TCPHeader.SourcePort,
                    TCPHeader.AckNumber,
                    TCPHeader.SeqNumber,
                    RST,INVALID_BUFFER, 0);
        }

    }

    return TRUE;
}


/*********************************************************************
 * Function:        static void TransmitTCP(NODE_INFO* remote
 *                                          TCP_PORT localPort,
 *                                          TCP_PORT remotePort,
 *                                          DWORD seq,
 *                                          DWORD ack,
 *                                          BYTE flags,
 *                                          BUFFER buffer,
 *                                          WORD len)
 *
 * PreCondition:    TCPInit() is already called     AND
 *                  TCPIsPutReady() == TRUE
 *
 * Input:           remote      - Remote node info
 *                  localPort   - Source port number
 *                  remotePort  - Destination port number
 *                  seq         - Segment sequence number
 *                  ack         - Segment acknowledge number
 *                  flags       - Segment flags
 *                  buffer      - Buffer to which this segment
 *                                is to be transmitted
 *                  len         - Total data length for this segment.
 *
 * Output:          A TCP segment is assembled and put to transmit.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void TransmitTCP(NODE_INFO *remote,
                        TCP_PORT localPort,
                        TCP_PORT remotePort,
                        int32 tseq,
                        int32 tack,
                        int8 flags,
                        BUFFER buff,
                        int16 len)
{
    WORD_VAL        checkSum;
    TCP_HEADER      header;
    TCP_OPTIONS     options;
    PSEUDO_HEADER   pseudoHeader;

    /*
     * When using SLIP (USART), packet transmission takes some time
     * and hence before sending another packet, we must make sure
     * that, last packet is transmitted.
     * When using ethernet controller, transmission occurs very fast
     * and by the time next packet is transmitted, previous is
     * transmitted and we do not need to check for last packet.
     */
    while( !IPIsTxReady() );

    header.SourcePort           = localPort;
    header.DestPort             = remotePort;
    header.SeqNumber            = tseq;
    header.AckNumber            = tack;
    header.Flags.bits.Reserved2 = 0;
    header.DataOffset.Reserved3 = 0;
    header.Flags.b              = flags;
    // Receive window = MAC Free buffer size - TCP header (20) - IP header (20)
    //                  - ETHERNET header (14 if using NIC) .
    header.Window               = MACGetFreeRxSize();
#if !STACK_USE_SLIP
    /*
     * Limit one segment at a time from remote host.
     * This limit increases overall throughput as remote host does not
     * flood us with packets and later retry with significant delay.
     */
    if ( header.Window >= MAC_RX_BUFFER_SIZE )
        header.Window = MAC_RX_BUFFER_SIZE;

    else if ( header.Window > 54 )
    {
        header.Window -= 54;
    }
    else
        header.Window = 0;
#else
    if ( header.Window > 40 )
    {
        header.Window -= 40;
    }
    else
        header.Window = 0;
#endif

    header.Checksum             = 0;
    header.UrgentPointer        = 0;
       debug(debug_putc," TCP PUT: SP:%LU DP:%LU DO:%U %U%U%U%U%U%U W:%LU U:%LX SN:%LX AN:%LX",
       header.SourcePort,header.DestPort,header.DataOffset.Val,
       header.Flags.bits.flagFIN,header.Flags.bits.flagSYN,
       header.Flags.bits.flagRST,header.Flags.bits.flagPSH,
       header.Flags.bits.flagACK,header.Flags.bits.flagURG,
       header.Window,header.UrgentPointer,
       header.SeqNumber,
       header.AckNumber
       );

    SwapTCPHeader(&header);

    len += sizeof(TCP_HEADER);

    if ( flags & SYN )
    {
        len += sizeof(TCP_OPTIONS);
        options.Kind = TCP_OPTIONS_MAX_SEG_SIZE;
        options.Length = 0x04;

        // Load MSS in already swapped order.
        options.MaxSegSize.v[0]  = (MAC_RX_BUFFER_SIZE >> 8); // 0x05;
        options.MaxSegSize.v[1]  = (MAC_RX_BUFFER_SIZE & 0xff); // 0xb4;

        header.DataOffset.Val   = (sizeof(TCP_HEADER) + sizeof(TCP_OPTIONS)) >> 2;
    }
    else
        header.DataOffset.Val   = sizeof(TCP_HEADER) >> 2;


    // Calculate IP pseudoheader checksum.
    pseudoHeader.SourceAddress.v[0] = MY_IP_BYTE1;
    pseudoHeader.SourceAddress.v[1] = MY_IP_BYTE2;
    pseudoHeader.SourceAddress.v[2] = MY_IP_BYTE3;
    pseudoHeader.SourceAddress.v[3] = MY_IP_BYTE4;
    pseudoHeader.DestAddress    = remote->IPAddr;
    pseudoHeader.Zero           = 0x0;
    pseudoHeader.Protocol       = IP_PROT_TCP;
    pseudoHeader.TCPLength      = len;

    SwapPseudoTCPHeader(pseudoHeader);


    //header.Checksum = ~CalcIPChecksum((BYTE*)&pseudoHeader, sizeof(pseudoHeader));
    header.Checksum = ~CalcIPChecksum(&pseudoHeader, sizeof(PSEUDO_HEADER));

    checkSum.Val = header.Checksum;

    if ( buff == INVALID_BUFFER ) {
        buff = MACGetTxBuffer();
    }

    IPSetTxBuffer(buff, 0);

    // Write IP header.
    IPPutHeader(remote, IP_PROT_TCP, len);
//    IPPutArray((BYTE*)&header, sizeof(header));
    IPPutArray(&header, sizeof(TCP_HEADER));

    if ( flags & SYN ) {
//        IPPutArray((BYTE*)&options, sizeof(options));
        IPPutArray(&options, sizeof(TCP_OPTIONS));
   }

    IPSetTxBuffer(buff, 0);

    checkSum.Val = CalcTCPChecksum(len);

    // Update the checksum.
    IPSetTxBuffer(buff, 16);
    MACPut(checkSum.v[1]);
    MACPut(checkSum.v[0]);
    MACSetTxBuffer(buff, 0);

    MACFlush();
}



/*********************************************************************
 * Function:        static WORD CalcTCPChecksum(WORD len)
 *
 * PreCondition:    TCPInit() is already called     AND
 *                  MAC buffer pointer set to starting of buffer
 *
 * Input:           len     - Total number of bytes to calculate
 *                          checksum for.
 *
 * Output:          16-bit checksum as defined by rfc 793.
 *
 * Side Effects:    None
 *
 * Overview:        This function performs checksum calculation in
 *                  MAC buffer itself.
 *
 * Note:            None
 ********************************************************************/
int16 CalcTCPChecksum(int16 len)
{
    int1 lbMSB = TRUE;
    WORD_VAL checkSum;
    int8 Checkbyte;

    checkSum.Val = 0;

    while( len-- )
    {
        Checkbyte = MACGet();

        if ( !lbMSB )
        {
            if ( (checkSum.v[0] = Checkbyte+checkSum.v[0]) < Checkbyte)
            {
                if ( ++checkSum.v[1] == 0 )
                    checkSum.v[0]++;
            }
        }
        else
        {
            if ( (checkSum.v[1] = Checkbyte+checkSum.v[1]) < Checkbyte)
            {
                if ( ++checkSum.v[0] == 0 )
                    checkSum.v[1]++;
            }
        }

        lbMSB = !lbMSB;
    }

    checkSum.v[1] = ~checkSum.v[1];
    checkSum.v[0] = ~checkSum.v[0];
    return checkSum.Val;
}



/*********************************************************************
 * Function:        static TCP_SOCKET FindMatching_TCP_Socket(TCP_HEADER *h,
 *                                      NODE_INFO* remote)
 *
 * PreCondition:    TCPInit() is already called
 *
 * Input:           h           - TCP Header to be matched against.
 *                  remote      - Node who sent this header.
 *
 * Output:          A socket that matches with given header and remote
 *                  node is searched.
 *                  If such socket is found, its index is returned
 *                  else INVALID_SOCKET is returned.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
TCP_SOCKET FindMatching_TCP_Socket(TCP_HEADER *h, NODE_INFO *remote)
{
    SOCKET_INFO *ps;
    TCP_SOCKET s;
    TCP_SOCKET partialMatch;

    partialMatch = INVALID_SOCKET;

    for ( s = 0; s < MAX_SOCKETS; s++ )
    {
        ps = &TCB[s];

        if ( ps->smState != TCP_CLOSED )
        {
            if ( ps->localPort == h->DestPort )
            {
                if ( ps->smState == TCP_LISTEN )
                    partialMatch = s;

                if ( ps->remotePort == h->SourcePort &&
                     ps->remote.IPAddr.Val == remote->IPAddr.Val )
                {
                        return s;
                }
            }
        }
    }

    ps = &TCB[partialMatch];

    if ( partialMatch != INVALID_SOCKET &&
         ps->smState == TCP_LISTEN )
    {
                //DID DSR: change void * to int *
//        memcpy((int8*)&ps->remote, (int8*)remote, sizeof(*remote));
        memcpy(&ps->remote, remote, sizeof(NODE_INFO));

        //ps->remote              = *remote;
        ps->localPort           = h->DestPort;
        ps->remotePort          = h->SourcePort;
        ps->Flags.bIsGetReady   = FALSE;
        ps->TxBuffer            = INVALID_BUFFER;
        ps->Flags.bIsPutReady   = TRUE;

        return partialMatch;
    }

    if ( partialMatch == INVALID_SOCKET )
        return UNKNOWN_SOCKET;
    else
        return INVALID_SOCKET;
}






/*********************************************************************
 * Function:        static void SwapTCPHeader(TCP_HEADER* header)
 *
 * PreCondition:    None
 *
 * Input:           header      - TCP Header to be swapped.
 *
 * Output:          Given header is swapped.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void SwapTCPHeader(TCP_HEADER* header)
{
    header->SourcePort      = swaps(header->SourcePort);
    header->DestPort        = swaps(header->DestPort);
    header->SeqNumber       = swapl(header->SeqNumber);
    header->AckNumber       = swapl(header->AckNumber);
    header->Window          = swaps(header->Window);
    header->Checksum        = swaps(header->Checksum);
    header->UrgentPointer   = swaps(header->UrgentPointer);
}



/*********************************************************************
 * Function:        static void CloseSocket(SOCKET_INFO* ps)
 *
 * PreCondition:    TCPInit() is already called
 *
 * Input:           ps  - Pointer to a socket info that is to be
 *                          closed.
 *
 * Output:          Given socket information is reset and any
 *                  buffer held by this socket is discarded.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void CloseSocket(SOCKET_INFO* ps)
{
    if ( ps->TxBuffer != INVALID_BUFFER )
    {
        MACDiscardTx(ps->TxBuffer);
        ps->TxBuffer            = INVALID_BUFFER;
        ps->Flags.bIsPutReady   = TRUE;
    }

    ps->remote.IPAddr.Val = 0x00;
    ps->remotePort = 0x00;
    if ( ps->Flags.bIsGetReady )
    {
        MACDiscardRx();
    }
    ps->Flags.bIsGetReady       = FALSE;
    ps->TimeOut                 = TCP_START_TIMEOUT_VAL;

    ps->Flags.bIsTxInProgress   = FALSE;

    if ( ps->Flags.bServer )
        ps->smState = TCP_LISTEN;
    else
        ps->smState = TCP_CLOSED;
    return;
}



/*********************************************************************
 * Function:        static void HandleTCPSeg(TCP_SOCKET s,
 *                                      NODE_INFO *remote,
 *                                      TCP_HEADER* h,
 *                                      WORD len)
 *
 * PreCondition:    TCPInit() is already called     AND
 *                  TCPProcess() is the caller.
 *
 * Input:           s           - Socket that owns this segment
 *                  remote      - Remote node info
 *                  h           - TCP Header
 *                  len         - Total buffer length.
 *
 * Output:          TCP FSM is executed on given socket with
 *                  given TCP segment.
 *
 * Side Effects:    None
 *
 * Overview:        None
 *
 * Note:            None
 ********************************************************************/
void HandleTCPSeg(TCP_SOCKET s,
                        NODE_INFO *remote,
                        TCP_HEADER *h,
                        int16 len)
{
    int32 ackn;
    int32 seqn;
    int32 prevAck, prevSeq;
    SOCKET_INFO *ps;
    int8 flags = 0x00;


    ps = &TCB[s];

    /*
     * Remember current seq and ack for our connection so that if
     * we have to silently discard this packet, we can go back to
     * previous ack and seq numbers.
     */
    prevAck = ps->SND_ACK;
    prevSeq = ps->SND_SEQ;

    ackn = h->SeqNumber ;
    ackn += (int32)len;
    seqn = ps->SND_SEQ;

    /*
     * Clear retry counts and timeout tick counter.
     */
    ps->RetryCount  = 0;
    ps->startTick   = TickGet();
    ps->TimeOut = TCP_START_TIMEOUT_VAL;

    debug(debug_putc,"\r\n%LX HandleTCPSeg S%U L%LU ST%LX: ",TickCount,s,len,ps->startTick);

    if ( ps->smState == TCP_LISTEN )
    {
        debug(debug_putc,"*LISTEN* ");
        MACDiscardRx();

        ps->SND_SEQ     = ++ISS;
        ps->SND_ACK     = ++ackn;
        seqn             = ps->SND_SEQ;
        ++ps->SND_SEQ;
        if ( h->Flags.bits.flagSYN )
        {
            debug(debug_putc,"-SYN- ");

            /*
             * This socket has received connection request.
             * Remember calling node, assign next segment seq. number
             * for this potential connection.
             */
            //DID DSR: change void * to int *
//            memcpy((int8 *)&ps->remote, remote, sizeof(*remote));
            memcpy(&ps->remote, remote, sizeof(NODE_INFO));

            ps->remotePort  = h->SourcePort;

            /*
             * Grant connection request.
             */
            flags           = SYN | ACK;
            ps->smState     = TCP_SYN_RCVD;

        }
        else
        {
            /*
             * Anything other than connection request is ignored in
             * LISTEN state.
             */
            flags               = RST;
            seqn                 = ackn;
            ackn                 = h->SeqNumber;
            ps->remote.IPAddr.Val = 0x00;
        }

    }
    else
    {
         debug(debug_putc,"*NOTLISTEN*");
        /*
         * Reset FSM, if RST is received.
         */
        if ( h->Flags.bits.flagRST )
        {
            debug(debug_putc,"-RST-");
            MACDiscardRx();

            CloseSocket(ps);

            return;

        }

        else if ( seqn == h->AckNumber )
        {
           debug(debug_putc,"-SEQ/ACK MATCH- ");
            if ( ps->smState == TCP_SYN_RCVD )
            {
                debug(debug_putc,"@SYN RCVD@ ");
                if ( h->Flags.bits.flagACK )
                {
                    debug(debug_putc,"!ACK! ");
                    ps->SND_ACK = ackn;
                    ps->RetryCount = 0;
                    ps->startTick = TickGet();
                    ps->smState = TCP_EST;

                    if ( len > 0 )
                    {
                        debug(debug_putc,"LEN>0 ");
                        ps->Flags.bIsGetReady   = TRUE;
                        ps->RxCount             = len;
                        ps->Flags.bFirstRead    = TRUE;
                    }
                    else
                        MACDiscardRx();
                }
                else
                {
                   debug(debug_putc,"!NOTACK!");
                    MACDiscardRx();
                }
            }
            else if ( ps->smState == TCP_SYN_SENT )
            {
                 debug(debug_putc,"@SYN SENT@ ");
                if ( h->Flags.bits.flagSYN )
                {
                    debug(debug_putc,"!SYN! ");
                    ps->SND_ACK = ++ackn;
                    if ( h->Flags.bits.flagACK )
                    {
                        flags = ACK;
                        debug(debug_putc,"TO-EST");
                        ps->smState = TCP_EST;
                    }
                    else
                    {
                        debug(debug_putc," ACK");
                        flags = SYN | ACK;
                        ps->smState = TCP_SYN_RCVD;
                        ps->SND_SEQ = ++seqn;
                    }

                    if ( len > 0 )
                    {
                        debug(debug_putc," LEN>0");
                        ps->Flags.bIsGetReady   = TRUE;
                        ps->RxCount             = len;
                        ps->Flags.bFirstRead    = TRUE;
                    }
                    else
                        MACDiscardRx();
                }
                else
                {
                    debug(debug_putc,"CONFUSED-2");
                    MACDiscardRx();
                }
            }
            else
            {
               debug(debug_putc,"@OTHR@ ");
                if ( h->SeqNumber != ps->SND_ACK )
                {
                    // Discard buffer.
                    debug(debug_putc,"!SEQ NO MATCH! ");
                    MACDiscardRx();
                    return;
                }

                ps->SND_ACK = ackn;

                if ( ps->smState == TCP_EST )
                {
                     debug(debug_putc,"!EST! ");
                    if ( h->Flags.bits.flagACK )
                    {
                        if ( ps->TxBuffer != INVALID_BUFFER )
                        {
                            debug(debug_putc,"INV_BUFFER ");
                            MACDiscardTx(ps->TxBuffer);
                            ps->TxBuffer            = INVALID_BUFFER;
                            ps->Flags.bIsPutReady   = TRUE;
                        }
                    }

                    if ( h->Flags.bits.flagFIN )
                    {
                        debug(debug_putc,"FIN ");
                        /*
                        flags = ACK;
                        ackn = ps->SND_ACK;
                        seqn = ps->SND_SEQ;

                        ackn++;
                        ps->SND_ACK=ackn;
                        ps->smState = TCP_CLOSE_WAIT;
                        */
                        //dsr add 07/02/04
                        flags = FIN | ACK;
                        seqn = ps->SND_SEQ;
                        ps->SND_SEQ += 1;
                        ps->SND_ACK += 1;
                        ackn = ps->SND_ACK;
                        ps->smState = TCP_LAST_ACK;
                    }


                    if ( len > 0 )
                    {
                       debug(debug_putc,"!LEN>0! ");
                        if ( !ps->Flags.bIsGetReady )
                        {
                           debug(debug_putc,"NOT_GET_READY ");
                            ps->Flags.bIsGetReady   = TRUE;
                            ps->RxCount             = len;
                            ps->Flags.bFirstRead    = TRUE;

                             // 4/1/02
                            flags = ACK;
                       }
                        else
                        {
                            /*
                             * Since we cannot accept this packet,
                             * restore to previous seq and ack.
                             * and do not send anything back.
                             * Host has to resend this packet when
                             * we are ready.
                             */
                            debug(debug_putc,"GET_READY ");
                            flags = 0x00;
                            ps->SND_SEQ = prevSeq;
                            ps->SND_ACK = prevAck;

                            MACDiscardRx();
                        }
                    }
                    else
                    {
                       debug(debug_putc,"CONFUSED3 ");
                        MACDiscardRx();
                    }

                }
                else if ( ps->smState == TCP_LAST_ACK )
                {
                   debug(debug_putc,"!LAST_ACK! ");
                    MACDiscardRx();

                    if ( h->Flags.bits.flagACK )
                    {
                       debug(debug_putc,"-CLOSE- ");
                        CloseSocket(ps);
                    }
                }
                else if ( ps->smState == TCP_FIN_WAIT_1 )
                {
                    debug(debug_putc,"!FIN_WAIT_1! ");
                    MACDiscardRx();

                    if ( h->Flags.bits.flagFIN )
                    {
                       debug(debug_putc,"-FIN- ");
                        flags = ACK;
                        ackn = ++ps->SND_ACK;
                        if ( h->Flags.bits.flagACK )
                        {
                           debug(debug_putc,"CLOSE ");
                            CloseSocket(ps);
                        }
                        else
                        {
                           debug(debug_putc,"CLOSING ");
                            ps->smState = TCP_CLOSING;
                        }
                    }
                }
                else if ( ps->smState == TCP_CLOSING )
                {
                   debug(debug_putc,"!CLOSING! ");
                    MACDiscardRx();

                    if ( h->Flags.bits.flagACK )
                    {
                       debug(debug_putc,"CLOSE ");
                        CloseSocket(ps);
                    }
                }
            }
        }
        else
        {
            debug(debug_putc," CONFUSED");
            MACDiscardRx();
        }
    }

    if ( flags > 0x00 )
    {

         debug(debug_putc,"\r\nHANDLE ");

        TransmitTCP(remote,
                h->DestPort,
                h->SourcePort,
                seqn,
                ackn,
                flags,INVALID_BUFFER, 0);
    }
}



