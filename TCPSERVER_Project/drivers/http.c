//////////////////////////////////////////////////////////////////////////////
///
///                              HTTP.C
///
/// Simple webserver for the Microchip TCP/IP stack.
/// NOTE: THIS IS A DIFFERENT HTTP.C THAN WHAT MICROCHIP PROVIDES
///
/// **** CONFIGURATION ****
///
/// STACK_USE_HTTP - Define this to be true before you include stacktsk.c
///        in your application.  Defining this to be true will cause
///        the stack to include the HTTP portion and execute the init
///        and process any HTTP tasks.
///
/// HTTP_PORT - The TCP/IP port the HTTP server will listen to for HTTP
///        connections.
///
/// HTTP_NUM_SOCKETS - Number of sockets the stack will open for the
///        HTTP server.  You probably will be fine with just 1.  The
///        more sockets you use the more RAM is used to hold buffers
///        and state configuration.
///
/// HTTP_GET_PARAM_MAX_SIZE - This defines the maximum size of several
///        buffers.  This limits the size of your GET or POST requests
///        and all CGI POST data.  If you need more buffer space
///        to hold a lot of CGI POST data you will have to tweak this.
///
/// **** HOW IT WORKS ****
///
/// The TCP/IP stack will open sockets to the desired ports.  It will
/// then listen for GET or POST requests.  When it gets a GET or POST
/// request it passes the page request to the callback function
/// http_get_page() which then returns 0 if the page doesn't exist, or
/// a pointer to the constant memory area that holds the page in program
/// memory.  If it was a POST request it waits until the HTTP header is
/// done and then saves the POST data into a buffer, and passes the
/// buffer to the callback function http_exec_cgi().  http_exec_cgi() will
/// parse the CGI post data and act upon it.  When done, the HTTP
/// server then responds by sending the page.  If the page is to have
/// variable data, it can be represented by an escape code - %0 or %1
/// for example.  When the HTTP stack sees such an escape code it calls
/// the callback function http_format_char() to format the escape code
/// into the needed variable data (such as ADC readings).  After the
/// HTTP stack is done sending the request it will close the port.
/// If the page didn't exist in program memory it will send a 404 File
/// not found error.  If there was a problem/timeout parsing the request
/// the HTTP stack will send a 500 Internal Server Error response.
///
/// **** FUNCTIONS *****
///
/// Most of the functions in this file are called automatically by the
/// TCP/IP stack, but two functions are provided to help in parsing CGI
/// post strings from which the main application can use:
///
/// http_parse_cgi() - Give it a key and it returns a pointer to the value
///
/// http_parse_cgi_int() - Give it a key and returns a pointer to the value
///
/// **** CALL BACK FUNCTIONS ****
///
/// Your main application must provide the following callback functions to
/// fill application dependent needs:
///
/// int32 http_get_page(char *file) - Given a file name, returns a pointer
///    to the page in flash program memory.  If file doesn't exist will
///    return 0.
///
/// void http_exec_cgi(char *cmd) -
///    HTTP Stack passes the CGI post data from which the
///    the application can parse and act upon.
///
/// int8 http_format_char(char id, char *str, int8 max_ret) -
///    Given an escaped character in the program memory
///    HTTP file, convert to variable data.
///    id is the escaped character (in ASCII).
///    *str is where to save the result.
///    max_ret is the maximum amount of bytes you can save to *str.
///    returns the number of bytes written to *str.
///
/// **** LIMITATIONS ****
///
/// Only parses CGI data from a POST.  Does not parse CGI data from a GET.
///
/// When creating web pages with forms, keep your form names (keys) simple
/// because the HTTP stack does not format the escape characters.  For example,
/// when sending "Pass+Word" the HTTP client will parse it out as "Pass%2bWord".
/// The HTTP stack will correctly parse out the escape chars when retrieving
/// the value, but not the key.  Therefore keep your keys simple.
///
//////////////////////////////////////////////////////////////////////////////

#include "drivers\http.h"

#define HTTP_PORT             80

#define HTTP_NUM_SOCKETS      1

#define HTTP_GET_PARAM_MAX_SIZE  100

const char http_header[]="HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
const char http_404_error[]="HTTP/1.0 404 Not found\r\n\r\n<H1>404 Error</H1><HR>Not found.\r\n";
const char http_500_error[]="HTTP/1.0 500 Server Error\r\n\r\n<H1>500 Error</H1><HR>Internal Server Error\r\n";

//key=val pair string, & delimited
void http_parse_cgi_str(int32 file, char *cgistr);

//**** CALLBACKS START ******///

/// the following three functions are callbacks and
/// must be written in your main application!!!

//what page is the user trying to GET or POST.
//return a pointer to "page" in program memory if page exists,
//else return NULL (0)
int32 http_get_page(char *file);

//convert a formatting char, ex %c, to a special string.
// page - the page that this formatting is being done on
// id - the conversion character.  For example, if %c then id=c
// str - where to save result
// max_ret - max size of string str can handle without overrun
// returns length of string that is converted, or 0 if we can't handle this code
int8 http_format_char(int32 file, char id, char *str, int8 max_ret);

