/**
 * @file logger.c
 * @brief Este fichero contiene las funcionalidades del logger
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

#include "logger.h"

/**
 * @brief Hace la acción de un proceso minero
 * @author Duna Puente y Claudia Saiz
 *
 * @param reader Descriptor de fichero de la tubería de lectura con worker
 * @param writer Descriptor de fichero de la tubería de escritura con worker
 * @return OK si ejecuta correctamente, ERROR en caso contrario
 */
int logger_actions(int reader, int writer){
  pid_t ppid;
  int target, acc_round, solution, validated = 0, votes, num_procs;
  char buffer[SIZE], filename[SIZE], str_validated[SIZE];
  FILE *file = NULL;
  char *toks = NULL;
  int nbytes = 0;
  int monedas = 0;

  /*Crear fichero log*/
  ppid = getppid();
  nbytes = sprintf(filename, "%d.log", (int)ppid);
  if(nbytes <= 0){
    perror("sprintf");
    return ERROR;
  }

  /*Abrir fichero log*/
  file = fopen(filename, "w");
  if (file == NULL) {
    perror("fopen");
    return ERROR;
  }

  do{
    /*Leer mensaje*/
    nbytes = 0;
    nbytes = read(reader, buffer, MESSAGE);
    if (nbytes == -1) {
      perror("read");
      fclose(file);
      return ERROR;
    } else if (nbytes != MESSAGE){
      fprintf(stdout, "Miner closed comunication unexpectedly\n");
      fclose(file);
      return ERROR;
    }
    buffer[MESSAGE] = '\0';

    /*Descifrar mensaje*/
    toks = strtok(buffer, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return ERROR;
    }
    acc_round = atoi(toks);

    toks = strtok(NULL, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return ERROR;
    }
    target = atoi(toks);

    toks = strtok(NULL, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return ERROR;
    }
    solution = atof(toks);

    toks = strtok(NULL, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return ERROR;
    }
    validated = atof(toks);

    toks = strtok(NULL, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return ERROR;
    }
    votes = atoi(toks);

    toks = strtok(NULL, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return ERROR;
    }
    num_procs = atoi(toks);

    if(votes >= num_procs){
      monedas++;
      strcpy(str_validated, "validated");
    } else {
      strcpy(str_validated, "rejected");
    }

    /*Escribir en fichero*/
    if(solution != -1 && validated == 1){
      fprintf(file, "Id:       %d\n", acc_round);
      fprintf(file, "Winner:   %d\n", (int)ppid);
      fprintf(file, "Target:   %08d\n", (int)target);
      fprintf(file, "Solution: %08d (%s)\n", (int)solution, str_validated);
      fprintf(file, "Votes:    %d/%d\n", votes, num_procs);
      fprintf(file, "Wallets:  %d:%d\n\n", monedas, acc_round);
    }

    /*Mandar señal a minero*/
    nbytes = write(writer, "CONTINUE", CONTINUE);
    if (nbytes == -1){
      perror("write");
      fprintf(stdout, "Logger exited unexpectedly\n");
      fclose(file);
      return ERROR;
    }
  } while (solution != -1);

  if (fclose(file) != 0) {
    perror("fclose");
    return ERROR;
  }

  return OK;
}