/* 
 * File: sws.c
 * Authors: Alex Brodsky.  Modified by Sean Mahoney and Will Wilson
 * Purpose: This file contains the implementation of a simple web server.
 *          It consists of five functions: 
 *           Main() contains the main loop that accepts client connections.
 *          process_request() gathers and stores information pertaining to 
 *           each client request.
 *          sjf() uses the 'shortest job first' algorithm to service requests.
 *           roundRobin() uses the 'round robin' algorithm' to service requests.
 *           ...multi level feedback....     
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "network.h"
#include <sys/queue.h>

#define MAX_HTTP_SIZE 8192                 /* size of buffer to allocate */
#define RCB_SIZE 100                       /*size of rcb[] */
#define roundByte 8192                     /* how many bytes are sent a round for round robin*/
#define QUEUE_LIMIT 100                     /*max size of the worker_queue */ 

void process_request( int fd);
void sjf();
void roundRobin();
void* threads_work();
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*An array of request control blocks, which contain information for each request*/
static struct requestTable{
  int sequence_num;                          /* sequence number of the request */          
  int file_descriptor;                                    /* file descriptor of the client*/
  FILE *handle;                              /* file handle of the file being requested */
  int bytes_remaining;                       /*number of bytes of the file that remain to be sent */
  } rcb[RCB_SIZE];

  struct node{                                     /*struct for the work_queue */
    TAILQ_ENTRY(node) tailq;
    int fd;                                         /* client's file descriptor will be inserted into the queue */
  }file_d;
  
TAILQ_HEAD(q ,node) head;
  pthread_t *workers;
  int request_num =0;                                   /*global sequence variable */
  int work_queue_size=0;                            /*current number of requests in the work queue */
  int waiting_for_scheduler=0;                        /*number of requests that were processed and waiting in the scheduler queue*/  
  char scheduler[5]={0};                            /* type of scheduler*/
/*    This function is where the program starts running.
 *    The function first parses its command line parameters to determine port #
 *    and the user's choice of scheduler.  Then, it initializes, the network and
 *    enters the main loop.  The main loop waits for a client (1 or more to connect),
 *    and then processes all clients' information by calling the process_request() 
 *    function for each one.  Next, depending on the user's input, it uses one of 
 *    three shceduling algorithms to service each request. 
 * 
 *    Parameters: 

 *             argc : number of command line parameters (including program name)
 *             argv : array of pointers to command line parameters
 *           
 * Returns: an integer status code, 0 for success, something else for error.
 */
int main( int argc, char **argv ) {
  int port = -1;                                    /* server port number */
  int number_of_threads;                            /* number of threads specified by the user */
  int fd = -1;                                      /* client's file descriptor  */
  
  TAILQ_INIT(&head);                          /*initialize the worker queue */

  /* initialize the value of all elements in rcb[] to 0 or null */
int i;
for(i=0; i<RCB_SIZE; i++){
  rcb[i].sequence_num=0;                                    
  rcb[i].file_descriptor=0;                       
  rcb[i].handle=NULL;                             
  rcb[i].bytes_remaining=0;                                               
}

  /* check for and process parameters */
  if( (argc < 4)  ||  (sscanf( argv[1], "%d", &port ) < 1)  || (sscanf(argv[2], "%s", scheduler )<1) || (sscanf(argv[3], "%d", &number_of_threads)<1)) {
    printf( "usage: sms <port>\n" );
    return 0;
  }
  

  network_init( port );                             /* init network module */
  pthread_attr_t attr;                              /* thread atributes */
  pthread_attr_init(&attr);                         /* set default attributes */
  workers = (pthread_t *)malloc(number_of_threads * sizeof(pthread_t));
  
  for(i=0; i<number_of_threads;i++){
    pthread_create(&workers[i], &attr, threads_work, NULL);
  }
  
  request_num = 1;
  for( ;; ) {                                       /* main loop */
    network_wait();                                 /* wait for clients */

    for(fd = network_open(); fd >= 0 && work_queue_size<= QUEUE_LIMIT; fd = network_open()) { /* get clients */
      struct node*e = malloc(sizeof(struct node));
        if (e == NULL)
        {
            fprintf(stderr, "malloc failed");
            exit(EXIT_FAILURE);
        }
        e->fd=fd;
      TAILQ_INSERT_TAIL(&head, e, tailq);                                     
      work_queue_size++;
      
      }
  }
}


