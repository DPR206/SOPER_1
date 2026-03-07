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

int logger_actions(int reader, int writer){
  pid_t ppid;
  int target, acc_round, solution, validated = 0;
  char buffer[SIZE], filename[SIZE], str_validated[SIZE];
  FILE *file = NULL;
  char *toks = NULL;
  int nbytes = 0;

  /*Crear fichero log*/
  ppid = getppid();
  nbytes = sprintf(filename, "%d.log", (int)ppid);
  if(nbytes <= 0){
    perror("sprintf");
    return 0;
  }

  /*Abrir fichero log*/
  file = fopen(filename, "w");
  if (file == NULL) {
    perror("fopen");
    return 0;
  }

  do{
    /*Leer mensaje*/
    nbytes = 0;
    nbytes = read(reader, buffer, MESSAGE);
    if (nbytes == -1) {
      perror("read");
      fclose(file);
      return 0;
    } else if (nbytes != MESSAGE){
      fprintf(stdout, "Miner closed comunication unexpectedly\n");
      fclose(file);
      return 0;
    }
    buffer[MESSAGE] = '\0';

    /*Descifrar mensaje*/
    toks = strtok(buffer, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return 0;
    }
    acc_round = atoi(toks);

    toks = strtok(NULL, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return 0;
    }
    target = atoi(toks);

    toks = strtok(NULL, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return 0;
    }
    solution = atof(toks);

    toks = strtok(NULL, "|");
    if (toks == NULL){
      perror("strtok");
      fclose(file);
      return 0;
    }
    validated = atof(toks);

    if(validated){
      strcpy(str_validated, "validated");
    } else {
      strcpy(str_validated, "rejected");
    }

    /*Escribir en fichero*/
    if(solution != -1){
      fprintf(file, "Id:       %d\n", acc_round);
      fprintf(file, "Winner:   %d\n", (int)ppid);
      fprintf(file, "Target:   %08d\n", (int)target);
      fprintf(file, "Solution: %08d (%s)\n", (int)solution, str_validated);
      fprintf(file, "Votes:    %d/%d\n", acc_round, acc_round);
      fprintf(file, "Wallets:  %d:%d\n\n", (int)ppid, acc_round);
    }

    /*Mandar señal a minero*/
    nbytes = write(writer, "CONTINUE", CONTINUE);
    if (nbytes == -1){
      perror("write");
      fprintf(stdout, "Logger exited unexpectedly\n");
      fclose(file);
      return 0;
    }
  } while (solution != -1);

  if (fclose(file) != 0) {
    perror("fclose");
    return 0;
  }

  return 1;
}