//process CGI command
// file is the 32bit address of the file in constant memory that is being served for this request
// key is the CGI key
// val is the value assigned to this CGI key (key=val pair)
void http_exec_cgi(int32 file, char *key, char *val);

//**** CALLBACKS END ******///

int8 http_socket[HTTP_NUM_SOCKETS];
enum {HTTP_IGNORE, HTTP_LISTEN_WAIT, HTTP_CONNECTED, HTTP_PARSE_POST, HTTP_SEND_RESPONSE, HTTP_CLOSE,HTTP_CLOSE_WAIT} http_state[HTTP_NUM_SOCKETS]=HTTP_IGNORE;

//sends out a HTML page that is saved in program eeprom to the specified socket.
//addy is the pointer to program memory.
void tcp_http_put_rom(int8 socket, int32 addy) {
   int8 c;
   int32 file;
   char str[20];
   char *ptr;
   int8 strlen;

   do {
      read_program_memory(addy++, &c, 1);
      if (c=='%') {
         read_program_memory(addy++, &c, 1);
         if (c && (c!='%')) {
            strlen=http_format_char(file, c, &str[0], sizeof(str)-1);
            ptr=&str[0];
            while (strlen--) {
               TCPPut(socket, *ptr);
               ptr++;
            }
         }
         else if (c=='%') {
            TCPPut(socket,c);
         }
      }
      else if (c)
         TCPPut(socket,c);
   } while (c);
}

//initializes the HTTP state machine.  called automatically by the TCP/IP stack
void HTTP_Init(void) {
   int8 i;
   for (i=0;i<HTTP_NUM_SOCKETS;i++) {
      http_socket[i]=TCPListen(HTTP_PORT);
      if (http_socket[i]!=INVALID_SOCKET) {
         http_state[i]=HTTP_LISTEN_WAIT;
      }
   }
}

