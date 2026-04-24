#define _POSIX_C_SOURCE 200809L 
#define _DEFAULT_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <mqueue.h>
#include "types.h"

#define FIRST_TARGET 0