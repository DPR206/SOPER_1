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

int main(int argv, char **argc)
{
  pid_t pid_reg, wpid, ppid;
  pthread_t *hilos = NULL;
  Datos *datos = NULL;
  int pipe_status, status, i, j, k;
  int target, rounds, num_threads, acc_round;
  int error;
  double espacio;
  int log_to_miner[2], miner_to_log[2];
  char buffer[SIZE], filename[SIZE];
  FILE *file = NULL;
  char *toks = NULL;
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
    rounds = atoi(argc[2]);
    num_threads = atoi(argc[3]);
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
    exit(EXIT_FAILURE);
  }

  /*Fork()*/
  pid_reg = fork();


  if (pid_reg < 0)
  {
    perror("Error en el fork");
    exit(EXIT_FAILURE);
  }
  else if (pid_reg == 0)
  {
    /*tarea registrador*/
    close(log_to_miner[0]);
    close(miner_to_log[1]);
    ppid = getppid();

    nbytes = sprintf(filename, "%d.log", (int)ppid);
    if(nbytes <= 0){
      perror("sprintf");
      exit(EXIT_FAILURE);
    }

    file = fopen(filename, "w");
    if (file == NULL) {
      perror("fopen");
      exit(EXIT_FAILURE);
    }

    do{
      nbytes = 0;
      nbytes = read(miner_to_log[0], buffer, MESSAGE);
      buffer[MESSAGE + 1] = '\0';

      toks = strtok(buffer, "|");
      acc_round = atoi(toks);
      toks = strtok(NULL, "|");
      target = atoi(toks);
      toks = strtok(NULL, "|");
      resultado = atof(toks);

      if(resultado != -1){
        fprintf(file, "Id:       %d\n", acc_round);
        fprintf(file, "Winner:   %d\n", (int)ppid);
        fprintf(file, "Target:   %08d\n", (int)target);
        fprintf(file, "Solution: %08d\n", (int)resultado);
        fprintf(file, "Votes:    %d/%d\n", acc_round, acc_round);
        fprintf(file, "Wallets:  %d:%d\n\n", (int)ppid, acc_round);
      }

      nbytes = write(log_to_miner[1], "CONTINUE", CONTINUE);
      if (nbytes == -1){
        perror("write");
        exit(EXIT_FAILURE);
      }
    } while (resultado != -1);

    close(log_to_miner[1]);
    close(miner_to_log[0]);
    fclose(file);
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
        fprintf(stdout, "Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
      }

      /*Asignar memoria para los datos de cada hilo*/
      datos = (Datos *)calloc(num_threads, sizeof(Datos));

      if (!datos)
      {
        free(hilos);
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
          exit(EXIT_FAILURE);
        } else if (nbytes != CONTINUE){
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
          free(hilos);
          free(datos);
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
      
      /*Comprobacion*/
      printf("Solution: %08d --> %08d\n", (int)target, (int)resultado);

      /*Mandar mensaje a logger*/
      nbytes = 0;
      nbytes = sprintf(buffer, "%02d|%08d|%08d", i + 1, target, (int)resultado);
      if(nbytes != MESSAGE){
        perror("sprintf");
        exit(EXIT_FAILURE);
      }

      nbytes = write(miner_to_log[1], buffer, MESSAGE);
      if(nbytes <= 0){
        perror("write");
        exit(EXIT_FAILURE);
      }

      /*Cambiar el objetivo y resetear la variable global de 'encontrado'*/
      target = resultado;
      found = 0;
    }

    /*Mandar señal de fin*/
    nbytes = 0;
    resultado = -1;
    nbytes = sprintf(buffer, "%02d|%08d|%08d", i + 1, target, (int)resultado);
    if(nbytes <= 0){
      perror("sprintf");
      exit(EXIT_FAILURE);
    }
    write(miner_to_log[1], buffer, nbytes + 1);

    wpid = waitpid(pid_reg, &status, 0);
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