void* threads_work(){
  for(;;){
    struct node*e;
    int request=-1;                              /*file descriptor of the client's request */
    while(work_queue_size>0){
     pthread_mutex_lock(&mutex); 
     TAILQ_REMOVE(&head, e, tailq);
     request= e->fd;
     free(e);
     e=NULL;
     work_queue_size--;

     pthread_mutex_unlock(&mutex); 
     process_request(request);
    }
    while(waiting_for_scheduler>0){
       /* The requests are serviced by one of the three scheduling algorithms,
      which is determined by the user's input at run time */

      if (strcmp(scheduler, "SJF") || strcmp(scheduler, "sjf")){
        sjf();                      
    }
      else if (strcmp(scheduler, "RR") || strcmp(scheduler, "rr")){
        roundRobin();  
    }
    } 
  }
}
/* This function takes a file descriptor to a client, reads in the request, 
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

  pthread_mutex_lock(&mutex);
  for(index=0; index<RCB_SIZE; index++){
    if (rcb[index].sequence_num == 0)
      rcb[index].sequence_num = request_num; 
      break;
  }
  pthread_mutex_unlock(&mutex);
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
  } 

  else {                                          /* if so, open file */
    req++;                                          /* skip leading / */
    rcb[index].handle = fopen( req, "r" );                        /* open file */
    if( !rcb[index].handle ) {                                    /* check if successful */
      len = sprintf( buffer, "HTTP/1.1 404 File not found\n\n" );  
      write( fd, buffer, len );                     /* if not, send err */
    } 

    else {                                           /* if so, send success code */
      len = sprintf( buffer, "HTTP/1.1 200 OK\n\n" );/* send success code */
      write( fd, buffer, len );

      //getting the size of the file requested 
      fseek(rcb[index].handle, 0L, SEEK_END);
rcb[index].bytes_remaining = ftell(rcb[index].handle);

rewind(rcb[index].handle);
  fclose(rcb[index].handle);                                
  rcb[index].file_descriptor = fd;
  request_num++;   
  printf("Request for file ");
  waiting_for_scheduler++;                                                             
  }
}
}

/* This function uses the 'shortest job first' algorithm to serve client requests 
 * Parameters: None
 * Returns: None
 */

  void sjf() {
      int i; 
      int index =0;
      
      /* find the request with the least amount of bytes*/
      for(i=1; i<RCB_SIZE; i++){
        if (rcb[index].bytes_remaining > rcb[i].bytes_remaining && rcb[index].bytes_remaining>0 && rcb[i].bytes_remaining>0){
          index=i;
      }    
  }
      if(write( rcb[index].file_descriptor, rcb[index].handle, rcb[index].bytes_remaining)<0)
        printf("Error writing to socket");
      
      close(rcb[index].file_descriptor);                                     /* close client connection*/
      pthread_mutex_lock(&mutex);
      /* delete the request info from the RCB */
      rcb[index].sequence_num=0;                                  
      rcb[index].file_descriptor=0;                   
      rcb[index].handle=NULL;                            
      rcb[index].bytes_remaining=0;       
      waiting_for_scheduler--;                    
      pthread_mutex_unlock(&mutex);
}
/* This function uses the 'Round Robin' algorithm to serve client requests. 
 * Parameters: None
 * Returns: None
 */

  void roundRobin() {
    int i; 
      
     /* Cycle through the array*/
    for(i=1; i<RCB_SIZE; i++){
      if (rcb[i].bytes_remaining> roundByte) { // Does the current RCB need more than its allowed in a quantum?
        write(rcb[i].file_descriptor, rcb[i].handle, roundByte ); //If so, only send it 8kb and move on.
      } 
      else {  // RCB at i is less than the remaining amount. Just send it that much data, then close it off
        write(rcb[i].file_descriptor, rcb[i].handle, rcb[i].bytes_remaining); //Give it the last bytes. No reason to overflow/take up the entire quantum.
        close(rcb[i].file_descriptor); //Close it off
       
        pthread_mutex_lock(&mutex);        //Delete it.
        rcb[i].sequence_num=0;                                  
        rcb[i].file_descriptor=0;                   
        rcb[i].handle=NULL;                            
        rcb[i].bytes_remaining=0; 
        waiting_for_scheduler--;                     
        pthread_mutex_unlock(&mutex);
      }
    }
  }
