#define _POSIX_C_SOURCE 200809L 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include "pow.h"

#define SIZE 256
#define CONTINUE 8
#define MESSAGE 22

int worker_actions(int secs, int num_threads, int reader, int writer);

typedef struct datos
{
  double objective;
  double from;
  double to;
}Datos;