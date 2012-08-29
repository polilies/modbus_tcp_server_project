//////////////////////////////////////////////////////////////////////////////
//
// ccstcpip.h - Common code shared among all Embedded Internet/Embedded
// Ethernet tutorial book examples.
//
// If you are using a CCS Embedded Ethernet Board (labeled PICENS, which
// has an MCP ENC28J60) then define STACK_USE_CCS_PICENS to TRUE.
//
// If you are using a CCS Embedded Internet Board (labeled PICNET, which
// has a Realtek RTL8019AS and a 56K Modem) then define STACK_USE_CCS_PICNET
// to TRUE.
//
//////////////////////////////////////////////////////////////////////////////
//
// 10/25/06
//  - Added STACK_USE_CCS_PICEEC
//  - ExampleUDPPacket[] UDP header length fixed
//
//////////////////////////////////////////////////////////////////////////////

#define STACK_USE_CCS_PICENS   1 //TODO
#define STACK_USE_CCS_PICNET   0
#define STACK_USE_CCS_PICEEC   0  

#if STACK_USE_CCS_PICENS
 #define STACK_USE_MCPENC 1
#endif

#if STACK_USE_CCS_PICEEC
 #define STACK_USE_MCPINC 1
#endif

/*#if STACK_USE_CCS_PICENS
 #include <18F4620.h>
 #use delay(clock=40000000)
 #fuses H4, NOWDT, NOLVP, NODEBUG
#elif STACK_USE_CCS_PICNET
 #include <18F6722.h>
 #use delay(clock=40000000)
 #fuses H4, NOWDT, NOLVP, NODEBUG
#elif STACK_USE_CCS_PICEEC
 #include <18F67J60.h>
 //#use delay(clock=41666667)
 //#fuses NOWDT, NODEBUG, H4_SW, NOIESO, NOFCMEN, PRIMARY
 #use delay(clock=25M)
 #fuses NOWDT, NODEBUG, HS, NOIESO, NOFCMEN, PRIMARY, ETHLEDNOEMB
#elif STACK_USE_CCS_PICNET_OLD
 #include <18F6720.h>
 #use delay(clock=20000000)
 #fuses HS, NOWDT, NOLVP, NODEBUG
 #define STACK_USE_CCS_PICNET   TRUE
#else
 #error You need to define your custom hardware
#endif

#use rs232(baud=9600, xmit=PIN_C6, rcv=PIN_C7)*/

#include "tcpip/stacktsk.c"    //include Microchip TCP/IP Stack

#if STACK_USE_CCS_PICENS
 #define BUTTON1_PRESSED()  (!input(PIN_A4))

 #define USER_LED1    PIN_A5
 #define USER_LED2    PIN_B4
 #define USER_LED3    PIN_B5
 #define LED_ON       output_low
 #define LED_OFF      output_high
 void init_user_io(void) {
   setup_adc(ADC_CLOCK_INTERNAL);
   setup_adc_ports(AN0);
   *0xF92=(*0xF92 & 0xDF) | 0x11;   //a5 output, a4 and a0 input
   *0xF93=*0xF93 & 0xCF;   //b4 and b5 output
   LED_OFF(USER_LED1);
   LED_OFF(USER_LED2);
   LED_OFF(USER_LED3);
 }
#elif STACK_USE_CCS_PICEEC
 #define BUTTON1_PRESSED()  (!input(PIN_A4))

 #define USER_LED1    PIN_A5
 #define USER_LED2    PIN_B4
 #define USER_LED3    PIN_B6
 #define LED_ON       output_low
 #define LED_OFF      output_high
 void init_user_io(void) {
   setup_adc(ADC_CLOCK_INTERNAL);
   setup_adc_ports(AN0_TO_AN2);
   set_adc_channel(2);
   *0xF92=*0xF92 & 0xFC;   //a0 and a1 output
   *0xF93=*0xF93 & 0xCF;   //b4 and b5 output
   LED_OFF(USER_LED1);
   LED_OFF(USER_LED2);
   LED_OFF(USER_LED3);
 }
#else
 #define BUTTON1_PRESSED()  (!input(PIN_B0))
 #define BUTTON2_PRESSED()  (!input(PIN_B1))

 #define USER_LED1    PIN_B2
 #define USER_LED2    PIN_B4
 #define LED_ON       output_low
 #define LED_OFF      output_high
 void init_user_io(void) {
   setup_adc(ADC_CLOCK_INTERNAL );
   setup_adc_ports(ANALOG_AN0_TO_AN1);
   *0xF92=*0xF92 | 3;            //a0 and a1 input (for ADC)
   *0xF93=(*0xF93 & 0xEB) | 3;   //B0 and B1 input, B2 and B4 output
   LED_OFF(USER_LED1);
   LED_OFF(USER_LED2);
 }
#endif

void MACAddrInit(void) {
   MY_MAC_BYTE1=0;
   MY_MAC_BYTE2=2;
   MY_MAC_BYTE3=3;
   MY_MAC_BYTE4=4;
   MY_MAC_BYTE5=5;
   MY_MAC_BYTE6=6;
}

void IPAddrInit(void) {
   //IP address of this unit
   MY_IP_BYTE1=10;
   MY_IP_BYTE2=46;
   MY_IP_BYTE3=3;
   MY_IP_BYTE4=222;

   //network gateway
   MY_GATE_BYTE1=10;
   MY_GATE_BYTE2=46;
   MY_GATE_BYTE3=3;
   MY_GATE_BYTE4=226;

   //subnet mask
   MY_MASK_BYTE1=255;
   MY_MASK_BYTE2=255;
   MY_MASK_BYTE3=255;
   MY_MASK_BYTE4=0;
}

char ExampleIPDatagram[] = {
   0x45, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
   0x64, 0x11, 0x2A, 0x9D, 0x0A, 0x0B, 0x0C, 0x0D,
   0x0A, 0x0B, 0x0C, 0x0E
};

char ExampleUDPPacket[] = {
   0x04, 0x00, 0x04, 0x01, 0x00, 0x08, 0x00, 0x00,
   0x01, 0x02, 0x03, 0x04
};
