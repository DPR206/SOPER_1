/**
 * @file worker.c
 * @brief Este fichero contiene las funcionalidades de los minero
 *
 *
 * @author Duna Puente y Claudia Saiz
 * @date 18/03/2026
*/

#include "worker.h"
#define MUTEX_PID_NAME "/mutex1"
#define MUTEX_TARGET_NAME "/mutex2"

/*Variable global*/
int found = 0;
int flag = 0;
double resultado;
sem_t *mutex_pid = NULL;
sem_t *mutex_target = NULL;

int send_message(int writer, int round, int target, double resultado, int validated);
int read_message(int reader);
int entrar();
int salir();
int read_pids(int fpid, pid_t *pids, int *num_pids, int *pos);
int write_pids(int fpid, pid_t *pids, int num_pids, int pos);
int first_proc(int fpid);
int other_proc(int fpid);

void handler_SIGUSR1(int sig) { printf("He recibido SIGUSR1\n");}

void handler_ALRM(int sig) { flag = 1; printf("Se acabó mi tiempo\n");}

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

int worker_actions(int secs, int num_threads, int reader, int writer){
  pthread_t *hilos = NULL;
  Datos *datos = NULL;
  int espacio, validated = 0;
  int error, status = 0;
  int i, j, k, t;
  char str_validated[SIZE];
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
    free(hilos);
    perror("calloc");
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }

  /*Registrarse como nuevo proceso*/
  status = entrar();
  if(!status){  
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

  while(!flag) {

    /*Esperar mensaje de confirmacion de logger*/
    /*if(i != 0) {
      status = read_message(reader);
      if(!status){    
        free(hilos);
        free(datos);
        return 0;
      }
    }*/

    /*Dividir espacio de búsqueda*/
    espacio = POW_LIMIT / num_threads;
    /*Asignar los argumentos de cada hilo*/
    /*for (j = 0, k = -1; j < num_threads; j++, k++) {
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
      }*/

      /*Crear hilo*/
      /*error = pthread_create(&hilos[j], NULL, minero, &datos[j]);

      if (error != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(error));
        for (t = 0; t < j; t++) {
          pthread_join(hilos[t], NULL);
        }
        free(hilos);
        free(datos);
        fprintf(stdout, "Miner exited unexpectedly\n");
        return 0;
      }
          
    }*/

    /*Esperar hilos*/
    /*for ( j = 0; j < num_threads; j++) {
      error = pthread_join(hilos[j], NULL);
      if (error != 0) {
        free(hilos);
        free(datos);
        fprintf(stderr, "pthread_join: %s\n", strerror(error));
        fprintf(stdout, "Miner exited unexpectedly\n");
        return 0;
      } 
    }*/
        
    /*Validar solucion*/
    /*if(resultado == 9331340){
      validated = 0; /*rejected*/
      /*strcpy(str_validated, "rejected");
    } else {
      validated = 1; /*validated*/
      /*strcpy(str_validated, "accepted");
    }
  }*/

    /*Comprobacion*/
    /*printf("Solution %s: %08d --> %08d\n", str_validated, (int)target, (int)resultado);

    /*Mandar mensaje a logger*/
    /*status = send_message(writer, i + 1 ,23, resultado, validated);
    if(!status){
      free(hilos);
      free(datos);
      return 0;
    }*/

    /*Cambiar el objetivo y resetear la variable global de 'encontrado'*/
      /*target = resultado;
      found = 0;
  }

  /*Mandar señal de fin*/
  /*status = send_message(writer, i + 1 ,23, -1, validated);
  if(!status){
    free(hilos);
    free(datos);
    return 0;*/
  }

  status = salir();
  if(!status){ 
    free(hilos);
    free(datos);
    return 0;
  }

  free(hilos);
  free(datos);
  return 1;
}

