//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                                 SMTP.C                                   //
//              SMTP Engine for Microchip TCP/IP Stack API                  //
//
// Define STACK_USE_SMTP to TRUE before including stacktsk.h in your code
// to enable this SMTP API.  Once enabled, you can use these functions:
//
// SMTPConnect(IP_ADDR ip, int16 port, char *from, char *to, char *subject)
//    Will open the IP address and TCP port (which should be your SMTP
//    server), and start the engine that will initiate SMTP connection.
//    The SMTP engine will then send the mail to: and rcpt from: command,
//    and create basic E-Mail headers.  Will return TRUE if successfully
//    started the engine, will return FALSE if a previous engine is still
//    running.
//    Once an engine is running, it will stop running once
//    SMTPIsPutReady() returns TRUE -OR- SMTPLastError() returns a
//    non-zero number.
//
// SMTPIsPutReady()
//    After a succesfull SMTPConnect(), the SMTP engine will be sending
//    SMTP commands.  You cannot start sending the body of the e-mail
//    until the SMTP engine has got the SMTP server in a state that is
//    ready for the body of the e-mail.  SMTPIsPutReady() returns TRUE
//    if the SMTP engine and the SMTP server is ready.
//
// SMTPLastError()
//    If there was an error with the SMTP, this will return non-zero.
//    Once this returns non-zero then you can try again by calling
//    SMTPConnect().
//
// SMTPPut(char c)
//    Puts this char into the body of the e-mail.  SMTPIsPutReady() must
//    return TRUE before this is called.
//
// SMTPDisconnect()
//    Close the e-mail and have the SMTP server send it.  SMTPIsPutReady()
//    must return TRUE before you call this.  After calling this, wait
//    until SMTPIsFree() returns TRUE and use SMTPLastError() to see
//    if the email was sent sucessfully.
//
// SMTPIsFree()
//    Will return TRUE if the SMTP engine is free for another connection.
//
// NOTE: You *MUST* use the SMTP server for your ISP.  If you do not know it
//  then ask your ISP.  The reason for this is that because of the war on
//  spam almost all SMTP servers block access to clients who aren't on their
//  network.
//
// NOTE: The SMTP engine can only handle one socket at a time.  Therefore you
//  cannot call a SMTPConnect() until the previous SMTPConnect() has been
//  disconnected.
//
// NOTE: Due to the war on spam many internet service providers are placing
//  restrictions upon SMTP servers.  Such restrictions may be authentication,
//  sender-id, message-id and max message-per-minute rate.  This engine
//  deals with none of those restrictions.  It's very likely in the future
//  that it will be impossible for a PIC to have the resources to send e-mail
//  using SMTP.
//
// NOTE: If you are using Ethernet, you will have to enable ARP!!!
//
///////////////////////////////////////////////////////////////////////////////

#ifndef debugf_smtp
#define debugf_smtp
#endif

enum {
   SMTP_STATE_WAITING=0, SMTP_STATE_START=1, SMTP_STATE_ARP_REQ=2,
   SMTP_STATE_ARP_WAIT=3, SMTP_STATE_CONNECT=4, SMTP_STATE_CONNECT_WAIT=5,
   SMTP_STATE_EHLO=6, SMTP_STATE_MAIL_FROM=7, SMTP_STATE_RCPT_TO=8,
   SMTP_STATE_DATA_START=9, SMTP_STATE_DO_CMD=10, SMTP_STATE_DO_CMD_GET_RESP=11,
   SMTP_STATE_PUT_HEADER=12, SMTP_STATE_PUT_BODY=13, SMTP_STATE_FINISH_EMAIL=14,
   SMTP_STATE_FINISH_EMAIL_WAIT=15, SMTP_STATE_CLOSE=16,
   SMTP_STATE_FORCE_CLOSE=17, SMTP_STATE_RESET=18
} smtp_state;


