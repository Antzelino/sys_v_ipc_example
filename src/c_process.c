#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/common.h"


int main(int argc, char* argv[]) {
  int M = atoi(argv[1]);
  int array[M];
  int n = atoi(argv[2]);
  int my = atoi(argv[3]);
  struct sembuf wait_my_mutex={my, -1, 0}; // for semop, to wait on my-th mutex
  double moving_avg = 0.0; // in units of milisec

  FILE* fd = fopen("out.txt", "a");

  // 1. Gain access to semaphore set

    // 1.a Semaphore set key
  key_t sem_key;
  key_t nsem_key;
  errorcheck(sem_key = ftok("./src/feeder.c", 'A'), "ftok");
  errorcheck(nsem_key = ftok("./src/c_process.c", 'N'), "ftok");

    // 1.b Semaphore set && its ID
  int sem_id;
  int nsem_id;
  errorcheck(sem_id = semget(sem_key, 2, 0666), "child: semget");
  errorcheck(nsem_id = semget(nsem_key, n, 0666), "child: semget");


  // 2. Gain access to the shared memory

    // 2.a Shared memory key
  key_t shm_key;
  errorcheck(shm_key = ftok("./src/feeder.c", 'R'), "ftok");

    // 2.b Shared memory && its ID
  int shm_id;
  errorcheck(shm_id = shmget(shm_key, shm_size, 0666), "shmget");

    // 2.c Attach to shared memory && get the pointer
  char* shm_ptr;
  errorcheck((int)(long)(shm_ptr = shmat(shm_id, NULL, 0)), "shmat");


  // 3. Read shm data
  struct timespec feeder_tp;
  struct timespec tp;
  clockid_t clk_id = CLOCK_MONOTONIC;
  long sec2msec;
  double nsec2msec;
  int i, j;
  for (i = 0; i < M; i++) {

      // 3.a Wait my mutex. Prevents from reading i-th int more than once
    errorcheck(semop(nsem_id, &wait_my_mutex, 1), "semop"); // wait my mutex

      // 3.b Change num of readers and block writer for n reads (1 from each)
    errorcheck(semop(sem_id, &wait_mutex, 1), "semop"); // block mtx_rsrce
    errorcheck(semop(sem_id, &signal_read, 1), "semop"); // -1 reader
    if ((j = semctl(sem_id, SEM_READ, GETVAL, 0)) == (2*n-1)) { // if 1st reader
      errorcheck(j, "semctl");
      errorcheck(semop(sem_id, &wait_resource, 1), "semop"); // block rsrce
    }
    errorcheck(semop(sem_id, &signal_mutex, 1), "semop"); // unblock mtx_rsrce

      // 3.c Get timestamp and integer from shared memory
    clock_gettime(clk_id, &tp);
    memcpy(&feeder_tp, shm_ptr, sizeof(struct timespec));
    memcpy(&array[i], shm_ptr+sizeof(struct timespec), sizeof(int));

      // 3.d Change num of readers and let writer write next integer
    errorcheck(semop(sem_id, &wait_mutex, 1), "semop"); // wait mtx_rsrce
    if ((j = semctl(sem_id, SEM_READ, GETVAL, 0)) == 1) { // if last reader
      errorcheck(j, "semctl");
      errorcheck(semop(sem_id, &signal_read, 1), "semop"); // -1 reader
      errorcheck(semop(sem_id, &signal_resource, 1), "semop"); // unblock rsrce
    }
    else{
      errorcheck(semop(sem_id, &signal_read, 1), "semop"); // -1 reader
    }
    errorcheck(semop(sem_id, &signal_mutex, 1), "semop"); // unblock mtx_rsrce

      // 3.e Calculate moving average
    sec2msec = (tp.tv_sec - feeder_tp.tv_sec)*1000; // sec -> msec
    nsec2msec = (tp.tv_nsec - feeder_tp.tv_nsec)/1000000.0; // nsec -> msec
    moving_avg = ((sec2msec + nsec2msec) + i*moving_avg)/(i+1.0); // msec avg
  }


  // 4. Prints and logs
  errorcheck(semop(sem_id, &wait_mutex, 1), "semop"); // let only one print
  printf("pid: %d\navg: %f milisec\n\n", getpid(), moving_avg); // print
  fprintf(fd, "pid: %d\narr: ", getpid()); // log pid
  for (i = 0; i < M; i++) {
    fprintf(fd, "%d ", array[i]); // log array
  }
  fprintf(fd, "\navg: %f milisec\n\n", moving_avg); // log moving avg
  errorcheck(semop(sem_id, &signal_mutex, 1), "semop"); // now let next print


  // 5. Detach from shm && close file
  errorcheck(shmdt(shm_ptr), "shmdt");
  fclose (fd);

  return 0;
}
