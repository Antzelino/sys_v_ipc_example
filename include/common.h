#ifndef COMMON_H
#define COMMON_H

#include <sys/sem.h>

#define SEM_RESOURCE 0
#define SEM_READ 1
#define SEM_MUTEX 2
#define SZ_TIMESTAMP sizeof(struct timespec)
#define SZ_DATA sizeof(int)

/*
* Escape sequences for BASH
*/
char* ul_1 = "\e[4m";  // enable underline
char* ul_0 = "\e[24m"; // disable underline
char* b_1 = "\e[1m";   // enable bold
char* b_0 = "\e[0m";   // disable bold

union semun {
  int val;
  struct semid_ds *buf;
  ushort *array;
};

size_t shm_size = (size_t) (SZ_DATA + SZ_TIMESTAMP); // size of shared memory

extern inline void errorcheck(int val, char* errorprint){
  val == -1 ? perror(errorprint), exit(1) : NULL;
}

struct sembuf wait_resource={SEM_RESOURCE, -1, 0}; // wait -> unblock -> down 1
struct sembuf signal_resource={SEM_RESOURCE, 1, 0}; // up 1
struct sembuf signal_read={SEM_READ, -1, 0}; // down 1
struct sembuf wait_read={SEM_READ, 0, 0}; // wait until 0 -> unblock
struct sembuf wait_mutex={SEM_MUTEX, -1, 0}; // wait -> unblock -> down 1
struct sembuf signal_mutex={SEM_MUTEX, 1, 0}; // up 1



#endif // COMMON_H