TCP_SOCKET smtp_socket=INVALID_SOCKET;
NODE_INFO smtp_remote;
char *smtp_engine_from;
char *smtp_engine_to;
char *smtp_engine_subject;
SMTP_EC smtp_last_error=0;
int16 smtp_engine_port;

//user functions
int8 SMTPConnect(IP_ADDR *ip, int16 port, char *from, char *to, char *subject) {
   if (smtp_socket==INVALID_SOCKET) {
      smtp_state=SMTP_STATE_START;
      memcpy(&smtp_remote.IPAddr, ip, sizeof(IP_ADDR));
      smtp_engine_port=port;
      smtp_engine_from=from;
      smtp_engine_to=to;
      smtp_engine_subject=subject;
      smtp_last_error=0;
      return(TRUE);
   }
   smtp_last_error=SMTP_EC_CLOSE_PREVIOUS;
   return(FALSE);
}

int8 SMTPIsFree(void) {
   return(smtp_socket==INVALID_SOCKET);
}

int8 SMTPIsPutReady(void) {
   return((smtp_state==SMTP_STATE_PUT_BODY)&&(TCPIsPutReady(smtp_socket)));
}

void SMTPPut(char c) {
   if (SMTPIsPutReady() && TCPIsPutReady(smtp_socket)) {
      TCPPut(smtp_socket,c);
   }
}

void SMTPDisconnect(void) {
   if (smtp_state==SMTP_STATE_PUT_BODY)
      smtp_state=SMTP_STATE_FINISH_EMAIL;
   else if (smtp_socket!=INVALID_SOCKET)
      smtp_state=SMTP_STATE_CLOSE;
}

SMTP_EC SMTPLastError(void) {
   return(smtp_last_error);
}

//stack functions
void SMTPInit(void) {
   if (smtp_socket!=INVALID_SOCKET) {
      TCPDisconnect(smtp_socket);
   }
   smtp_socket=INVALID_SOCKET;
   smtp_state=SMTP_STATE_WAITING;
}

void SMTPError(SMTP_EC ec) {
   smtp_last_error=ec;
   smtp_state=SMTP_STATE_RESET;
}

void SMTPPutCmd(char c) {
   TCPPut(smtp_socket,c);
}


