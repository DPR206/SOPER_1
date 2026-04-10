/**
 * @file worker.c
 * @brief Este fichero contiene las funcionalidades de los minero
 *
 *
 * @author Duna Puente y Claudia Saiz
 * @date 18/03/2026
*/

#include "worker.h"

/*Variables globales*/
int flag = 0;
int resultado;
int found = 0;
sem_t *mutex_pid = NULL;
sem_t *mutex_target = NULL;
sem_t *mutex_winner = NULL;
sem_t *mutex_round = NULL;
sem_t *mutex_vot = NULL;

/*Funciones privadas*/
int send_message(int writer, int round, int target, double resultado, int validated);
int read_message(int reader);

int entrar(int *target);
int salir();

int write_pid(int fpid);
int read_pids(int fpid, pid_t *pids, int *num_pids, int *pos);
int rewrite_pids(int fpid, pid_t *pids, int num_pids, int pos);

int read_target(int *target);
int write_target(int target);

int first_proc(int fpid, int *target);
int other_proc(int *target);

int fin_de_ronda(int *target, int *validated);
int read_vots(int fvot, int *num_vots);
int write_round();

void handler_SIGUSR1(int sig) {}
void handler_SIGUSR2(int sig) {}
void handler_ALRM(int sig) { flag = 1; printf(" - Se acabó mi tiempo\n");}

/**
 * @brief Tarea del minero para resolver el POW
 * @author Duna Puente y Claudia Saiz
 *
 * @param arg Datos necesarios para ejecutar la funcion
 * @return NULL para todos los casos
 */
void *minero(void *arg) {
  int i;
  Datos *info = arg;
  long int result;

  for (i = info->from; i < info->to && found == 0; i++)
  {
    result = pow_hash(i);
    if (result == info->objective)
    {
      found = 1; /*Se marca que se ha encontrado la solución*/
      resultado = i; /*Se guarda la solución*/
    }
  }
  return NULL;
}

