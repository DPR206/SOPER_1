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

#include "pow.h"
#include "miner.h"
#include "logger.h"

/*Variable global*/
int found = 0;
double resultado;

/**
 * @brief Tarea del minero para resolver el POW
 * @author Duna Puente y Claudia Saiz
 *
 * @param arg Datos necesarios para ejecutar la funcion
 * @return NULL para todos los casos
 */
void *minero(void *arg)
{
  Datos *info = arg;
  int i;
  long int result;

  for (i = info->from; i < info->to && found == 0; i++)
  {
    result = pow_hash(i);
    if (result == info->objective)
    {
      found = 1; /*Se marca que se ha encontrado la solución*/
      resultado = i; /*Se guarda la solución*/
      /*pthread_exit((void *)result);*/
      /*return (void *)result;*/
      return NULL;
    } 
  }

  /*pthread_exit(NULL)*/
  return NULL;
}

/**
 * @brief Ejecuta el programa principal
 * @author Duna Puente y Claudia Saiz
 *
 * @param argv número de argumentos de entrada
 + @param argc argumentos de entrada
 * @return 0 en caso de éxito, 1 en caso contrario
 */
int main(int argv, char **argc)
{
  pid_t pid_reg, wpid;
  pthread_t *hilos = NULL;
  Datos *datos = NULL;
  int pipe_status, status, i, j, k, t;
  int target, rounds, num_threads;
  int error, log_status = 0;
  double espacio;
  int log_to_miner[2], miner_to_log[2], validated = 0;
  char buffer[SIZE], str_validated[SIZE];
  int nbytes = 0;

  /*Comprobación de argumentos de entrada*/
  if (argv != 4)
  {
    fprintf(stderr, "Error in the input parameters:\n\n");
    fprintf(stderr, "%s < TARGET_INI > < ROUNDS > < N_THREADS >\n", argc[0]);
    exit(EXIT_FAILURE);
  }
  else
  {
    /*Asignación de argumentos a variables*/
    target = atoi(argc[1]);
    if(target < 0){
      fprintf(stderr, "TARGET must be positive integer\n");
      exit(EXIT_FAILURE);
    }
    rounds = atoi(argc[2]);
    if(rounds < 1){
      fprintf(stderr, "ROUNDS must be greater than 0\n");
      exit(EXIT_FAILURE);
    }
    num_threads = atoi(argc[3]);
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
    } else if (log_status == 1){
      close(log_to_miner[1]);
      close(miner_to_log[0]);
    }
  }
  else
  {
    /*tarea minero*/
    close(miner_to_log[0]);
    close(log_to_miner[1]);

    /*Asignar memoria para los hilos*/
      hilos = (pthread_t *)calloc(num_threads, sizeof(pthread_t));

      if (!hilos)
      {
        close(miner_to_log[1]);
        close(log_to_miner[0]);
        fprintf(stdout, "Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
      }

      /*Asignar memoria para los datos de cada hilo*/
      datos = (Datos *)calloc(num_threads, sizeof(Datos));

      if (!datos)
      {
        free(hilos);
        close(miner_to_log[1]);
        close(log_to_miner[0]);
        fprintf(stdout, "Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
      }

    /*Conteo rondas*/
    for (i = 0; i < rounds; i++)
    {
      /*Esperar mensaje de confirmacion de logger*/
      if(i != 0){
        nbytes = 0;
        nbytes = read(log_to_miner[0], buffer, CONTINUE);
        if (nbytes == -1) {
          perror("read");
          close(miner_to_log[1]);
          close(log_to_miner[0]);
          free(hilos);
          free(datos);
          fprintf(stdout, "Miner exited unexpectedly\n");
          exit(EXIT_FAILURE);
        } else if (nbytes != CONTINUE){
          close(miner_to_log[1]);
          close(log_to_miner[0]);
          free(hilos);
          free(datos);
          fprintf(stdout, "Logger closed comunication unexpectedly\n");
          exit(EXIT_FAILURE);
        }
      }

      /*Dividir espacio de búsqueda*/
      espacio = POW_LIMIT / num_threads;

      /*Crear los hilos*/
      for (j = 0, k=-1; j < num_threads; j++, k++)
      {
        /*Asignar los argumentos de cada hilo*/
        datos[j].objective = target; 
    
        if (j == 0)
        {
          datos[j].from = 0;
          datos[j].to = espacio;
        }else if (j == num_threads-1) /*Asegurarse de que se llega al final del espacio de búsqueda*/
        {
          datos[j].from = datos[k].from + espacio;
          datos[j].to = POW_LIMIT;
        } else
        {
          datos[j].from = datos[k].from + espacio;
          datos[j].to = datos[k].to + espacio;
        }

        /*Crear hilo*/
        error = pthread_create(&hilos[j], NULL, minero, &datos[j]);

        if (error != 0)
        {
          fprintf(stderr, "pthread_create: %s\n", strerror(error));
          for (t = 0; t < j; t++) {
            pthread_join(hilos[t], NULL);
          }
          free(hilos);
          free(datos);
          close(miner_to_log[1]);
          close(log_to_miner[0]);
          fprintf(stdout, "Miner exited unexpectedly\n");
          exit(EXIT_FAILURE);
        }
        
      }

      /*Esperar hilos*/
      for ( j = 0; j < num_threads; j++)
      {
        error = pthread_join(hilos[j], NULL);
        if (error != 0)
        {
          close(miner_to_log[1]);
          close(log_to_miner[0]);
          free(hilos);
          free(datos);
          fprintf(stderr, "pthread_join: %s\n", strerror(error));
          fprintf(stdout, "Miner exited unexpectedly\n");
          exit(EXIT_FAILURE);
        } 
      }
      
      /*Validar solucion*/
      if(resultado == 9331340){
        validated = 0; /*rejected*/
        strcpy(str_validated, "rejected");
      } else {
        validated = 1; /*validated*/
        strcpy(str_validated, "accepted");
      }

      /*Comprobacion*/
      printf("Solution %s: %08d --> %08d\n", str_validated, (int)target, (int)resultado);

      /*Mandar mensaje a logger*/
      nbytes = 0;
      nbytes = sprintf(buffer, "%02d|%08d|%08d|%01d", i + 1, target, (int)resultado, validated);
      if(nbytes != MESSAGE){
        perror("sprintf");
        close(miner_to_log[1]);
        close(log_to_miner[0]);
        free(hilos);
        free(datos);
        fprintf(stdout, "Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
      }
      buffer[MESSAGE] = '\0';

      nbytes = write(miner_to_log[1], buffer, MESSAGE);
      if(nbytes <= 0){
        perror("write");
        close(miner_to_log[1]);
        close(log_to_miner[0]);
        free(hilos);
        free(datos);
        fprintf(stdout, "Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
      }

      /*Cambiar el objetivo y resetear la variable global de 'encontrado'*/
      target = resultado;
      found = 0;
    }

    /*Mandar señal de fin*/
    nbytes = 0;
    resultado = -1;
    nbytes = sprintf(buffer, "%02d|%08d|%08d|%01d", i + 1, target, (int)resultado, validated);
    if(nbytes <= 0){
      perror("sprintf");
      close(miner_to_log[1]);
      close(log_to_miner[0]);
      free(hilos);
      free(datos);
      fprintf(stdout, "Miner exited unexpectedly\n");
      exit(EXIT_FAILURE);
    }
    buffer[MESSAGE] = '\0';

    nbytes = write(miner_to_log[1], buffer, MESSAGE);
    if(nbytes <= 0){
      perror("write");
      close(miner_to_log[1]);
      close(log_to_miner[0]);
      free(hilos);
      free(datos);
      fprintf(stdout, "Miner exited unexpectedly\n");
      exit(EXIT_FAILURE);
    }

    /*Esperar al proceso hijo*/
    wpid = waitpid(pid_reg, &status, 0);
    if(wpid == -1){
      perror("waitpid");
      close(miner_to_log[1]);
      close(log_to_miner[0]);
      free(hilos);
      free(datos);
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

    /*Liberacion memoria*/
    free(hilos);
    free(datos);
    close(miner_to_log[1]);
    close(log_to_miner[0]);

    /*Mensaje de salida*/
    fprintf(stdout, "Miner exited with status %d\n", EXIT_SUCCESS);
    exit(EXIT_SUCCESS);
  }

  exit(EXIT_SUCCESS);
}