void HTTP_Task(void) {
   static char http_get_str[]="GET";
   static char http_post_str[]="POST";
   static char http_len_str[]="Content-Length: ";

   static char lc;
   static char buffer[HTTP_NUM_SOCKETS][HTTP_GET_PARAM_MAX_SIZE];

   static int8 i[HTTP_NUM_SOCKETS];
   static enum {HTTP_REQ_GET=0, HTTP_REQ_POST=1, HTTP_REQ_UNKOWN=0xFF} http_cmd[HTTP_NUM_SOCKETS]={0xFF};
   static int32 http_page_req[HTTP_NUM_SOCKETS];
   static int16 http_post_size[HTTP_NUM_SOCKETS]=0;
   static int16 http_timer[HTTP_NUM_SOCKETS];

   int8 hs;
   char c;
   char *ptr, *p;

   for (hs=0;hs<HTTP_NUM_SOCKETS;hs++) {
      if (!TCPIsConnected(http_socket[hs])) {
         http_state[hs]=HTTP_LISTEN_WAIT;
         i[hs]=0;
      }

      switch(http_state[hs]) {
         case HTTP_LISTEN_WAIT:
            if (!TCPIsConnected(http_socket[hs])) {
               break;
            }
            i[hs]=0;
            http_state[hs]=HTTP_CONNECTED;
            http_timer[hs]=TickGet();

         //wait until we get '\r\n\r\n', which marks the end of the HTTP request header
         case HTTP_CONNECTED:
         case HTTP_PARSE_POST:
            while (TCPIsGetReady(http_socket[hs])) {
               if (TCPGet(http_socket[hs],&c)) {
                  if (http_state[hs]==HTTP_CONNECTED) {
                     if (
                           ((c=='\n')&&(lc=='\r'))
                                    ||
                           ((c=='\r')&&(lc=='\n'))
                        )
                     {
                        if ((ptr=strstr(&buffer[hs][0],http_get_str))==&buffer[hs][0]) {
                           http_post_size[hs]=0;
                           http_cmd[hs]=HTTP_REQ_GET;
                           ptr+=4;
                           p=ptr-1;
                           do {
                              p++;
                              c=*p;
                           } while ((c!=' ')&&(c!='?'));
                           *p=0;
                           http_page_req[hs]=http_get_page(ptr);
                           if (c=='?') {
                              p++;
                              ptr=p;
                              while(*p!=' ') p++;
                              *p=0;
                              http_parse_cgi_str(http_page_req[hs], ptr);   
                           }
                        }

                        else if ((ptr=strstr(&buffer[hs][0],http_post_str))==&buffer[hs][0]) {
                           http_post_size[hs]=0;
                           http_cmd[hs]=HTTP_REQ_POST;
                           ptr+=5;
                           p=ptr;
                           while((*p!=' ')&&(*p!='?')) p++;
                           //while(*p!=' ') p++;
                           *p=0;
                           http_page_req[hs]=http_get_page(ptr);
                        }

                        else if ((ptr=strstr(&buffer[hs][0], http_len_str))==&buffer[hs][0]) {
                           ptr+=16;
                           http_post_size[hs]=atol(ptr);
                        }
                        else if (i[hs]==1) {
                           if (http_cmd[hs]==HTTP_REQ_GET) {
                              http_state[hs]=HTTP_SEND_RESPONSE;
                              TCPDiscard(http_socket[hs]);
                              continue;
                           }
                           else if (http_cmd[hs]==HTTP_REQ_POST) {
                              if (http_post_size[hs]) {
                                 http_state[hs]=HTTP_PARSE_POST;
                                 i[hs]=0;
                                 buffer[hs][0]=0;
                              }
                              else {
                                 http_page_req[hs]=0xFFFFFFFF;
                                 http_state[hs]=HTTP_SEND_RESPONSE;
                                 TCPDiscard(http_socket[hs]);
                                 continue;
                              }
                              //return;
                           }
                           else {
                              http_page_req[hs]=0xFFFFFFFF;
                              http_state[hs]=HTTP_SEND_RESPONSE;
                           }
                        }
                        i[hs]=0;
                     }
                     else {
                        buffer[hs][i[hs]]=c;
                        i[hs]=i[hs]+1;
                        if (i[hs]>=HTTP_GET_PARAM_MAX_SIZE) {i[hs]=HTTP_GET_PARAM_MAX_SIZE-1;}
                        buffer[hs][i[hs]]=0;
                        lc=c;
                     }
                  }
                  else if ((http_state[hs]==HTTP_PARSE_POST) && http_post_size[hs] ) {
                     buffer[hs][i[hs]]=c;
                     i[hs]=i[hs]+1;
                     if (i[hs]>=HTTP_GET_PARAM_MAX_SIZE) {i[hs]=HTTP_GET_PARAM_MAX_SIZE-1;}
                     buffer[hs][i[hs]]=0;
                     http_post_size[hs]--;
                  }
               }
            }
            if ((http_state[hs]==HTTP_PARSE_POST) && (http_post_size[hs] == 0)) {
               TCPDiscard(http_socket[hs]);
               //http_exec_cgi(&buffer[hs][0]);
               http_parse_cgi_str(http_page_req[hs], &buffer[hs][0]);
               http_state[hs]=HTTP_SEND_RESPONSE;
               continue;
            }
            if (TickGetDiff(TickGet(),http_timer[hs]) > TICKS_PER_SECOND*20) {
               http_page_req[hs]=0xFFFFFFFF;
               http_state[hs]=HTTP_SEND_RESPONSE;
            }
            break;

         case HTTP_SEND_RESPONSE:
            if (TCPIsPutReady(http_socket[hs])) {
               if (http_page_req[hs]==0xFFFFFFFF) {
                  tcp_http_put_rom(http_socket[hs], label_address(http_500_error));
               }
               else if (http_page_req[hs]) {
                  tcp_http_put_rom(http_socket[hs], label_address(http_header));
                  tcp_http_put_rom(http_socket[hs], http_page_req[hs]);

               }
               else {
                  tcp_http_put_rom(http_socket[hs], label_address(http_404_error));
               }

               TCPFlush(http_socket[hs]);
               http_state[hs]=HTTP_CLOSE;
            }
            break;


         case HTTP_CLOSE:
            if (TCPIsPutReady(http_socket[hs])) {
              TCPDisconnect(http_socket[hs]);
              http_state[hs]=HTTP_CLOSE_WAIT;
            }
            break;

         //default: return;
      }
   }
}

//given a string of key=value pairs, pass individual key=value pair
//to the callback function http_exec_cgi().  Also parses special chars % and +
void http_parse_cgi_str(int32 file, char *cgistr) {
   char c;
   int8 i;
   char key[HTTP_GET_PARAM_MAX_SIZE/2];
   char val[HTTP_GET_PARAM_MAX_SIZE/2];
   char *ptr;
   int8 special_char;
   int8 d;

   i=0;
   ptr=&key[0];
   key[0]=0;
   val[0]=0;
   special_char=0;

   while(TRUE) {
      c=*cgistr;
      cgistr++;
      
      if ((c=='&')||(c<0x20)) {
         http_exec_cgi(file,key,val);
         ptr=&key[0];
         key[0]=0;
         i=0;
         if (c==0)
            break;
      }
      else if (c=='=') {
         ptr=&val[0];
         val[0]=0;
         i=0;
      }
      else if (c=='%') {
         special_char=1;
      }
      else {
         if (special_char==1) {
            if (c<='9') {d=(c-'0')*16;}
            else {
               c&=~0x20;   //uppercase
               d=(c-'A'+10)*16;
            }
            special_char=2;
         }
         else {
            if (special_char==2) {
               if (c<='9') {d+=c-'0';}
               else {
                  c&=~0x20;   //uppercase
                  d+=c-'A'+10;
               }
               special_char=0;
               c=d;
            }
            else if (c=='+') {c=' ';}
            *ptr=c;
            if (i<HTTP_GET_PARAM_MAX_SIZE/2) {
               i++;
               ptr++;
            }
            *ptr=0;
         }
      }
   }
}