int worker_actions(int secs, int num_threads, int reader, int writer){
  pthread_t *hilos = NULL;
  Datos *datos = NULL;
  int espacio, validated = 0;
  int error;
  int i=0, j, k, t;
  int target;
  sigset_t set, oldset;
  struct sigaction act;

  /*Asignar memoria para los hilos*/
  hilos = (pthread_t *)calloc(num_threads, sizeof(pthread_t));
  if (!hilos) {
    perror("calloc");
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }

  /*Asignar memoria para los datos de cada hilo*/
  datos = (Datos *)calloc(num_threads, sizeof(Datos));
  if (!datos) {
    perror("calloc");
    free(hilos);
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }

  /*Abrir semáforo mutex_pid*/
  if((mutex_pid == NULL) && (mutex_pid = sem_open(MUTEX_PID_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
    perror("sem_open");
    free(hilos);
    free(datos);
    return 0;
  }

  /*Abrir semáforo mutex_target*/
  if((mutex_target == NULL) && (mutex_target = sem_open(MUTEX_TARGET_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
    perror("sem_open");
    sem_close(mutex_pid);
    free(hilos);
    free(datos);
    return 0;
  }

  /*Abrir semáforo mutex_winner*/
  if((mutex_winner == NULL) && (mutex_winner = sem_open(MUTEX_WINNER_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
    perror("sem_open");
    sem_close(mutex_pid);
    sem_close(mutex_target);
    free(hilos);
    free(datos);
    return 0;
  }

  /*Abrir semáforo mutex_winner*/
  if((mutex_round == NULL) && (mutex_round = sem_open(MUTEX_ROUND_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
    perror("sem_open");
    sem_close(mutex_pid);
    sem_close(mutex_target);
    sem_close(mutex_winner);
    free(hilos);
    free(datos);
    return 0;
  }

  /*Abrir semáforo mutex_vot*/
  if((mutex_vot == NULL) && (mutex_vot = sem_open(MUTEX_VOT_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
    perror("sem_open");
    sem_close(mutex_pid);
    sem_close(mutex_target);
    sem_close(mutex_winner);
    sem_close(mutex_vot);
    free(hilos);
    free(datos);
    return 0;
  }

  /*Bloqueamos las señales SIGUSR durante las rondas*/
  signal(SIGUSR1, handler_SIGUSR1);
  signal(SIGUSR2, handler_SIGUSR2);
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGUSR2);
  sigprocmask(SIG_BLOCK, &set, &oldset);


  /*Registrarse como nuevo proceso*/
  if(!entrar(&target)){  
    free(hilos);
    free(datos);
    return 0;
  }

  /*Conteo segundos*/
  act.sa_handler = handler_ALRM;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (sigaction(SIGALRM, &act, NULL) < 0){
    perror("sigaction");
    exit(EXIT_FAILURE);
  }
  alarm(secs);

  /*Empiezan las rondas*/
  while(!flag) {
    /*Resetear la variable global de 'encontrado'*/
    found = 0;

    /*Esperar mensaje de confirmacion de logger*/
    if(i != 0) {
      if(!read_message(reader)){    
        free(hilos);
        free(datos);
        return 0;
      }
    }

    /*Dividir espacio de búsqueda*/
    espacio = POW_LIMIT / num_threads;
    /*Asignar los argumentos de cada hilo*/
    for (j = 0, k = -1; j < num_threads; j++, k++) {
      datos[j].objective = target; 
      
      if (j == 0) {
        datos[j].from = 0;
        datos[j].to = espacio;
      } else if (j == num_threads-1) {
        datos[j].from = datos[k].from + espacio;
        datos[j].to = POW_LIMIT;
      } else {
        datos[j].from = datos[k].from + espacio;
        datos[j].to = datos[k].to + espacio;
      }

      /*Crear hilo*/
      error = pthread_create(&hilos[j], NULL, minero, &datos[j]);

      if (error != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(error));
        for (t = 0; t < j; t++) {
          pthread_join(hilos[t], NULL);
        }
        free(hilos);
        free(datos);
        salir();
        fprintf(stdout, "Miner exited unexpectedly\n");
        return 0;
      }
          
    }

    /*Esperar hilos*/
    for ( j = 0; j < num_threads; j++) {
      error = pthread_join(hilos[j], NULL);
      if (error != 0) {
        free(hilos);
        free(datos);
        salir();
        fprintf(stderr, "pthread_join: %s\n", strerror(error));
        fprintf(stdout, "Miner exited unexpectedly\n");
        return 0;
      } 
    }

    /*Comprobacion*/
    printf("Solution %d: %08d --> %08d %d\n", i + 1, (int)target, (int)resultado, getpid());

    /*Mandar mensaje a logger*/
    if(!send_message(writer, i + 1 , target, resultado, validated)){
      free(hilos);
      free(datos);
      salir();
      return 0;
    }

    if(!fin_de_ronda(&target, &validated)){
      free(hilos);
      free(datos);
      return 0;
    }
    i++;
  }

  /*Se borra del fichero si ha terminado su tiempo*/
  if(flag){
    printf("Intento salir... %d\n", getpid());
    if(!salir()){
      return 0;
    }
    printf("    He salido ok %d\n", getpid());
  }

  /*Mandar señal de fin*/
  if(!send_message(writer, i + 1, target, -1, validated)){
    free(hilos);
    free(datos);
    return 0;
  }

  free(hilos);
  free(datos);
  sem_close(mutex_vot);
  return 1;
}

int send_message(int writer, int round, int target, double resultado, int validated){
  int nbytes = 0;
  char buffer[SIZE];
  nbytes = sprintf(buffer, "%03d|%08d|%08d|%01d", round, target, (int)resultado, validated);
  if(nbytes <= 0){
    perror("sprintf");
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }
  buffer[MESSAGE] = '\0';
  nbytes = write(writer, buffer, MESSAGE);
  if(nbytes <= 0){
    perror("write");
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }
  return 1;
}

int read_message(int reader){
  int nbytes = 0;
  char buffer[SIZE];
  nbytes = read(reader, buffer, CONTINUE);
  if (nbytes == -1) {
    perror("read");
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  } else if (nbytes != CONTINUE){
    fprintf(stdout, "Logger closed comunication unexpectedly\n");
    return 0;
  }
  return 1;
}

int entrar(int *target){
  int fpid;
  pid_t pids[SIZE];
  int num_pids, pos, i;

  sem_wait(mutex_pid);
  
  fpid = open(FILE_PID_NAME, O_CREAT | O_EXCL | O_RDWR , S_IRUSR | S_IWUSR);
  if (fpid != -1) {
    /*Primer proceso*/
    if(!first_proc(fpid, target)){
      fprintf(stdout, "Miner exited unexpectedly\n");
      close(fpid);
      sem_post(mutex_pid);
      salir();
      return 0;
    }
    sem_post(mutex_pid);

    /* Manda señal de SIGUSR1 para empezar la ronda*/
    while(1){
      usleep(100);
      sem_wait(mutex_pid);
      if(!read_pids(fpid, pids, &num_pids, &pos)){
        fprintf(stdout, "Miner exited unexpectedly\n");
        sem_post(mutex_pid);
        salir();
        return 0;
      }
      /*Nos aseguramos que una vez que empezamos la ronda, no se apuntaran más*/
      if(num_pids > 1){
        for(i=0; i<num_pids; i++){
          if(i != pos){
            kill(pids[i], SIGUSR1);
          }
          sem_wait(mutex_round);
          if(!write_round()){
            fprintf(stdout, "Miner exited unexpectedly\n");
            sem_post(mutex_pid);
            salir();
          }
          sem_post(mutex_round);
        }
        sem_post(mutex_pid);
        break;
      }
      fflush(stdout);
      sem_post(mutex_pid);
    }
    close(fpid);

  } else {
    if (errno == EEXIST) {
      /*No primer proceso*/
      if(!other_proc(target)){
        fprintf(stdout, "Miner exited unexpectedly\n");
        sem_close(mutex_pid);
        return 0;
      }    

    } else {
      perror("open");
      return 0;
    }
  }
  return 1;
}

int first_proc(int fpid, int *target){
  int nbytes = 0;
  pid_t pids[SIZE];
  char buffer[SIZE];
  int num_pids, pos, i, ftarget, fvot, fround;

  /*Crear fichero voting y cerrarlo*/
  sem_wait(mutex_vot);
  fvot = open(FILE_VOT_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if(fvot == -1){
    perror("open");
    return 0;
  }
  close(fvot);
  sem_post(mutex_vot);

  /*Crear fichero round y cerrarlo*/
  sem_wait(mutex_vot);
  fround = open(FILE_ROUND_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if(fround == -1){
    perror("open");
    return 0;
  }
  close(fround);
  sem_post(mutex_vot);

  /*Escribir en fichero de pids (sección crítica)*/
  if(!write_pid(fpid)){
    return 0;
  }

  /*Imprimir mensaje de entrada*/
  fprintf(stdout, "Miner %d added to the system (first process)\n", getpid());
  if(!read_pids(fpid, pids, &num_pids, &pos)){
    return 0;
  }
  for(i=0; i<num_pids; i++){
    fprintf(stdout, " - %d\n", pids[i]);
  }

  /*Escribir en el fichero de target (sección crítica)*/
  sem_wait(mutex_target);
  *target = FIRST_TARGET;
  /*Crear fichero target*/
  ftarget = open(FILE_TARGET_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if(ftarget == -1){
    perror("open");
    sem_post(mutex_target);
    return 0;
  }
  nbytes = sprintf(buffer, "%08d", FIRST_TARGET);
  if(nbytes <= 0){
    perror("sprintf");
    sem_post(mutex_target);
    close(ftarget);
    return 0;
  }
  buffer[nbytes] = '\0';
  nbytes = write(ftarget, buffer, nbytes);
  if(nbytes <= 0){
    perror("write");
    sem_post(mutex_target);
    close(ftarget);
    close(fpid);
    return 0;
  }
  close(ftarget);
  sem_post(mutex_target);

  return 1;
}

int other_proc(int *target){
  sigset_t espera_usr1;
  pid_t pids[SIZE];
  int num_pids, pos, i, fpid;

  /*Abrir fichero ya creado*/
  fpid = open(FILE_PID_NAME, O_RDWR);
  if (fpid == -1) {
    perror("open");
    sem_post(mutex_pid);
    return 0;
  }

  /*Escribir en fichero de pids (sección crítica)*/
  if(!write_pid(fpid)){
    sem_post(mutex_pid);
    close(fpid);
    return 0;
  }

  /*Imprimir mensaje de entrada*/
  fprintf(stdout, "Miner %d added to the system\n", getpid());
  if(!read_pids(fpid, pids, &num_pids, &pos)){
    sem_post(mutex_pid);
    close(fpid);
    return 0;
  }
  for(i=0; i<num_pids; i++){
    fprintf(stdout, " - %d\n", pids[i]);
  }
  close(fpid);
  fflush(stdout);
  sem_post(mutex_pid);

  sem_wait(mutex_target);
  if(!read_target(target)){
    sem_post(mutex_target);
    return 0;
  }
  sem_post(mutex_target);

  /*Esperar señal SIGUSR1*/
  sigemptyset(&espera_usr1);
  sigaddset(&espera_usr1, SIGUSR2);
  sigsuspend(&espera_usr1);

  return 1;
}

int salir(){
  int fpid, pos, num_pids;
  pid_t pids_array[SIZE];

  sem_wait(mutex_pid);

  /*Abrir fichero*/
  fpid = open(FILE_PID_NAME, O_RDWR);
  if (fpid == -1) {
    perror("open");
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    return 0;
  }
    
  /*Leer los PIDs del fichero*/
  if(read_pids(fpid, pids_array, &num_pids, &pos) == 0){
    close(fpid);
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    return 0;
  }

  /*Mirar si es el último*/
  if (num_pids - 1 == 0) {
    close(fpid);
    unlink(FILE_PID_NAME);
    unlink(FILE_TARGET_NAME);
    unlink(FILE_VOT_NAME);
    unlink(FILE_ROUND_NAME);

    sem_post(mutex_pid);
    sem_close(mutex_pid);
    sem_unlink(MUTEX_PID_NAME);

    sem_close(mutex_target);
    sem_unlink(MUTEX_TARGET_NAME);

    sem_close(mutex_winner);
    sem_unlink(MUTEX_WINNER_NAME);

    sem_close(mutex_round);
    sem_unlink(MUTEX_ROUND_NAME);

    fprintf(stdout, "Miner %d exited system (last process)\n", getpid());
    return 1;
  }

  /*Reescribir fichero sin mi PID*/
  if(rewrite_pids(fpid, pids_array, num_pids, pos) == 0){
    close(fpid);
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    return 0;
  }

  /*Salir*/
  fprintf(stdout, "Miner %d exited system\n", getpid());
  close(fpid);
  sem_post(mutex_pid);

  sem_close(mutex_pid);
  sem_close(mutex_target);
  sem_close(mutex_winner);
  sem_close(mutex_round);
  return 1;
}

int write_pid(int fpid){
  int nbytes = 0;
  char buffer[SIZE];

  lseek(fpid, 0, SEEK_END);
  nbytes = sprintf(buffer, "%d\n", getpid());
  if(nbytes <= 0){
    perror("sprintf");
    return 0;
  }
  buffer[nbytes] = '\0';

  nbytes = write(fpid, buffer, nbytes);
  if(nbytes <= 0){
    perror("write");
    return 0;
  }
  return 1;
}

int read_pids(int fpid, pid_t *pids, int *num_pids, int *pos) {
  FILE *file = NULL;
  int i = 0;
  pid_t mypid = getpid();

  file = fdopen(dup(fpid), "r");
  if (file == NULL) {
    perror("fdopen");
    return 0;
  }
  fseek(file, 0, SEEK_SET);

  *pos = -1;
  while (fscanf(file, "%d", &pids[i]) == 1) {
    if (pids[i] == mypid) {
      *pos = i;
    }
    i++;
  }
  rewind(file);
  fclose(file);
  if(*pos == -1){
    return 0;
  }
  *num_pids = i;

  return 1;
}

int rewrite_pids(int fpid, pid_t *pids, int num_pids, int pos) {
  FILE *file = NULL;
  int i;

  ftruncate(fpid, 0);
  lseek(fpid, 0, SEEK_SET);
  file = fdopen(dup(fpid), "w");
  if (file == NULL) {
    perror("fdopen");
    return 0;
  }
  for (i = 0; i < num_pids; i++) {
    if(i != pos){
      fprintf(file, "%d\n", pids[i]);
    }
  }

  fflush(file);
  fclose(file);
  return 1;
}

int read_target(int *target){
  FILE *file = NULL;
  int ftarget = open(FILE_TARGET_NAME, O_RDWR);
  if (ftarget == -1) {
    perror("open");
    return 0;
  }

  file = fdopen(dup(ftarget), "r");
  if (file == NULL) {
    perror("fdopen");
    return 0;
  }
  fseek(file, 0, SEEK_SET);

  if(fscanf(file, "%d", target) != 1){
    fclose(file);
    close(ftarget);
    return 0;
  }
  rewind(file);
  fclose(file);
  close(ftarget);

  return 1;
}

int write_target(int target){
  int nbytes;
  char buffer[SIZE];
  int ftarget = open(FILE_TARGET_NAME, O_RDWR);
  if (ftarget == -1) {
    perror("open");
    return 0;
  }

  ftruncate(ftarget, 0);
  nbytes = sprintf(buffer, "%08d", target);
  if(nbytes <= 0){
    perror("sprintf");
    close(ftarget);
    return 0;
  }
  buffer[nbytes] = '\0';
  nbytes = write(ftarget, buffer, nbytes);
  if(nbytes <= 0){
    perror("write");
    close(ftarget);
    return 0;
  }
  close(ftarget);

  return 1;
}

int fin_de_ronda(int *target, int *validated){
  sigset_t espera_usr1, espera_usr2;
  int try = 0, nbytes;
  pid_t pids[SIZE];
  char buffer[SIZE];
  int num_pids, pos, i;
  int num_vots, fvot, fround, fpid;
  char c_validated;

  if(sem_trywait(mutex_winner)==0){
    /*Ganador de la ronda*/
    sem_wait(mutex_target);
    *target = resultado;
    if(!write_target(resultado)){
      perror("write_target");
      sem_post(mutex_target);
      return 0;
    }
    sem_post(mutex_target);

    /*Leer procesos en ronda y mandar que voten*/
    sem_wait(mutex_vot);
    fvot = open(FILE_VOT_NAME, O_RDWR);
    if(fvot == -1){
      perror("open");
      return 0;
    }
    ftruncate(fvot, 0);
    sem_post(mutex_vot);

    fround = open(FILE_ROUND_NAME, O_RDWR);
    if(fround == -1){
      perror("open");
      sem_post(mutex_target);
      sem_post(mutex_winner);
      return 0;
    }

    /*Leer los procesos que han participado en esta ronda*/
    sem_wait(mutex_round);
    if(!read_pids(fround, pids, &num_pids, &pos)){
      fprintf(stdout, "Miner exited unexpectedly\n");
      sem_close(mutex_pid);
      return 0;
    }
    sem_post(mutex_round);
    for(i=0; i<num_pids; i++){
      if(i != pos){
        kill(pids[i], SIGUSR2);
      }
    }

    /*Esperar a que voten todos los procesos participantes*/
    do{
      sem_wait(mutex_vot);
      if(!read_vots(fvot, &num_vots)){
        close(fvot);
        return 0;
      }
      sem_post(mutex_vot);
      usleep(100);
      try++;
    } while (num_vots != num_pids-1 && try < MAX_TRIES);

    /* Manda señal de SIGUSR1 para empezar la ronda*/
    fpid = open(FILE_PID_NAME, O_RDWR);
    if(fpid == -1){
      fprintf(stdout, "Miner exited unexpectedly\n");
      sem_close(mutex_pid);
      return 0;
    }
    while(1){
      sem_wait(mutex_pid);
      if(!read_pids(fpid, pids, &num_pids, &pos)){
        fprintf(stdout, "Miner exited unexpectedly\n");
        sem_close(mutex_pid);
        return 0;
      }
      /*Nos aseguramos que una vez que empezamos la ronda, no se apuntan más*/
      if(num_pids > 1 ){
        if(!write_round()){
          fprintf(stdout, "Miner exited unexpectedly\n");
          sem_close(mutex_pid);
          return 0;
        }
        sem_post(mutex_pid);
        break;
      }
      /*Si somos el último proceso y se acaba nuestro tiempo nos salimos*/
      if(flag){
        sem_post(mutex_pid);
        break;
      }
      sem_post(mutex_pid);
      usleep(100);
    }

    for(i=0; i<num_pids; i++){
      if(i != pos){
        kill(pids[i], SIGUSR1);
      }
    } 
    close(fpid);
    sem_post(mutex_winner);
  } else {
    /*Votante*/

    /*Esperar señal SIGUSR2*/
    sigemptyset(&espera_usr2);
    sigaddset(&espera_usr2, SIGUSR1);
    sigsuspend(&espera_usr2);

    /*Leen el siguiente objetivo*/
    sem_wait(mutex_target);
    if(!read_target(target)){
      return 0;
    }
    if(*target == resultado){
      c_validated = 'Y';
      *validated = 1;
    }else{
      c_validated = 'N';
      *validated = 0;
    }
    sem_post(mutex_target);

    /*Escribir mi voto en fichero de votos*/
    sem_wait(mutex_vot);
    fvot = open(FILE_VOT_NAME, O_RDWR | O_APPEND);
    if(fvot == -1){
      return 0;
    }
    nbytes = sprintf(buffer, "%c", c_validated);
    if(nbytes <= 0){
      perror("sprintf");
      close(fvot);
      return 0;
    }
    buffer[nbytes] = '\0';
    nbytes = write(fvot, buffer, nbytes);
    if(nbytes <= 0){
      perror("write");
      close(fvot);
      return 0;
    }
    close(fvot);
    sem_post(mutex_vot);

    /*Salen si ha terminado su tiempo*/
    if(flag){
      return 1;
    }

    /*Espera señal SIGUSR1 de empezar nueva ronda*/
    sigemptyset(&espera_usr1);
    sigaddset(&espera_usr1, SIGUSR2);
    sigsuspend(&espera_usr1);
  }
  return 1;
}

int read_vots(int fvot, int *num_vots){
  int i = 0;
  char temp;
  FILE *file = NULL;

  file = fdopen(dup(fvot), "r");
  if (file == NULL) {
    perror("fdopen");
    return 0;
  }
  fseek(file, 0, SEEK_SET);

  while (fscanf(file, "%c", &temp) == 1) {
    i++;
  }
  rewind(file);
  fclose(file);
  *num_vots = i;

  return 1;
}

int write_round(){
  FILE *file = NULL;
  int i = 0, nbytes;
  int fround, fpid;
  pid_t pids[SIZE];
  char buffer[SIZE];

  fround = open(FILE_ROUND_NAME, O_RDWR);
  fpid = open(FILE_PID_NAME, O_RDWR);
  if(fround == -1 || fpid == -1){
    perror("open");
    return 0;
  }

  file = fdopen(dup(fpid), "r");
  if (file == NULL) {
    perror("fdopen");
    return 0;
  }
  fseek(file, 0, SEEK_SET);

  /*Limpiar fichero de ronda antes de escribir*/
  ftruncate(fround, 0);
  lseek(fround, 0, SEEK_SET);

  while(fscanf(file, "%d", &pids[i]) == 1){
    nbytes = sprintf(buffer, "%d\n", pids[i]);
    write(fround, buffer, nbytes);
    i++;
  }

  fclose(file);
  close(fround);
  close(fpid);
  return 1;
}