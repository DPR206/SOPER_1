#define _POSIX_C_SOURCE 200809L 
#define _DEFAULT_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
#define MESSAGE 29

#define MUTEX_PID_NAME "/mutex1"
#define MUTEX_TARGET_NAME "/mutex2"
#define MUTEX_WINNER_NAME "/mutex3"
#define MUTEX_ROUND_NAME "/mutex4"
#define MUTEX_VOT_NAME "/mutex5"

#define FILE_PID_NAME "pids.pid"
#define FILE_TARGET_NAME "target.tgt"
#define FILE_VOT_NAME "voting.vot"
#define FILE_ROUND_NAME "round.pid"

#define FIRST_TARGET 0
#define MAX_TRIES 10000

int worker_actions(int secs, int num_threads, int reader, int writer);

typedef struct datos
{
  double objective;
  double from;
  double to;
}Datos;