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

// defining how many threads we should use
#define NUMTHREADS 1

// struct to contain the data of the received packet from the client. we are using __attribute__((packed)) to avoid trailing padding
typedef struct __attribute__((packed)) {
  uint8_t hash[32]; // hash
  uint64_t start;   // start 
  uint64_t stop;    // stop
  uint8_t p;        // priority
} packet_struct;

// here we are checking whether the received packet is actually 49 bytes. if not, we throw it out
_Static_assert(sizeof(packet_struct) == 49, "not 49 bytes");

// struct used to create a linked list of all jobs
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

// struct type to keep track of each priority
typedef struct {
  void *next; // points to the next priority struct
  crack_job_t *head; // points to the next job to crack
  uint8_t priority; // stores the priority
} prio_struct;

// a global pointer; job_list_head, which  points to the most recent element of linked list
crack_job_t *job_list_head = NULL;

// a global pointer of type prio_struct which points to the most recent added element of the priority linked list 
prio_struct *prio_list_head = NULL;

// this function is for each new thread to go through 
void *process_thread(void *threadArgument) {
  intptr_t my_id = (intptr_t) threadArgument; //
  SHA256_CTX context = {0}; // allocates and creates empty variable 'context', of type SHA256_CTX: a struct, and sets all fields to 0
  uint8_t my_hash[32] = {0}; // creates an array of 32 bytes to contain the hash and sets all elements to 0 by list initializer
  crack_job_t *job = NULL; // pointer named job of type crack_job_t which is the job we are currently working on
  prio_struct *p_cursor = NULL; // creates a pointer p_cursor of type prio_struct, to keep track of which priority we are currently working on  
  
  while (1) {
    // TODO there needs to be a condition/broadcast here to wait for new jobs

    // set current priority, p_cursor, to the last added element of the priority linked list
    p_cursor = prio_list_head;

    while (p_cursor != NULL) {
      job = p_cursor->head;

      while (job != NULL) {
        if (job->found == 0) {

	  // if the job->current is 0, we add the my_id paramter to the start in order to avoid the threads working on the same hashes.
	  // e.g. with a start of 1000, thread 1 will start working on 1000, thread 2 on 1001, thread 3 on 1002 and so on. 
          if (job->current[my_id] == 0) {
            job->current[my_id] = job->start + my_id;
          }
	  
          uint64_t this_stop = job->current[my_id] + 100000*NUMTHREADS;
	  
          if (this_stop > job->stop) {
            this_stop = job->stop;
          }
	  
	  // if the hash is not found and the job->current is less or equal to the stop, we calculate the hash if the current number
	  while ((!job->found) && job->current[my_id] <= job->stop) {
	    SHA256_Init(&context); // initializes context, the SHA256_CTX struct
	    uint64_t payload = htole64(job->current[my_id]); // just in case the program is not working on a little endian pc, we convert the current number being worked on from host to little endian
	    SHA256_Update(&context, &payload, sizeof(payload)); // run over and update the data to be hashed 
	    SHA256_Final(my_hash, &context); // extracts the hash and deletes the SHA256_CTX struct
	    
	    // memcmp compares the newly computed hash to the hash received by the client and if it's found, it sets job->found to current number being worked on
	    if (0 == memcmp(my_hash, job->hash, 32)) {
	      job->found = job->current[my_id];
	      
	      // if the job->client_socket is set, we convert to big endian and send the result back to the client
	      if (job->client_socket) {
		payload = htobe64(job->current[my_id]);
		send(job -> client_socket, &payload, sizeof(payload), MSG_NOSIGNAL);
	      }
	    } // memcmp if statement end
	    job->current[my_id] += NUMTHREADS;
	  } // while !job->found && job->current[my_id] <= job->stop loop end
	} // while job->found == 0 loop end
	job = job->next;
      }
      p_cursor = p_cursor->next;
    }
  }
  pthread_exit(NULL);
}

// DEBUG PRINT //
/*
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
*/