void SMTPTask(void) {
   TICKTYPE currTick;
   static TICKTYPE lastTick;
   int16 smtp_result;
   static int16 smtp_expected_result;

   static char ehlomsg[]="ehlo me";
   static char datamsg[]="data";
   static char mailfrommsg[]="mail from: ";
   static char rcpttomsg[]="rcpt to: ";

   static char *cmdptr;
   static char *cmdptr2;
   static SMTP_EC on_err;
   static int8 next_state;

   currTick=TickGet();

   switch(smtp_state) {
      case SMTP_STATE_WAITING:
         break;

      case SMTP_STATE_START:
         lastTick=currTick;
         smtp_state=SMTP_STATE_ARP_REQ;

   #if STACK_USE_ARP
      case SMTP_STATE_ARP_REQ:
         if (ARPIsTxReady()) {
            ARPResolve(&smtp_remote.IPAddr);
            lastTick=currTick;
            smtp_state=SMTP_STATE_ARP_WAIT;
         }
         else if (TickGetDiff(currTick,lastTick) > (TICKS_PER_SECOND / 2)) {
            SMTPError(SMTP_EC_MAC_TX_FAIL);
         }
         break;

      case SMTP_STATE_ARP_WAIT:
         if (ARPIsResolved(&smtp_remote.IPAddr, &smtp_remote.MACAddr)) {
            smtp_state=SMTP_STATE_CONNECT;
         }
         else if (TickGetDiff(currTick, lastTick) > (TICKS_PER_SECOND * (TICKTYPE)5)) {
            SMTPError(SMTP_EC_ARP_FAIL);
         }
         break;
   #else    
      case SMTP_STATE_ARP_REQ:
      case SMTP_STATE_ARP_WAIT:
         smtp_state=SMTP_STATE_CONNECT;
   #endif


      case SMTP_STATE_CONNECT:
         smtp_socket=TCPConnect(&smtp_remote, smtp_engine_port);
         if (smtp_socket!=INVALID_SOCKET) {
            lastTick=currTick;
            smtp_state=SMTP_STATE_CONNECT_WAIT;
         }
         else {
            SMTPError(SMTP_EC_INVALID_SOCKET);
         }
         break;

      case SMTP_STATE_CONNECT_WAIT:
         if (TCPIsConnected(smtp_socket)) {
            smtp_state=SMTP_STATE_DO_CMD_GET_RESP;
            smtp_expected_result=220;
            next_state=SMTP_STATE_EHLO;
            on_err=SMTP_EC_BAD_WELCOME;
            lastTick=currTick;
            SMTPReadResultCodeReset();
         }
         else if (TickGetDiff(currTick, lastTick) > (TICKS_PER_SECOND * 10)) {
            SMTPError(SMTP_EC_CONNECT_FAIL);
         }
         break;

      case SMTP_STATE_EHLO:
         cmdptr=ehlomsg;
         cmdptr2=0;
         smtp_expected_result=250;
         on_err=SMTP_EC_BAD_EHLO;
         smtp_state=SMTP_STATE_DO_CMD;
         next_state=SMTP_STATE_MAIL_FROM;
         lastTick=currTick;
         break;

      case SMTP_STATE_MAIL_FROM:
         cmdptr=mailfrommsg;
         cmdptr2=smtp_engine_from;
         smtp_expected_result=250;
         on_err=SMTP_EC_BAD_MAILFROM;
         smtp_state=SMTP_STATE_DO_CMD;
         next_state=SMTP_STATE_RCPT_TO;
         lastTick=currTick;
         break;

      case SMTP_STATE_RCPT_TO:
         cmdptr=rcpttomsg;
         cmdptr2=smtp_engine_to;
         smtp_expected_result=250;
         on_err=SMTP_EC_BAD_RCPTTO;
         smtp_state=SMTP_STATE_DO_CMD;
         next_state=SMTP_STATE_DATA_START;
         lastTick=currTick;
         break;

      case SMTP_STATE_DATA_START:
         cmdptr=datamsg;
         cmdptr2=0;
         smtp_expected_result=354;
         on_err=SMTP_EC_BAD_RCPTTO;
         smtp_state=SMTP_STATE_DO_CMD;
         next_state=SMTP_STATE_PUT_HEADER;
         lastTick=currTick;
         break;

      case SMTP_STATE_DO_CMD:
         TCPDiscard(smtp_socket);
         if (TCPIsPutReady(smtp_socket)) {
           printf(SMTPPutCmd, "%s", cmdptr);
           if (cmdptr2)
              printf(SMTPPutCmd, "%s", cmdptr2);
           SMTPPutCmd('\r');
           SMTPPutCmd('\n');
           TCPFlush(smtp_socket);
           smtp_state=SMTP_STATE_DO_CMD_GET_RESP;
           lastTick=currTick;
           SMTPReadResultCodeReset();
         }
         else if (TickGetDiff(currTick,lastTick) > (TICKS_PER_SECOND / 2)) {
            SMTPError(SMTP_EC_MAC_TX_FAIL);
         }
         break;

      case SMTP_STATE_DO_CMD_GET_RESP:
         if (TCPIsGetReady(smtp_socket)) {
            lastTick=currTick;
            if (SMTPReadResultCode(&smtp_result)) {
               if (smtp_result==smtp_expected_result) {
                  lastTick=currTick;
                  smtp_state=next_state;
               }
               else {
                  SMTPError(on_err);
               }
            }
         }
         else if (TickGetDiff(currTick, lastTick) > (TICKS_PER_SECOND * (TICKTYPE)10)) {
            SMTPError(on_err);
         }
         break;

      case SMTP_STATE_PUT_HEADER:
         if (TCPIsPutReady(smtp_socket)) {
            printf(SMTPPutCmd, "To: %s\r\n", smtp_engine_to);
            printf(SMTPPutCmd, "From: %s\r\n", smtp_engine_from);
            printf(SMTPPutCmd, "Subject: %s\r\n", smtp_engine_subject);
            TCPFlush(smtp_socket);
            smtp_state=SMTP_STATE_PUT_BODY;
            lastTick=currTick;
         }
         else if (TickGetDiff(currTick, lastTick) > (TICKS_PER_SECOND * 3)) {
            SMTPError(SMTP_EC_PUT_HEADER);
         }
         break;

      case SMTP_STATE_PUT_BODY:
         //sit in an infinite loop here.
         //now the user can add their own contents to the email by using SMTPPut().
         //but do a timeout check:
         if (TCPIsGetReady(smtp_socket))
            TCPDiscard(smtp_socket);
         if (TickGetDiff(currTick, lastTick) > (TICKS_PER_SECOND * 60)) {
            smtp_state=SMTP_STATE_FINISH_EMAIL;
         }
         else
            break;

      case SMTP_STATE_FINISH_EMAIL:
         lastTick=currTick;
         smtp_state=SMTP_STATE_FINISH_EMAIL_WAIT;

      case SMTP_STATE_FINISH_EMAIL_WAIT:
         if (TCPIsPutReady(smtp_socket)) {
            SMTPPutCmd('\r');
            SMTPPutCmd('\n');
            SMTPPutCmd('.');
            SMTPPutCmd('\r');
            SMTPPutCmd('\n');
            TCPFlush(smtp_socket);

            smtp_state=SMTP_STATE_DO_CMD_GET_RESP;
            smtp_expected_result=250;
            next_state=SMTP_STATE_CLOSE;
            on_err=SMTP_EC_BODY_NOT_ACCEPTED;
            lastTick=currTick;
            SMTPReadResultCodeReset();
         }
         else if (TickGetDiff(currTick, lastTick) > (TICKS_PER_SECOND * 3)) {
            smtp_state=SMTP_STATE_FORCE_CLOSE;
         }
         break;

      case SMTP_STATE_CLOSE:
         if (TCPIsPutReady(smtp_socket)) {
            smtp_state=SMTP_STATE_FORCE_CLOSE;
         }
         else if (TickGetDiff(currTick, lastTick) > (TICKS_PER_SECOND * 3)) {
            smtp_state=SMTP_STATE_FORCE_CLOSE;
         }
         else
            break;

      case SMTP_STATE_FORCE_CLOSE:
         TCPDisconnect(smtp_socket);

      case SMTP_STATE_RESET:
         SMTPInit();
         break;
   }
}

///*** read result code

int16 smtp_result_code_scratch;
char smtp_result_code_fnnc;   //first non-numeric char

void SMTPReadResultCodeReset(void) {
   smtp_result_code_scratch=0;
   smtp_result_code_fnnc=0;
}

int8 SMTPReadResultCode(int16 *smtp_result) {
   char c;
   debugf_smtp("\r\nGet Result:\r\n");
   while (TCPGet(smtp_socket, &c)) {
      debugf_smtp("%c",c);
      if ( (c>='0') && (c<='9') && (smtp_result_code_fnnc==0) ) {
         smtp_result_code_scratch*=10;
         smtp_result_code_scratch+=c-'0';
      }
      else if (smtp_result_code_fnnc==0) {
         smtp_result_code_fnnc=c;
      }
      if (c==0x0A) {
         if (smtp_result_code_fnnc==' ') {
            debugf_smtp("\r\nResult=%LU",smtp_result_code_scratch);
            *smtp_result=smtp_result_code_scratch;
            TCPDiscard(smtp_socket);
            return(TRUE);
         }
         else {
            debugf_smtp("\r\nContinue");
            SMTPReadResultCodeReset(); //read next line (some commands have multi-line responses)
         }
      }
   }
   return(FALSE);
}

///*** read result code
