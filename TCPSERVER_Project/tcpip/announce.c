/*********************************************************************
 *
 *                  Announce Module for Microchip TCP/IP Stack
 *
 *********************************************************************
 * FileName:        announce.c
 * Dependencies:    UDP.h
 * Processor:       PIC18
 * Complier:        MCC18 v1.00.50 or higher
 *                  HITECH PICC-18 V8.10PL1 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * This software is owned by Microchip Technology Inc. ("Microchip")
 * and is supplied to you for use exclusively as described in the
 * associated software agreement.  This software is protected by
 * software and other intellectual property laws.  Any use in
 * violation of the software license may subject the user to criminal
 * sanctions as well as civil liability.  Copyright 2006 Microchip
 * Technology Inc.  All rights reserved.
 *
 * This software is provided "AS IS."  MICROCHIP DISCLAIMS ALL
 * WARRANTIES, EXPRESS, IMPLIED, STATUTORY OR OTHERWISE, NOT LIMITED
 * TO MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * INFRINGEMENT.  Microchip shall in no event be liable for special,
 * incidental, or consequential damages.
 *
 *
 * Author               Date    Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Howard Schlunder     10/7/04	Original
 * Howard Schlunder		2/9/05	Simplified MAC address to text
 *								conversion logic
 * Howard Schlunder		2/14/05	Fixed subnet broadcast calculation
 * Darren Rook (CCS)    07/13/06 In synch with Microchip's V3.02 stack
 * Darren Rook (CCS)    08/15/06 AnnounceIP() returns TRUE if success
 * Darren Rook (CCS)    10/24/06 In synch with Microchips's V3.75 stack
 ********************************************************************/

#include "tcpip/UDP.h"
#include "tcpip/Helpers.h"

#ifndef ANNOUNCE_PORT
#define ANNOUNCE_PORT	30303
#endif

/*********************************************************************
 * Function:        void AnnounceIP(void)
 *
 * PreCondition:    Stack is initialized()
 *
 * Input:           None
 *
 * Output:          TRUE if success
 *                  FALSE because the MAC isn't ready to TX -OR- no UDP sockets
 *
 * Side Effects:    None
 *
 * Overview:        AnnounceIP opens a UDP socket and transmits a
 *					broadcast packet to port 30303.  If a computer is
 *					on the same subnet and a utility is looking for
 *					packets on the UDP port, it will receive the
 *					broadcast.  For this application, it is used to
 *					announce the change of this board's IP address.
 *					The messages can be viewed with the MCHPDetect.exe
 *					program.
 *
 * Note:            A UDP socket must be available before this
 *					function is called.  It is freed at the end of
 *					the function.  MAX_UDP_SOCKETS may need to be
 *					increased if other modules use UDP sockets.
 ********************************************************************/
int AnnounceIP(void)
{
	UDP_SOCKET	MySocket;
	NODE_INFO	Remote;
	int8 		i;

	// Make certain the MAC can be written to
   if (!MACIsTxReady())
      return(FALSE);
	
	// Set the socket's destination to be a broadcast over our IP
	// subnet
	// Set the MAC destination to be a broadcast
	memset(&Remote, 0xFF, sizeof(Remote));

	// Set the IP subnet's broadcast address
	Remote.IPAddr.Val = (AppConfig.MyIPAddr.Val & AppConfig.MyMask.Val) |
						 ~AppConfig.MyMask.Val;

	// Open a UDP socket for outbound transmission
	MySocket = UDPOpen(2860, &Remote, ANNOUNCE_PORT);

	// Abort operation if no UDP sockets are available
	// If this ever happens, incrementing MAX_UDP_SOCKETS in
	// StackTsk.h may help (at the expense of more global memory
	// resources).
	if( MySocket == INVALID_UDP_SOCKET )
		return(FALSE);

	UDPIsPutReady(MySocket);

	// Begin sending our MAC address in human readable form.
	// The MAC address theoretically could be obtained from the 
	// packet header when the computer receives our UDP packet, 
	// however, in practice, the OS will abstract away the useful
	// information and it would be difficult to obtain.  It also 
	// would be lost if this broadcast packet were forwarded by a
	// router to a different portion of the network (note that 
	// broadcasts are normally not forwarded by routers).
	for(i=0; i < sizeof(AppConfig.NetBIOSName)-1; i++)
	{
		UDPPut(AppConfig.NetBIOSName[i]);
	}
	UDPPut('\r');
	UDPPut('\n');

	// Convert the MAC address bytes to hex (text) and then send it
	i = 0;
	while(1)
	{
		UDPPut(btohexa_high(AppConfig.MyMACAddr.v[i]));
	    UDPPut(btohexa_low(AppConfig.MyMACAddr.v[i]));
	    if(++i == 6)
	    	break;
	    UDPPut('-');
	}
	UDPPut('\r');
	UDPPut('\n');

	// Send some other human readable information.
	UDPPut('D');
	UDPPut('H');
	UDPPut('C');
	UDPPut('P');
	UDPPut('/');
	UDPPut('P');
	UDPPut('o');
	UDPPut('w');
	UDPPut('e');
	UDPPut('r');
	UDPPut(' ');
	UDPPut('e');
	UDPPut('v');
	UDPPut('e');
	UDPPut('n');
	UDPPut('t');
	UDPPut(' ');
	UDPPut('o');
	UDPPut('c');
	UDPPut('c');
	UDPPut('u');
	UDPPut('r');
	UDPPut('r');
	UDPPut('e');
	UDPPut('d');

	// Send the packet
	UDPFlush();

	// Close the socket so it can be used by other modules
	UDPClose(MySocket);
	
	return(TRUE);
}

