#include "C:\Users\AKAYIKCI\Documents\Ki�isel\Elektronik\ccs c Pic\Uygulamalar\TCPSERVER_Project\main.h"
#define STACK_USE_ICMP  1
#define STACK_USE_ARP   1
#define STACK_USE_TCP   1
#include "ccstcpip.h"


#if STACK_USE_CCS_PICENS
 #include "tcpip/mlcd.c"
#elif STACK_USE_CCS_PICEEC
 #include "tcpip/elcd.c"
#else
 #include "tcpip/dlcd.c"
#endif

#define NUM_LISTEN_SOCKETS 2

#define EXAMPLE_TCP_PORT   (int16)502

char lcd_str[NUM_LISTEN_SOCKETS][20];

//this function is called by MyTCPTask() when the specified socket is connected
//to the PC running the TCPSERVER.EXE demo.
//returns TRUE if BUTTON2 was pressed, therefore we must disconnect the socket
int8 TCPConnectedTask(TCP_SOCKET socket, int8 which) {
   char c[];
   static int8 counter[NUM_LISTEN_SOCKETS];
   static int8 button1_held[NUM_LISTEN_SOCKETS];
   char mb_frame[] ;
   char str[20];
   int8 i=0;
   
   if (TCPIsGetReady(socket)) {
      mb_frame = TCPGetArray(socket,c,12); 
      if(strlen(mb_frame)==12)
      {
         if((mb_frame[5]== 6) && (mb_frame[6]== 250) && (mb_frame[7]== 3) && (mb_frame[11]== 2))
            {
               output_high(PIN_B5);
            }
         else
         {
            output_low(PIN_B5);
         }
      }
       else
       {
            output_low(PIN_B5);
       }
         lcd_str[which][i++]=c;
         if (i>=20) {i=19;}
         lcd_str[which][i]=0;
      }
   }

//when button 1 is pressed: send message over TCP
//when button 2 is pressed: disconnect socket
   if (BUTTON1_PRESSED() && !button1_held[which] && TCPIsPutReady(socket)) {
      button1_held[which]=TRUE;
      sprintf(str,"BUTTON C=%U",counter[which]++);
      TCPPutArray(socket,str,strlen(str));
      TCPFlush(socket);
   }
   if (!BUTTON1_PRESSED()) {
      button1_held[which]=FALSE;
   }
  #if defined(BUTTON2_PRESSED())
   if (BUTTON2_PRESSED()) {
      return(TRUE);
   }
  #endif
   return(FALSE);
}


void MyTCPTask() {
   static TICKTYPE lastTick[NUM_LISTEN_SOCKETS];
   static TCP_SOCKET socket[NUM_LISTEN_SOCKETS]={INVALID_SOCKET};
   static enum {
      MYTCP_STATE_NEW=0, MYTCP_STATE_LISTENING=1,
      MYTCP_STATE_CONNECTED=2, MYTCP_STATE_DISCONNECT=3,
      MYTCP_STATE_FORCE_DISCONNECT=4
   } state[NUM_LISTEN_SOCKETS]={0};
   static NODE_INFO remote[NUM_LISTEN_SOCKETS];
   TICKTYPE currTick;
   int8 dis;
   int8 i;

   currTick=TickGet();

   for (i=0;i<NUM_LISTEN_SOCKETS;i++) {
      switch (state[i]) {
         case MYTCP_STATE_NEW:
            socket[i]=TCPListen(EXAMPLE_TCP_PORT);
            if (socket[i]!=INVALID_SOCKET) {
               state[i]=MYTCP_STATE_LISTENING;
               sprintf(&lcd_str[i][0],"LISTENING");
            }
            else {
               sprintf(&lcd_str[i][0],"SOCKET ERROR");
            }
            break;

         case MYTCP_STATE_LISTENING:
            if (TCPIsConnected(socket[i])) {
               state[i]=MYTCP_STATE_CONNECTED;
               sprintf(&lcd_str[i][0],"CONNECTED!");
               lastTick[i]=currTick;
            }
            break;

         case MYTCP_STATE_CONNECTED:
            if (TCPIsConnected(socket[i])) {
               if (TickGetDiff(currTick,lastTick[i]) > ((int16)TICKS_PER_SECOND * 300)) {
                  state[i]=MYTCP_STATE_DISCONNECT;
                  sprintf(&lcd_str[i][0],"TIMEOUT");
                  lastTick[i]=currTick;
               }
               else {
                  dis=TCPConnectedTask(socket[i],i);
                  if (dis) {
                     sprintf(&lcd_str[i][0],"DISCONNECT");
                     state[i]=MYTCP_STATE_DISCONNECT;
                     lastTick[i]=currTick;
                  }
               }
            }
            else {
               sprintf(&lcd_str[i][0],"DISCONNECTED");
               state[i]=MYTCP_STATE_FORCE_DISCONNECT;
            }
            break;

         case MYTCP_STATE_DISCONNECT:
            if (TCPIsPutReady(socket[i])) {
               state[i]=MYTCP_STATE_FORCE_DISCONNECT;
            }
            else if (TickGetDiff(currTick, lastTick[i]) > (TICKS_PER_SECOND * 10)) {
               state[i]=MYTCP_STATE_FORCE_DISCONNECT;
            }
            break;

         case MYTCP_STATE_FORCE_DISCONNECT:
            TCPDisconnect(socket[i]);
            state[i]=MYTCP_STATE_NEW;
            break;
      }
   }
}

void LCDTask(void) {
   static TICKTYPE lastTick;
   TICKTYPE currTick;
   static enum {LCD_STATE_DISPLAY=0, LCD_STATE_WAIT=1} state=0;

   currTick=TickGet();

   switch(state) {
      case LCD_STATE_DISPLAY:
         printf(lcd_putc,"\f%s\n%s",&lcd_str[0][0],&lcd_str[1][0]);
         state=LCD_STATE_WAIT;
         lastTick=currTick;
         break;

      case LCD_STATE_WAIT:
         if (TickGetDiff(currTick,lastTick) > (TICKS_PER_SECOND / 4))
            state=LCD_STATE_DISPLAY;
         break;
   }
}


void main(void)
{

   setup_adc_ports(NO_ANALOGS|VSS_VDD);
   setup_adc(ADC_OFF|ADC_TAD_MUL_0);
   setup_psp(PSP_DISABLED);
   setup_spi(SPI_SS_DISABLED);
   setup_wdt(WDT_OFF);
   setup_timer_0(RTCC_INTERNAL);
   setup_timer_1(T1_DISABLED);
   setup_timer_2(T2_DISABLED,0,1);
   setup_comparator(NC_NC_NC_NC);
   setup_vref(FALSE);
//Setup_Oscillator parameter not selected from Intr Oscillator Config tab

   // TODO: USER CODE!!
   
   

   printf("\r\n\nCCS TCP/IP TUTORIAL, EXAMPLE 13B (TCP SERVER)\r\n");
   MACAddrInit();
   IPAddrInit();

   init_user_io();

   lcd_init();
   printf("deneme");
   delay_ms(3000);
   sprintf(&lcd_str[0][0],"INIT");
   sprintf(&lcd_str[1][0],"INIT");
   lcd_putc('\f');

   StackInit();

   while(TRUE) {
      StackTask();
      MyTCPTask();
      LCDTask();
   }
}
