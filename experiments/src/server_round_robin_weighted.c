#define _DEFAULT_SOURCE // endian.h requires this macro to be defined
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <endian.h>
#include <assert.h>
#include <string.h>
#include <openssl/sha.h>
 //#include "messages.h"

//Creating one thread that constantly works
#define NUMTHREADS 1
#define printf(...) {}

//Union, including the original hash
typedef struct __attribute__((packed)) {
  uint8_t hash[32];
  uint64_t start;
  uint64_t stop;
  uint8_t p; //Priority
} packet_struct;

_Static_assert(sizeof(packet_struct) == 49, "not 49 bytes");

//Defining a structure, a list of all jobs
typedef struct {
  void * next;
  uint64_t found;
  int client_socket;
  uint8_t prio;
  uint8_t hash[32];
  uint64_t start;
  uint64_t stop;
  uint64_t current[NUMTHREADS];
} crack_job_t;

typedef struct {
  void *next;
  crack_job_t *head;
  uint8_t priority;
} prio_struct;


// Global pointer for all threads to see. points at most recent element of linked list
crack_job_t *job_list_head = NULL;
prio_struct *prio_list_head = NULL;

void *process_thread(void *threadArgument) {
  intptr_t my_id = (intptr_t) threadArgument; //
  SHA256_CTX context = {0}; // creates variable context of type SHA256_CTX, which is a struct, and sets all fields to 0
  uint8_t my_hash[32] = {0}; // creates an array of 32 bytes to contain the hash and sets all elements to 0 by list initializer
  crack_job_t *job = NULL; // creates a pointer to current job to keep track of which job we are working on
  prio_struct *p_cursor = NULL;

  while (1) {
    // TODO there needs to be a condition/broadcast here to wait for new jobs

    // are we still in the beginning of the program?
    p_cursor = prio_list_head;

    while (p_cursor != NULL) {
      job = p_cursor->head;

      while (job != NULL) {
	       if (job -> found == 0) {
	  //printf("my_id: %d start: %lu stop: %lu\n", my_id, job_list_head->start, job_list_head->stop);

	  if (job->current[my_id] == 0) {
	    job->current[my_id] = job->start + my_id;
	  }

	  // the chunk of work we do before the job stops is weighted based on the priority
	  uint64_t this_stop = job->current[my_id] + (0x20000 / (256 - job->prio))*NUMTHREADS;

	  if (this_stop > job->stop) {
	    this_stop = job->stop;
	  }

	  while ((!job -> found) && job->current[my_id] <= this_stop) {
	    printf("hash: %p\n", (void*) job);
	    SHA256_Init( &context);
	    uint64_t payload = htole64(job->current[my_id]);
	    SHA256_Update( &context, &payload, sizeof(payload));
	    SHA256_Final(my_hash, &context);
	    if (0 == memcmp(my_hash, job -> hash, 32)) {

	      printf("FOUND!\n");
	      //printf("hash: ");
	      //for(int i = 0; i < 32 ; i++) {
	      //printf("%02x", (unsigned char)(((char*)&my_hash)[i]));}
	      //printf("\n");
	      job -> found = job->current[my_id];
	      if (job -> client_socket) {
		payload = htobe64(job->current[my_id]);
		send(job -> client_socket, &payload, sizeof(payload), MSG_NOSIGNAL);
	      }
	    } // memcompare if statement end
	    job->current[my_id] += NUMTHREADS;
	  } // while !job->found end loop
	  //printf("%d trying %lu\n", my_id, idx);
	} //job found == 0
	job = job -> next;
      } // while job!=NULL loop end
      p_cursor = p_cursor->next;
    } // while p_cursor!=NULL loop end
  }
  pthread_exit(NULL);
}

void printdebug() {
  prio_struct *p_cursor = prio_list_head;
  while(p_cursor != NULL) {
    printf("\nprio [%u] ", p_cursor->priority);
    crack_job_t *j_cursor = p_cursor->head;
    while(j_cursor != NULL) {
      printf("-> %lu[%u]", j_cursor->start, j_cursor->prio);
      j_cursor = j_cursor->next;
    }
    p_cursor = p_cursor->next;
  }
  printf("\n");
}

