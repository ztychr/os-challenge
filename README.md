# Experiment for Johannes

## Introduction
For the individual experiment to conduct, I figured it would be interesting to implement a preemptive scheduling algorithm, 
which means that the CPU is assigned a fixed time or cycles, also known as a quantum, to work on a process.

To make matters more interesting, I decided to implement the round robin scheduling algorithm and defining the priority as the jobs to be cycled over.
This way, a variant of the round robin algorithm can be done where each process is assigned a varying time quantum based on it's priority.

## How it works
To create each priority element in the linked list, we are using the newly defined struct: `prio_struct`. The struct is as follows:

```C
typedef struct {
  void *next;
  crack_job_t *head;
  uint8_t priority;
} prio_struct;
```

```C
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
```

The void pointer `*next`, points the linked list element with the next highest priority value out of the elements remaining. 
The `*head` pointer points to a linked list of type `crack_job_t`, which resembles the jobs to be computed of that same priority.
Once all jobs from the `crack_job_t` linked list is computed, the `*head` pointer will be NULL and the program will work on the `prio_struct` element which the `*next` pointer points to.

To visualize the process, let's imagine the following packets and their corresponding priority are sent to the server:
Let *p<sub>n</sub>* be the packet number and *pr<sub>n</sub>* be the priority.

```bash
p1:pr1 -> p2:pr2 -> p3:pr2 -> p4:pr1 -> p5:pr2
```

Where the base program would only create a linked list of type `crack_job_t`, the round robin version will create a linked list of type `prio_struct`, each with a unique priority and a pointers to the next element. As mentioned above, each element also has a pointer to the list of jobs of type `crack_job_t` to compute with the same exact priority.

To visualize it, this is what the computingprocess would look like of the received packets listed above:

Let:
`prio_list_head` be the element of the highest priority
*P<sub>n</sub>* the element in the linked list with priority *n*
*J<sub>n</sub>* the element in the linked resembling the job coming with packet *n*

```bash
prio_list_head = P2

P2.next = P1
P2.head = J5
  -> J5.next = J3
    -> J3.next = J2
      -> J2.next = NULL
      
--> skip to P2.next = P1

P1.next = NULL 
  -> P1.head = J4
    -> J4.next = J1
      -> J1.next = NULL
    
--> skip to P1.next = NULL

--> DONE
```

## Benchmarking
I have not been able to run the provided vagrant virtual machines. 
Instead I've created a virtual machine with ressources much like the provided vagrant machines. 
The benchmarking for this experiment happened on the following system and with 512MB RAM available:

```bash
Architecture: x86_64
CPU op-mode(s): 32-bit, 64-bit
Byte Order: Little Endian
Address sizes: 39 bits physical, 48 bits virtual
CPU(s): 4
On-line CPU(s) list: 0-3
Thread(s) per core: 1
Core(s) per socket: 4
Model name: Intel(R) Core(TM) i5-8265U CPU @ 1.60GHz
```

As we are not implementing any functionality to cope with the repetition of hashes, but only with priorities, the benchmark is run with the following parameters:

- **seed**: 7974
- **total**: 100
- **start**: 1
- **difficulty**: 30000000 
- **rep_prob_percent**: 0
- **delay**: 750000
- **prio_lambda**: 0.5

First of, we'll do a threading benchmark, in order to find out what number of threads are performing the best.

To compile the experiments type `make` while in the folder named johannes. Three binaries should be compiled. The original server used for comparison, the round robin implementation, and a tweak of the round robin version, where the higher the priority, the more computational time is assigned.

We will run from 1 through 6 threads with the parameters listed above.
|           | threads 1 | threads 2 | threads 3 | threads 4 | threads 5 | threads 6 |
| --------- | --------- | --------- | --------- | --------- | --------- | --------- |
| **score** | 246160503 | 179106528 | 143192784 | 106073658 | 139860083 | 111756743 |

With a score of 106073658, we will continue benchmarking the round robin implementation with threads, matching the 4 cores.
The cycle changes once *n* hashes has been calculated. The amount of hashes to be computed before is defined by the `uint64_ variable` `this_stop`.

For scaleability, `this_stop` is assigned to *n* \* *NUMTHREADS*

### Methodology
To begin with, I've changed *n* to increase exponentially, starting with *10* through *1.000.000*.
When hitting what seemed to be top scorers, I've benchmarked a few numbers in between, to cover a wider area. 
Therefore the final top scorers are: *n = 10.000 \* NUMTHREADS*, *n = 50.000 \* NUMTHREADS*, *n = 100.000 \* NUMTHREADS* and *n = 200.000 \* NUMTHREADS*.
Each of the four top performers have been benchmarked 3 times each to calculate their average.

In the table below, let *nt* be denotion of *NUMTHREADS*.

|     10\*nt   |    100\*nt   |   1.000\*nt  |   10.000\*nt |   50.000\*nt |  100.000\*nt |  200.000\*nt |  500.000\*nt | 1.000.000\*nt|
| ------------ | ------------ | ------------ | ------------ | ------------ | ------------ | ------------ | ------------ | ------------ |
|   88604667   |   93529555   |   73596261   |   52496849   |   64061916   |   54555352   |   68117874   |   68220893   |   59060249   |
|              |              |              |   60166253   |   49656934   |   52219460   |   48754493   |              |              |
|              |              |              |   58586763   |   50328749   |   57186040   |   60133025   |              |              |
|              |              |              |**57083288.3**| **54682533** |**54653617.3**|**59001797.3**|              |              |

From the benchmark result, we can conclude that setting *n* to 100.000 yields the result of **54653617**, the best result yet. While the benchmarks with *n* set to 50.000 and 200.000, technically provided scores as low as **48754493**, the spread and consistency of *n* set to 100.000 is what made it the best option in the end.

For the weighted round robin experiment, this is what defined when a cycle should be invoked. 

```c
uint64_t this_stop = job->current[my_id] + (0x20000 / (256 - job->prio))*NUMTHREADS;
```

`this_stop` is determined by the job_id + hexnumber 0x20000 (131072 in decimal), divided by 256, minus the job priority, multiplied with the number of threads.

As an example, lets say my_id is 1012 and the priority is 3. The equation would then be: 1012 + (131072 / (256-3))\*4 = 3084.28
Wit the same example, but with a priority of 7: 1012 + (131072 / (256-7))\*4 = 3117.57

The higher priority job would get slighty more time to be computed before switching. After running the benchmarks of the round robin implementation, I can conclude that the equation, for now, does not return high enough numbers for a drastic difference to be made. Also the difference of the time between a high priority job and a lower priority, is most likely not enough for mesaureable effects.

The benchmark of the weighted round robin experiment is **213987250** and can not be considered a competitor to the round robin implementation.
For future work, it would be interesting to play around the the round robin weighted equation to see if it the parameters could be configured in such way, that if would be a better performing option.
