/**
 * @file miner.c
 * @brief Programa de minero que resuelve un POW usando multihilos
 *
 *
 * @author Duna Puente y Claudia Saiz
 * @date 18/02/2026
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#include "miner.h"
#include "logger.h"
#include "worker.h"

/**
 * @brief Ejecuta el programa principal
 * @author Duna Puente y Claudia Saiz
 *
 * @param argv número de argumentos de entrada
 + @param argc argumentos de entrada
 * @return 0 en caso de éxito, 1 en caso contrario
 */
int main(int argv, char **argc) {
  pid_t pid_reg, wpid;
  int pipe_status, status;
  int secs, num_threads;
  int log_status = 1, worker_status = 1;
  int log_to_miner[2], miner_to_log[2];

  /*Borro los semáforos usados por si acaso (no debería ser necesario)*/
  sem_unlink("/mutex1");
  sem_unlink("/mutex2");
  sem_unlink("/mutex3");
  sem_unlink("/mutex4");
  sem_unlink("/mutex5");

  /*Comprobación de argumentos de entrada*/
  if (argv != 3)
  {
    fprintf(stderr, "Error in the input parameters:\n\n");
    fprintf(stderr, "%s <N_SECS> <N_THREADS>\n", argc[0]);
    exit(EXIT_FAILURE);
  }
  else
  {
    /*Asignación de argumentos a variables*/
    secs = atoi(argc[1]);
    if(secs < 0){
      fprintf(stderr, "N_SECS must be positive integer\n");
      exit(EXIT_FAILURE);
    }
    num_threads = atoi(argc[2]);
    if(num_threads < 1){
      fprintf(stderr, "N_THREADS must be greater than 0\n");
      exit(EXIT_FAILURE);
    }
  }

  /*Crear tuberias*/
  pipe_status = pipe(log_to_miner);
  if (pipe_status == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pipe_status = pipe(miner_to_log);
  if (pipe_status == -1) {
    perror("pipe");
    close(log_to_miner[0]);
    close(log_to_miner[1]);
    exit(EXIT_FAILURE);
  }

  /*Fork()*/
  pid_reg = fork();
  if (pid_reg < 0)
  {
    perror("Error en el fork");
    close(log_to_miner[0]);
    close(log_to_miner[1]);
    close(miner_to_log[0]);
    close(miner_to_log[1]);
    exit(EXIT_FAILURE);
  }
  else if (pid_reg == 0)
  {
    
    /*tarea registrador*/
    close(log_to_miner[0]);
    close(miner_to_log[1]);

    log_status = logger_actions(miner_to_log[0], log_to_miner[1]);
    if(log_status == 0){
      close(log_to_miner[1]);
      close(miner_to_log[0]);
      exit(EXIT_FAILURE);
    }
    close(log_to_miner[1]);
    close(miner_to_log[0]);
  }
  else
  {
    /*tarea minero*/
    close(miner_to_log[0]);
    close(log_to_miner[1]);

    worker_status = worker_actions(secs, num_threads, log_to_miner[0], miner_to_log[1]);
    if(worker_status == 0){
      close(miner_to_log[1]);
      close(log_to_miner[0]);
      exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS); /*BORRAR DESPUES*/

    /*Esperar al proceso hijo*/
    wpid = waitpid(pid_reg, &status, 0);
    if(wpid == -1){
      perror("waitpid");
      exit(EXIT_FAILURE);
    }
    if (WIFEXITED(status))
    {
      fprintf(stdout, "Logger exited with status %d\n", WEXITSTATUS(status));
    }
    else
    {
      fprintf(stdout, "Logger exited unexpectedly\n");
    }

    close(miner_to_log[1]);
    close(log_to_miner[0]);

    /*Mensaje de salida*/
    fprintf(stdout, "Miner exited with status %d\n", EXIT_SUCCESS);
    exit(EXIT_SUCCESS);
  }

  exit(EXIT_SUCCESS);
}