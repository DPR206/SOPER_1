#ifndef TYPES_H
#define TYPES_H

#define MUTEX_PID_NAME "/mutex1"
#define MUTEX_TARGET_NAME "/mutex2"
#define MUTEX_WINNER_NAME "/mutex3"
#define MUTEX_ROUND_NAME "/mutex4"
#define MUTEX_VOT_NAME "/mutex5"

#define MEM_PID_NAME "/pids"
#define MEM_TARGET_NAME "/target"
#define MEM_VOT_NAME "/voting"
#define MEM_ROUND_NAME "/round"
#define MEM_VALIDATE_NAME "/validation"

#define MAX_PROCESOS 100

typedef struct {
	int num_pids;
	pid_t pids[MAX_PROCESOS];
} pids_data;

typedef struct {
  int num_vots;
  int num_yes;
  int num_no;
} vots_data;

typedef struct {
  int target;
  int resultado;
  pid_t winner;
  int votes_yes;
  int votes_no;
} target_data;

typedef struct {
  pid_t propietario;
  int monedas;
}cartera_data;

typedef struct {
  int target;
  int resultado;
  int validacion;
} validacion_data;


#define MEM_PID_SIZE sizeof(pids_data)
#define MEM_TARGET_SIZE sizeof(target_data)
#define MEM_VOT_SIZE sizeof(vots_data)
#define MEM_ROUND_SIZE sizeof(pids_data)
#define MEM_CARTERA_SIZE sizeof(cartera_data)
#define MEM_VALIDACION_SIZE sizeof(validacion_data)

#define MQ_NAME "/message_queue"
#define MAX_MESSAGE sizeof(target_data)+1

#define SIZE 256
#define CONTINUE 8
#define MESSAGE 30

#define OK 1
#define ERROR 0
#define EARLY 2

#define TRUE 1
#define FALSE 0

#endif