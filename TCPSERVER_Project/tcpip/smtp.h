typedef enum __SMTP_EC {
   SMTP_EC_OK = 0,
   SMTP_EC_CLOSE_PREVIOUS = 1,
   SMTP_EC_MAC_TX_FAIL = 2,   //MAC transmit buffer never became free
   SMTP_EC_ARP_FAIL = 3,   //didn't get an arp response
   SMTP_EC_INVALID_SOCKET = 4,   //TCPConnect() returned INVALID_SOCKET (too many TCP sockets open?)
   SMTP_EC_CONNECT_FAIL = 5,  //TCPIsConnected() never returned TRUE within timeout window
   SMTP_EC_BAD_EHLO = 6, //no or bad response to ehlo command
   SMTP_EC_BAD_MAILFROM = 7,  //no or bad response to mail from: command
   SMTP_EC_BAD_RCPTTO = 8, //no or bad response to rcpt to: command
   SMTP_EC_BAD_DATACMD = 9, //no or bad response to data command
   SMTP_EC_PUT_HEADER = 10, //timeout waiting for socket to be ready to transmit mail headers
   SMTP_EC_BAD_WELCOME = 11, //after making a TCP connection, didn't get proper welcome message from server
   SMTP_EC_BODY_NOT_ACCEPTED = 12 //after sending <CRLF>.<CRLF> we didn't get right response
} SMTP_EC;

//user functions
int8 SMTPConnect(IP_ADDR *ip, int16 port, char *from, char *to, char *subject);
int8 SMTPIsPutReady(void);
void SMTPPut(char c);
void SMTPDisconnect(void);

//stack functions
void SMTPInit(void);
void SMTPTask(void);
void SMTPReadResultCodeReset(void);
int8 SMTPReadResultCode(int16 *smtp_result);