// main program taking arguments
int main(int argc, char * argv[]) {
    //printf("Running server v1.1\n");

  // if portnumber to listen on is not provided, exit program with exit code 1 
  if (argc < 2) {
    printf("usage: ./server PORTNUMBER\n");
    exit(1);
  }

  // initializing the server socket
  int server_socket;
  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  // setting socket option
  int portno = atoi(argv[1]); //Change an string(int) to an int
  int yes = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  // create an instant of the new thread
  int error;
  pthread_t threads[NUMTHREADS]; // create an array threads of type pthread_t that is NUMTHREADS long

  // we create process threads < Numthreads
  for (intptr_t i = 0; i < NUMTHREADS; i++) {
    error = pthread_create(
			   &threads[i], // where in the array to store the thread id pointer
			   NULL, // initialize thread with default attributes
			   process_thread, // 
			   (void *) i); // creates names of each thread

    // if int error is the return value of thread creation and if it is ! 0, we exit with exit code -1
    if (error) {
      printf("Error:unable to create thread, %d\n", error);
      exit(-1);
    }
  }

  // initializing server_address of type struct sockaddr_in and assigning values to it's elements
  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET; // address family internet
  server_address.sin_port = htons(portno); // portnumber from host byte order to network byte order
  server_address.sin_addr.s_addr = inet_addr("0.0.0.0"); // accept connections from all ip's and convert ip address format to binary network byte order magic stuff

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

  // initialize client_address of type sockaddr_in
  struct sockaddr_in client_address;
  int clen = sizeof(client_address);

  // always accept client connections
  while (1) {
    int client_socket = accept(server_socket, (struct sockaddr * ) &client_address, (socklen_t * ) &clen);
    if (-1 == client_socket) {
      exit(10);
      break;
    }
    
    packet_struct packet_pack; // an interesting experiment could be to receive data into a crack_job_t directly instead of loading it into a packet_pack struct
    if (recv(client_socket, (unsigned char *) &packet_pack, sizeof(packet_pack), 0) != sizeof(packet_pack)) {
      exit(5);
    }

    crack_job_t *job = calloc(1, sizeof(crack_job_t)); // allocates memory for a new job and sets memory to 0 by using calloc instead of malloc
    if (NULL == job) {
      exit(4);
    }

    // load the values of the received packet into the newly created job of type crack_job_t
    // furthermore the start and stop numbers must be converted from network byte order (big endian) to host byte order (little endian)
    job->client_socket = client_socket;
    job->next = NULL;
    job->prio = packet_pack.p;
    job->start = be64toh(packet_pack.start);
    job->stop = be64toh(packet_pack.stop);

    // copy the received hash from packet_pack into job->hash in the crack_job_t
    memcpy(job->hash, packet_pack.hash, 32);
    // it would make sense to run through job_list_head here to see if the job hash aldready exists (caching experiment)

    // prio cursor is like the iterator in a for loop
    // prio_nearest keeps control of which prioroty is closest to the job->prio we are currently accepting
    prio_struct *prio_cursor = prio_list_head;
    prio_struct *prio_nearest = prio_list_head;

    
    // this loop is to find the place in the linked list of prio_structs where our new job should be inserted into an existing prio_struct or a new prio_struct to be created 
    // CASE 1
    while (prio_cursor != NULL)  {
      // CASE 1.1
      if (prio_cursor->priority >= job->prio) {
        prio_nearest = prio_cursor; 
	prio_cursor = prio_cursor->next;
      }
      // CASE 1.2
      else { prio_cursor = NULL; } // we'll set prio_cursor to NULL as the place in the list is found
    }

    // this case is used in case there is no existing prio_struct that matches the job->prio AND the list is not empty
    // CASE 2
    if (prio_nearest != NULL && prio_nearest->priority != job->prio) {
      prio_struct *this_prio = calloc(1, sizeof(*this_prio));
      //printf("allocating new in: (prio_nearest != NULL && prio_nearest->priority: %u != job->prio: %u) \n",  prio_nearest->priority, job->prio);                                                                
      this_prio->priority = job->prio;
      this_prio->head = job;

      // this case is used the list is not empty AND if no other elements are bigger than us
      // CASE 2.1
      if (prio_nearest->priority < job->prio) {
        this_prio->next = prio_nearest; // prio_nearest is the same as prio_list_head
        prio_list_head = this_prio;
      }
     
      else {
        this_prio->next = prio_nearest->next;
        prio_nearest->next = this_prio;
      }
    }

    // if prio_nearest is NULL, we callocate a new type prio_struct called this_prio
    // CASE 3 
    if (prio_nearest == NULL) {
      prio_struct *this_prio = calloc(1, sizeof(*this_prio));

      this_prio->priority = job->prio; // the new struct this_prio's priority field is assigned the current jobs->prio
      this_prio->head = job; // this_prio->head is assigned to the current job
      this_prio->next = NULL; // since this is the first one, we don't assign the next in the linked list
      prio_list_head = this_prio; // lastly the empty pointer initialized above is set to this_prio
    }

    // this case is used when the priority struct for the current job's priority already exists
    // CASE 4
    if (prio_nearest != NULL && prio_nearest->priority == job->prio) {
      job->next = prio_nearest->head;
      prio_nearest->head = job;
    }

    // printdebug();

    /* take lock */
    //pthread_mutex_lock();
    /* release lock */
    //pthread_mutex_unlock();
  }
  return 0;
}
