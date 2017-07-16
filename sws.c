/* 
 * File: sws.c
 * Author: Alex Brodsky
 * Purpose: This file contains the implementation of a simple web server.
 *          It consists of two functions: main() which contains the main 
 *          loop accept client connections, and serve_client(), which
 *          processes each client request.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "network.h"

#define MAX_HTTP_SIZE 8192                 /* size of buffer to allocate */
#define RCB_SIZE 50                           /*size of rcb[] */
void process_request( int fd);
void sjf();

/*An array of request control blocks, which contain information for each request*/
struct requestTable{
  int sequence_num;                          /* sequence number of the request */          
  int file_descriptor;                                    /* file descriptor of the client*/
  FILE *handle;                              /* file handle of the file being requested */
  int bytes_remaining;                       /*number of bytes of the file that remain to be sent */
  int quantum;                               /* max number of bytes to be sent when the request is serviced */
} rcb[RCB_SIZE];



int request_num =0;                                   /*global sequence variable */

/* This function is where the program starts running.
 *    The function first parses its command line parameters to determine port #
 *    Then, it initializes, the network and enters the main loop.
 *    The main loop waits for a client (1 or more to connect, and then processes
 *    all clients by calling the seve_client() function for each one.
 * Parameters: 
 *             argc : number of command line parameters (including program name
 *             argv : array of pointers to command line parameters
 * Returns: an integer status code, 0 for success, something else for error.
 */
int main( int argc, char **argv ) {
  int port = -1;                                    /* server port # */
  int fd;                                           /* client file descriptor */
 char scheduler[5]={0};                             /* type of scheduler
  
  /* initialize the value of all elements in rcb[] to 0 or null*/
int i;
for(i=0; i<RCB_SIZE; i++){
  rcb[i].sequence_num=0;                          /* sequence number of the request */          
  rcb[i].file_descriptor=0;                                    /* file descriptor of the client*/
  rcb[i].handle=NULL;                              /* file handle of the file being requested */
  rcb[i].bytes_remaining=0;                       /*number of bytes of the file that remain to be sent */
  rcb[i].quantum=0;                          
}

  /* check for and process parameters
   */
  if( ( argc < 3 ) || ( sscanf( argv[1], "%d", &port ) < 1 )|| sscanf(argv[2], "%s", scheduler )<1) {
    printf( "usage: sms <port>\n" );
    return 0;
  }
  network_init( port );                             /* init network module */
  request_num=1;
  for( ;; ) {                                       /* main loop */
    network_wait();                                 /* wait for clients */

    for( fd = network_open(); fd >= 0; fd = network_open()) { /* get clients */
      process_request(fd);
      }
      if (strcmp(scheduler, "SJF") || strcmp(scheduler, "sjf")){
        sjf();
                               /* process each client */
    }
  }
}

/* This function takes a file handle to a client, reads in the request, 
 *    parses the request, and acquires information about the request that 
 *    will required by the scheduler.  This information is stored in rcb[].
 *    If the request is improper or the file is not available, the appropriate
 *    error is sent back.
 * Parameters: 
 *             fd : the file descriptor to the client connection
 * Returns: None
 */
  void process_request( int fd ) {
  static char *buffer;                              /* request buffer */
  char *req = NULL;                                 /* ptr to req file */
  char *brk;                                        /* state used by strtok */
  char *tmp;                                        /* error checking ptr */
  int len;                                          /* length of data read */
  int index;                                        /* index for rcb[] */

  for(index=0; index<RCB_SIZE; index++){
    if (rcb[index].sequence_num = 0)
      break;
  }

  if( !buffer ) {                                   /* 1st time, alloc buffer */
    buffer = malloc( MAX_HTTP_SIZE );
    if( !buffer ) {                                 /* error check */
      perror( "Error while allocating memory" );
      abort();
    }
  }

  memset( buffer, 0, MAX_HTTP_SIZE );
  if( read( fd, buffer, MAX_HTTP_SIZE ) <= 0 ) {    /* read req from client */
    perror( "Error while reading request" );
    abort();
  } 

  /* standard requests are of the form
   *   GET /foo/bar/qux.html HTTP/1.1
   * We want the second token (the file path).
   */
  tmp = strtok_r( buffer, " ", &brk );              /* parse request */
  if( tmp && !strcmp( "GET", tmp ) ) {
    req = strtok_r( NULL, " ", &brk );
  }
 
  if( !req ) {                                      /* is req valid? */
    len = sprintf( buffer, "HTTP/1.1 400 Bad request\n\n" );
    write( fd, buffer, len );                       /* if not, send err */
  } else {                                          /* if so, open file */
    req++;                                          /* skip leading / */
    rcb[index].handle = fopen( req, "r" );                        /* open file */
    if( !rcb[index].handle ) {                                    /* check if successful */
      len = sprintf( buffer, "HTTP/1.1 404 File not found\n\n" );  
      write( fd, buffer, len );                     /* if not, send err */
    } else {                                        /* if so, send success code */
      write( fd, buffer, len ); 
      len = sprintf( buffer, "HTTP/1.1 200 OK\n\n" );/* send success code */
      write( fd, buffer, len );

      fseek(rcb[index].file_descriptor, 0L, SEEK_END);
rcb[index].bytes_remaining = ftell(rcb[index].file_descriptor);

rewind(rcb[index].file_descriptor);
      rcb[index].sequence_num = request_num;                             
  rcb[index].file_descriptor = fd;
  fclose(rcb[index].handle); 
  request_num++;                                                                
  }
}
}

/* This function uses the 'shortest job first' algorithm to serve client requests 
 * Parameters: None
 * Returns: None
 */

  void sjf() {
      int i; 
      int index =1;
      
      /* find the request with the least amount of bytes*/
      for(i=2; i<request_num; i++){
        if (rcb[index].bytes_remaining > rcb[i].bytes_remaining && rcb[index].bytes_remaining>0 && rcb[i].bytes_remaining>0){
          index=i;
      }    
  }
      write( rcb[index].file_descriptor, rcb[index].handle, rcb[index].bytes_remaining);
      
      close(rcb[index].file_descriptor);                                     /* close client connection*/

      /* delete the request info from the RCB */
      rcb[index].sequence_num=0;                                  
      rcb[index].file_descriptor=0;                   
      rcb[index].handle=NULL;                            
      rcb[index].bytes_remaining=0;                     
      rcb[index].quantum=0;
      
}

