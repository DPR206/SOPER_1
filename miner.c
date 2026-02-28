#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argv, char **argc)
{
  pid_t pid_reg, wpid;
  int status;

  if (argv != 4)
  {
    fprintf(stderr, "Error in the input parameters:\n\n");
    fprintf(stderr, "%s < TARGET_INI > < ROUNDS > < N_THREADS >\n", argc[0]);
    exit(EXIT_FAILURE);
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

    wpid = waitpid(pid_reg, &status, 0);
    
    if (WIFEXITED(status))
    {
      fprintf(stdout, "Logger exited with status %d\n", WEXITSTATUS(status));
    } else {
      fprintf(stdout, "Logger exited unexpectedly\n");
    }
    

    fprintf(stdout, "Prueba miner\n");
    fprintf(stdout, "Miner exited with status %d\n", EXIT_SUCCESS);
    exit(EXIT_SUCCESS);
  }

  exit(EXIT_SUCCESS);
}