int send_message(int writer, int round, int target, double resultado, int validated){
  int nbytes = 0;
  char buffer[SIZE];
  nbytes = sprintf(buffer, "%02d|%08d|%08d|%01d", round, target, (int)resultado, validated);
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

int entrar(){
  int fpid;
  pid_t pids[SIZE];
  int num_pids, pos, i;

  if((mutex_pid == NULL) && (mutex_pid = sem_open(MUTEX_PID_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
    perror("sem_open");
    return 0;
  }
  sem_wait(mutex_pid);

  fpid = open("pids.pid", O_CREAT | O_EXCL | O_RDWR , S_IRUSR | S_IWUSR);
  if (fpid != -1) {
    /*Primer proceso*/
    if(!first_proc(fpid)){
      fprintf(stdout, "Miner exited unexpectedly\n");
      return 0;
    }

    /* Manda señal de SIGUSR1 para empezar la ronda*/
    do {
      sem_wait(mutex_pid);
      if(!read_pids(fpid, pids, &num_pids, &pos)){
        fprintf(stdout, "Miner exited unexpectedly\n");
        return 0;
      }
      sem_post(mutex_pid);
    } while(num_pids < 2);
    
    for(i=0; i<num_pids; i++){
      if(i != pos){
        kill(pids[i], SIGUSR1);
      }
    }

  } else {
    if (errno == EEXIST) {
      /*No primer proceso*/
      if(!other_proc(fpid)){
        fprintf(stdout, "Miner exited unexpectedly\n");
        return 0;
      }    

    } else {
      perror("open");
      return 0;
    }
  }
  return 1;
}

int first_proc(int fpid){
  int nbytes = 0, val;
  pid_t pids[SIZE];
  char buffer[SIZE];
  int num_pids, pos, i, ftarget;

  /*Escribir en fichero de pids (sección crítica)*/
  nbytes = sprintf(buffer, "%d\n", getpid());
  if(nbytes <= 0){
    perror("sprintf");
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    return 0;
  }
  buffer[nbytes] = '\0';
  nbytes = write(fpid, buffer, nbytes);
  if(nbytes <= 0){
    perror("write");
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    return 0;
  }

  /*Imprimir mensaje de entrada*/
  fprintf(stdout, "Miner %d added to the system (first process)\n", getpid());
  if(!read_pids(fpid, pids, &num_pids, &pos)){
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }
  for(i=0; i<num_pids; i++){
    fprintf(stdout, " - %d\n", pids[i]);
  }
  sem_post(mutex_pid);

  /*Abrir semáforo mutex_target*/
  if((mutex_target == NULL) && (mutex_target = sem_open(MUTEX_TARGET_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
    perror("sem_open");
    return 0;
  }

  /*Escribir en el fichero de target (sección crítica)*/
  sem_wait(mutex_target);
  ftarget = open("target.tgt", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  nbytes = sprintf(buffer, "%08d", 0);
  if(nbytes <= 0){
    perror("sprintf");
    sem_post(mutex_target);
    sem_close(mutex_pid);
    sem_close(mutex_target);
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }
  buffer[nbytes] = '\0';
  nbytes = write(ftarget, buffer, nbytes);
  if(nbytes <= 0){
    perror("write");
    sem_post(mutex_target);
    sem_close(mutex_pid);
    sem_close(mutex_target);
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }
  close(ftarget);
  sem_post(mutex_target);
  /*sem_close(mutex_target);*/
  return 1;
}

int other_proc(int fpid){
  sigset_t mask, oldmask;
  pid_t pids[SIZE];
  int num_pids, pos, i;
  int nbytes = 0;
  char buffer[SIZE];

  /*Abrir fichero ya creado*/
  fpid = open("pids.pid", O_RDWR);
  if (fpid == -1) {
    perror("open");
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    return 0;
  }

  /*Escribir en fichero de pids (sección crítica)*/
  lseek(fpid, 0, SEEK_END);
  nbytes = sprintf(buffer, "%d\n", getpid());
  if(nbytes <= 0){
    perror("sprintf");
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }
  buffer[nbytes] = '\0';
  nbytes = write(fpid, buffer, nbytes);
  if(nbytes <= 0){
    perror("write");
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }

  /*Imprimir mensaje de entrada*/
  fprintf(stdout, "Miner %d added to the system\n", getpid());
  if(!read_pids(fpid, pids, &num_pids, &pos)){
    printf("falla aqui\n");
    sem_post(mutex_pid);
    sem_close(mutex_pid);
    fprintf(stdout, "Miner exited unexpectedly\n");
    return 0;
  }
  for(i=0; i<num_pids; i++){
    fprintf(stdout, " - %d\n", pids[i]);
  }
  sem_post(mutex_pid);

  /*Esperar señal SIGUSR1*/
  signal(SIGUSR1, handler_SIGUSR1);
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);
  sigsuspend(&oldmask);
  sigprocmask(SIG_SETMASK, &oldmask, NULL);

  return 1;
}

int salir(){
  sem_t *mutex;
  int fpid, pos, num_pids;
  pid_t pids_array[SIZE];

  mutex = sem_open("/mutex1", 0);
  if (mutex == SEM_FAILED) {
    perror("sem_open");
    return 0;
  }
  sem_wait(mutex);

  /*Abrir fichero*/
  fpid = open("pids.pid", O_RDWR);
  if (fpid == -1) {
    perror("open");
    sem_post(mutex);
    sem_close(mutex);
    return 0;
  }
    
  /*Leer los PIDs del fichero*/
  if(read_pids(fpid, pids_array, &num_pids, &pos) == 0){
    close(fpid);
    sem_post(mutex);
    sem_close(mutex);
    return 0;
  }

  /*Mirar si es el último*/
  if (num_pids - 1 == 0) {
    close(fpid);
    unlink("pids.pid");
    sem_post(mutex);
    sem_close(mutex);
    sem_unlink(MUTEX_PID_NAME);
    fprintf(stdout, "Miner %d exited system (last process)\n", getpid());
    return 1;
  }

  /*Reescribir fichero sin mi PID*/
  if(write_pids(fpid, pids_array, num_pids, pos) == 0){
    close(fpid);
    sem_post(mutex);
    sem_close(mutex);
    return 0;
  }

  /*Salir*/
  fprintf(stdout, "Miner %d exited system\n", getpid());
  close(fpid);
  sem_post(mutex);
  sem_close(mutex);
  return 1;
}

int read_pids(int fpid, pid_t *pids, int *num_pids, int *pos) {
  FILE *file = NULL;
  int i = 0;
  pid_t mypid = getpid();

  file = fdopen(fpid, "r");
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
  if(*pos == -1){
    return 0;
  }
  *num_pids = i;

  return 1;
}

int write_pids(int fpid, pid_t *pids, int num_pids, int pos) {
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