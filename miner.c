#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#include "pow.h"
#include "miner.h"

/*Variable global*/
int found = 0;
double resultado;

void *minero(void *arg)
{
  Datos *info = arg;
  int i;
  long int result;

  for (i = info->from; i < info->from && found == 0; i++)
  {
    result = pow_hash(i);
    if (result == info->objective)
    {
      found = 1;
      resultado = i;
      free(info);
      /*pthread_exit((void *)result);*/
      /*return (void *)result;*/
      return NULL;
    }
  }

  /*pthread_exit(NULL)*/
  return NULL;
}

int main(int argv, char **argc)
{
  pid_t pid_reg, wpid;
  pthread_t *hilos = NULL;
  Datos *datos = NULL;
  int status, i, j;
  int target, rounds, num_threads;
  int error;
  double espacio;
  /*double *retorno = NULL;*/

  if (argv != 4)
  {
    fprintf(stderr, "Error in the input parameters:\n\n");
    fprintf(stderr, "%s < TARGET_INI > < ROUNDS > < N_THREADS >\n", argc[0]);
    exit(EXIT_FAILURE);
  }
  else
  {
    target = atoi(argc[1]);
    rounds = atoi(argc[2]);
    num_threads = atoi(argc[3]);
  }

  pid_reg = fork();
  if (pid_reg < 0)
  {
    perror("Error en el fork");
    exit(EXIT_FAILURE);
  }
  else if (pid_reg == 0)
  {
    /*tarea registrador*/

    fprintf(stdout, "Prueba logger\n");
    fprintf(stdout, "Logger exited with status %d\n", EXIT_SUCCESS);
  }
  else
  {
    /*tarea minero*/
    /*Conteo rondas*/
    for (i = 0; i < rounds; i++)
    {
      /*Dividir espacio de búsqueda*/
      espacio = POW_LIMIT / num_threads;

      /*Asignar memoria para los hilos*/
      hilos = (pthread_t *)calloc(num_threads, sizeof(pthread_t));

      if (!hilos)
      {
        fprintf(stdout, "Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
      }

      /*Asignar memoria para los datos de cada hilo*/
      datos = (Datos *)calloc(num_threads, sizeof(Datos));

      if (!datos)
      {
        fprintf(stdout, "Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
      }

      /*Realizar las rondas*/
      for (j = 0; j < num_threads; j++)
      {
        datos[j].objective = target;
        /*found = 0;*/
        if (j == 0)
        {
          datos[j].from = 0;
          datos[j].to = espacio;
        }
        else
        {
          datos[j].from += espacio;
          datos[j].to += espacio;
        }

        /*Crear hilo*/
        error = pthread_create(&hilos[j], NULL, minero, &datos[j]);

        if (error != 0)
        {
          fprintf(stderr, "pthread_create: %s\n", strerror(error));
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
          fprintf(stderr, "pthread_join: %s\n", strerror(error));
          fprintf(stdout, "Miner exited unexpectedly\n");
          exit(EXIT_FAILURE);
        } 
      }
      
      /*cambiar el objetivo*/
      target = resultado;
      found = 0;

      /*Comprobacion*/
      printf("Solucion: %f\n", resultado);
    }

    wpid = waitpid(pid_reg, &status, 0);

    if (WIFEXITED(status))
    {
      fprintf(stdout, "Logger exited with status %d\n", WEXITSTATUS(status));
    }
    else
    {
      fprintf(stdout, "Logger exited unexpectedly\n");
    }

    /*fprintf(stdout, "Prueba miner\n");*/
    fprintf(stdout, "Miner exited with status %d\n", EXIT_SUCCESS);
    exit(EXIT_SUCCESS);
  }

  exit(EXIT_SUCCESS);
}