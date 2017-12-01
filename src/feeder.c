#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/common.h"



int main(int argc, char* argv[]) {
  // Error checking before execution
  if (argc != 5) {
    printf("Error: Wrong number of arguments. Synopsis:\n");
    printf("./build/run [%s-M%s %sarray_size%s]", b_1, b_0, ul_1, ul_0);
    printf(" [%s-n%s %sprocesses%s]\n", b_1, b_0, ul_1, ul_0);
    return 1;
  }


  // Initializations of 'M' and 'n' and error checking
  srand(time(NULL));
  int i, j, M, n;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-M") == 0 || strcmp(argv[i], "-m") == 0) {
      M = atoi(argv[++i]);
    }
    else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "-N") == 0) {
      n = atoi(argv[++i]);
    }
    else {
      printf("Error: Found wrong argument in command line. Synopsis:\n");
      printf("./build/run [%s-M%s %sarray_size%s]", b_1, b_0, ul_1, ul_0);
      printf(" [%s-n%s %sprocesses%s]\n", b_1, b_0, ul_1, ul_0);
      return 1;
    }
  }


  // 0. Initialize array with random integers
  int array[M];
  for (i = 0; i < M; i++) {
    array[i] = rand();
    printf("[%d]=%d\n",i, array[i]);
  }
  printf("\n");


  // 1. Create semaphore sets (one rsrce, read, mtx and one with n mutexes)

    // 1.a Semaphore sets's keys
  key_t sem_key;
  key_t nsem_key;
  errorcheck(sem_key = ftok("./src/feeder.c", 'A'), "ftok");
  errorcheck(nsem_key = ftok("./src/c_process.c", 'N'), "ftok");

    // 1.b Semaphore sets && their IDs
  int sem_id;
  int nsem_id;
  errorcheck(sem_id = semget(sem_key, 3, 0666 | IPC_CREAT), "semget");
  errorcheck(nsem_id = semget(nsem_key, n, 0666 | IPC_CREAT), "semget");

    // 1.c Initialize semaphore sets
  union semun mysemun;
  mysemun.val = 1; // init rsrc and mtx at 1
  errorcheck(semctl(sem_id, SEM_RESOURCE, SETVAL, mysemun), "semctl");
  errorcheck(semctl(sem_id, SEM_MUTEX, SETVAL, mysemun), "semctl");
  mysemun.val = 2*n; // init at 2*n (feeder waits for 0)
  errorcheck(semctl(sem_id, SEM_READ, SETVAL, mysemun), "semctl");
  union semun arg;
  unsigned short values[n];
  for (i = 0; i < n; i++) {
    values[i] = 0;
  }
  arg.array = values; // initialize nsem set's semaphores to 0
  errorcheck(semctl(nsem_id, 0, SETALL, arg), "semctl");


  // 2. Create shared memory

    // 2.a Shared memory key
  key_t shm_key;
  errorcheck(shm_key = ftok("./src/feeder.c", 'R'), "ftok");

    // 2.b Shared memory && its ID
  int shm_id;
  errorcheck(shm_id = shmget(shm_key, shm_size, 0666 | IPC_CREAT), "shmget");

    // 2.c Attach to shared memory && get the pointer
  char *shm_ptr;
  errorcheck((int)(long)(shm_ptr = shmat(shm_id, NULL, 0)), "shmat");


  // 3. Child processes
  for (i = 0; i < n; i++) { // fork n children processes
    pid_t pid;
    if ((pid = fork()) == -1) {
      perror("fork error");
      exit(-1);
    }
    else if (pid == 0) { // if in child process
      char arrsize[40];
      char nsize[40];
      char proc_num[40];
      sprintf(arrsize, "%d", M); // pass M
      sprintf(nsize, "%d", n); // pass n
      sprintf(proc_num, "%d", i); // pass process number (useful for its mutex)
      char *arguments[] = {"./build/client", arrsize, nsize, proc_num, NULL};
      errorcheck(execvp(arguments[0], arguments), "execvp");
    }
  }


  // 4. Write array elements one-by-one in shared memory
  struct timespec tp;
  clockid_t clk_id = CLOCK_MONOTONIC;
  struct sembuf sig_one_mutex[n]; // array of sembufs. signal someone's mutex
  for (j = 0; j < n; j++) {
    sig_one_mutex[j].sem_num = (unsigned short) j;
    sig_one_mutex[j].sem_op = 1;
    sig_one_mutex[j].sem_flg = 0;
  }

  for (i = 0; i < M; i++) {
      // 4.a Block readers from shared memory so I can write to it
    errorcheck(semop(sem_id, &wait_resource, 1), "semop"); // wait to write

      // 4.b Write one element to shared memory
    clock_gettime(clk_id, &tp);
    memcpy(shm_ptr, &tp, sizeof(struct timespec));
    memcpy(shm_ptr+sizeof(struct timespec), &array[i], sizeof(int));

      // 4.c Unblock readers so they can access shared memory
    errorcheck(semop(sem_id, &signal_resource, 1), "semop"); // let readers read

      // 4.d Let each of them read it once
    for (j = 0; j < n; j++) { // signal each process's mutex
      errorcheck(semop(nsem_id, &sig_one_mutex[j], 1), "semop");
    }

      // 4.e Wait until they all finish reading
    errorcheck(semop(sem_id, &wait_read, 1), "semop"); // wait readers to finish

    mysemun.val = 2*n; // set read_sem to 2*n again because it is back to 0
    errorcheck(semctl(sem_id, SEM_READ, SETVAL, mysemun), "semctl");
  }

  int status;
  while(waitpid(-1, &status, 0) != -1); // feeder waits for all children


  // 5. Detach feeder process from shared memory
  errorcheck(shmdt(shm_ptr), "shmdt");


  // 6. Deallocate shared memory
  errorcheck(shmctl(shm_id, IPC_RMID, 0), "shmctl");


  // 7. Deallocate semaphore sets
  errorcheck(semctl(sem_id, 0, IPC_RMID), "semctl");
  errorcheck(semctl(nsem_id, 0, IPC_RMID), "semctl");

  return 0;
}