void DiscoveryTask(void)
{
	static enum {
		DISCOVERY_HOME = 0,
		DISCOVERY_LISTEN,
		DISCOVERY_REQUEST_RECEIVED,
		DISCOVERY_DISABLED
	} DiscoverySM = DISCOVERY_HOME;

	static UDP_SOCKET	MySocket;
	int8 				i;
	
	switch(DiscoverySM)
	{
		case DISCOVERY_HOME:
			// Open a UDP socket for inbound and outbound transmission
			// Since we expect to only receive broadcast packets and 
			// only send unicast packets directly to the node we last 
			// received from, the remote NodeInfo parameter can be anything
			MySocket = UDPOpen(ANNOUNCE_PORT, NULL, ANNOUNCE_PORT);

			if(MySocket == INVALID_UDP_SOCKET)
				return;
			else
				DiscoverySM++;
			break;

		case DISCOVERY_LISTEN:
			// Do nothing if no data is waiting
			if(!UDPIsGetReady(MySocket))
				return;
			
			// See if this is a discovery query or reply
			UDPGet(&i);
			UDPDiscard();
			if(i != 'D')
				return;
			
			// We received a discovery request, reply
			DiscoverySM++;
			break;

		case DISCOVERY_REQUEST_RECEIVED:
			if(!UDPIsPutReady(MySocket))
				return;

			// Begin sending our MAC address in human readable form.
			// The MAC address theoretically could be obtained from the 
			// packet header when the computer receives our UDP packet, 
			// however, in practice, the OS will abstract away the useful
			// information and it would be difficult to obtain.  It also 
			// would be lost if this broadcast packet were forwarded by a
			// router to a different portion of the network (note that 
			// broadcasts are normally not forwarded by routers).
			for(i=0; i < sizeof(AppConfig.NetBIOSName)-1; i++)
			{
				UDPPut(AppConfig.NetBIOSName[i]);
			}
			UDPPut('\r');
			UDPPut('\n');
		
			// Convert the MAC address bytes to hex (text) and then send it
			i = 0;
			while(1)
			{
				UDPPut(btohexa_high(AppConfig.MyMACAddr.v[i]));
			    UDPPut(btohexa_low(AppConfig.MyMACAddr.v[i]));
			    if(++i == 6)
			    	break;
			    UDPPut('-');
			}
			UDPPut('\r');
			UDPPut('\n');

			// Change the destination to the unicast address of the last received packet
        	memcpy((void*)&UDPSocketInfo[MySocket].remoteNode, (const void*)&remoteNode, sizeof(remoteNode));
		
			// Send the packet
			UDPFlush();

			// Listen for other discovery requests
			DiscoverySM = DISCOVERY_LISTEN;
			break;

		case DISCOVERY_DISABLED:
			break;
	}	

}