//Main
int main(int argc, char * argv[]) {
  // create socket with a4ddress family type, stream socket and tcp
  //printf("V1.09\n");

  if (argc < 2) {
    printf("usage: ./server PORTNUMBER\n");
    exit(1);
  }

  // Creating a tcp server
  int server_socket;
  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  // Choosing a portnumber
  int portno = atoi(argv[1]); //Change an string(int) to an int
  int yes = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  // Create an instant of the new thread
  int error;
  pthread_t threads[NUMTHREADS]; //Create a constant by using pthread

  // We create process threads < Numthreads
  for (intptr_t i = 0; i < NUMTHREADS; i++) {
    error = pthread_create( &
      threads[i], // where to store thread id pointer
      NULL, // initialize thread with default attr
      process_thread, //
      (void *) i); //It creates names of each thread.
    if (error) {
      printf("Error:unable to create thread, %d\n", error);
      exit(-1);
    }
  }

  // server address
  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(portno);
  server_address.sin_addr.s_addr = inet_addr("0.0.0.0");

  // bind socket
  int bind_code = bind(server_socket, (struct sockaddr * ) &server_address, sizeof(server_address));
  if (bind_code != 0) {
    printf("binding failed\n");
    exit(1);
  }

  // listen for max 50 connections
  int list_code = listen(server_socket, 50);
  if (list_code != 0) {
    printf("listen failed\n");
  }

  // accept the incoming connection
  struct sockaddr_in client_address;
  int clen = sizeof(client_address);

  while (1) {
    int client_socket = accept(server_socket, (struct sockaddr * ) &client_address, (socklen_t * ) &clen);
    if (-1 == client_socket) {
      exit(10);
      break;
    }

    packet_struct packet_pack; // An experiment here could be to receive data into a crack_job_t directly
    if (recv(client_socket, (unsigned char * ) &packet_pack, sizeof(packet_pack), 0) != sizeof(packet_pack)) {
      exit(5);
    }

    crack_job_t * job = calloc(1, sizeof(crack_job_t)); // allocates memory and makes sure memory is set to 0
    if (NULL == job) {
      exit(4);
    }

    job->client_socket = client_socket;
    job->next = NULL;
    job->prio = packet_pack.p;
    job->start = be64toh(packet_pack.start);
    job->stop = be64toh(packet_pack.stop);

    memcpy(job->hash, packet_pack.hash, 32);

    // printf("start: %lu: \n", job->start);
    // Here it could make sense to run through job_list_head to see if the job hash aldready exists.
    //    while (! job->next == NULL) {
    //  if (job->hash ==
    // }

    // prio cursor is like the iterator in a for loop
    // prio_nearest keeps control of which prioroty is closest to the job->prio we are currently accepting
    prio_struct *prio_cursor = prio_list_head;
    prio_struct *prio_nearest = prio_list_head;

    while (prio_cursor != NULL)  {
      if (prio_cursor->priority >= job->prio) {
	prio_nearest = prio_cursor;

      } //else { prio_cursor = NULL; }
      prio_cursor = prio_cursor->next;
    }

    if (prio_nearest != NULL && prio_nearest->priority != job->prio) {
      prio_struct *this_prio = calloc(1, sizeof(*this_prio));
      printf("allocating new in: (prio_nearest != NULL && prio_nearest->priority: %u != job->prio: %u) \n",
	     prio_nearest->priority, job->prio);
      this_prio->priority = job->prio;
      this_prio->head = job;

      if (prio_nearest->priority < job->prio) {
	this_prio->next = prio_nearest;
	prio_list_head = this_prio;
      } else {
	this_prio->next = prio_nearest->next;
	prio_nearest->next = this_prio;
      }
    }

    if (prio_nearest == NULL) {
      prio_struct *this_prio = calloc(1, sizeof(*this_prio));
      printf("allocating new in: if (prio_nearest == NULL) with priority %u \n", job->prio);
      this_prio->priority = job->prio;
      this_prio->head = job;
      this_prio->next = NULL;
      prio_list_head = this_prio;
    }

    if (prio_nearest != NULL && prio_nearest->priority == job->prio) {
      printf("inserting new job:\n");
      job->next = prio_nearest->head;
      prio_nearest->head = job;
    }


    /* take lock */
    //pthread_mutex_lock();
    //job_list_head = job;
    printdebug();
    /* release lock */
    //pthread_mutex_unlock();
  }
  return 0;
}
