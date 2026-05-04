/**
 * @file monitor.c
 * @brief Programa de monitorización del sistema
 *
 *
 * @author Duna Puente y Claudia Saiz
 * @date 13/04/2026
 */

#include "monitor.h"

int monitor_actions();
int monitor_inicializar();
int monitor_salir();

int comprobador_actions();
int comprobador_inicializar();
int comprobador_salir();

int semaforos_prod_cons();
int limpiar_semaforos();

/**
 * @brief Ejecuta el programa principal
 * @author Duna Puente y Claudia Saiz
 *
 * @param argv número de argumentos de entrada
 + @param argc argumentos de entrada
 * @return ERROR en caso de éxito, 1 en caso contrario
 */
int main(int argv, char **argc) {
	int lag_comprobador, lag_monitor;
	int pid_reg, status;
	sem_PC *sems = NULL;

	/*Comprobación de argumentos de entrada*/
	if (argv != 3) {
		fprintf(stderr, "Error in the input parameters:\n\n");
		fprintf(stderr, "%s <LAG_COMPROBADOR> <LAG_MONITOR>\n", argc[0]);
		exit(EXIT_FAILURE);
	} else {
		/*Asignación de argumentos a variables*/
		lag_comprobador = atoi(argc[1]);
		if (lag_comprobador < 1) {
			fprintf(stderr, "LAG_COMPROBADOR must be positive integer\n");
			exit(EXIT_FAILURE);
		}
		lag_monitor = atoi(argc[2]);
		if (lag_monitor < 1) {
			fprintf(stderr, "LAG_MONITOR must be positive integer\n");
			exit(EXIT_FAILURE);
		}
	}

	/*Mapera semáforos sin nombre productor-consumidor*/
	sems = mmap(NULL, MEM_SEMS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if(sems == MAP_FAILED){
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	/*Crear semaforos sin nombre productor-consumidor */
	if(!semaforos_prod_cons(&sems)){
		perror("Error with sems of productor-consumidor");
		exit(EXIT_FAILURE);
	}

	/*Fork()*/
	pid_reg = fork();
	if (pid_reg < 0) {
		perror("fork");
		exit(EXIT_FAILURE);

	} else if (pid_reg == 0) {

		/*Monitor*/
		if (!monitor_actions(lag_monitor, sems)) {
			limpiar_semaforos(sems);
			fprintf(stdout, "Monitor exited unexpectedly\n");
			exit(EXIT_FAILURE);
		}

	} else {

		/*Comprobador*/
		if (!comprobador_actions(lag_comprobador, sems)) {
			limpiar_semaforos(sems);
			fprintf(stdout, "Comprobador exited unexpectedly\n");
			exit(EXIT_FAILURE);
		}

		/*Esperar al proceso hijo*/
		if (waitpid(pid_reg, &status, 0) == -1) {
			limpiar_semaforos(sems);
			perror("waitpid");
			exit(EXIT_FAILURE);
		}
		if (WIFEXITED(status)) {
			fprintf(stdout, "Monitor exited with status %d\n", WEXITSTATUS(status));
		} else {
			fprintf(stdout, "Monitor exited unexpectedly\n");
		}

		/*Limpiar recursos productor-consumidor*/
		limpiar_semaforos(sems);

		/*Mensaje de salida*/
		fprintf(stdout, "Comprobador exited with status %d\n", EXIT_SUCCESS);
	}

	exit(EXIT_SUCCESS);
}

int semaforos_prod_cons(sem_PC **sems){
		if (sem_init(&(*sems)->sem_empty, 1, TAM_BUFFER) != 0) {
			perror("Error vacios");
			return ERROR;
		}
		
    if (sem_init(&(*sems)->sem_fill, 1, 0) != 0) {       
			perror("Error llenos");
			return ERROR;
		}
		
    if (sem_init(&(*sems)->sem_mutex, 1, 1) != 0) {          
			perror("Error mutex");
			return ERROR;
		}

    (*sems)->prod_idx = 0;
    (*sems)->cons_idx = 0;

		return OK;
}

int limpiar_semaforos(sem_PC *b) {
    sem_destroy(&b->sem_empty);
    sem_destroy(&b->sem_fill);
    sem_destroy(&b->sem_mutex);
    munmap(b, MEM_SEMS_SIZE);

		return OK;
}

int monitor_actions(int lag_monitor, sem_PC *sems) {

	/*Abrir memoria compartida*/
	if (!monitor_inicializar()) {
		monitor_salir();
		/*Comunicar al resto que he acabado?? Como??*/
		/*Mandar señal??*/
		perror("Error starting monitor");
		return ERROR;
	}

	/*Comunicar mediante memoria con comprobador*/
	while (1)
	{
		validacion_data validacion_rec;

		/*Semáforos*/
		sem_wait(&sems->sem_fill);
		sem_wait(&sems->sem_mutex);

		validacion_rec = sems->buffer[sems->cons_idx];
		sems->cons_idx = (sems->cons_idx + 1) % TAM_BUFFER;
		
		/*Comprobar que no es un bloque especial*/
		if(validacion_rec.target == -1){
			fprintf(stdout, "Last miner exited, finishing monitor\n");
			break;
		}

		/*Comprobar la bandera del resultado */
		if(validacion_rec.validacion == TRUE){
			fprintf(stdout, "Solution %s: %08d --> %08d\n", "accepted", validacion_rec.target, validacion_rec.resultado);
		} else {
			fprintf(stdout, "Solution %s: %08d !-> %08d\n", "rejected", validacion_rec.target, validacion_rec.resultado);
		}
		fflush(stdout);

		/*Semáforos*/
		sem_post(&sems->sem_mutex);
		sem_post(&sems->sem_empty);

		usleep(lag_monitor);
	}

	/*Salir: borrar memoria compartida y semáforos*/
	printf("Finishing monitor\n");
	if (!monitor_salir()) {
		return ERROR;
	}

	return 1;
}

int comprobador_actions(int lag_comprobador, sem_PC *sems) {

	mqd_t mq;

	if(!comprobador_inicializar())
	{
		comprobador_salir();
		perror("Error starting comprobador");
		return ERROR;
	}

	while ((mq = mq_open(MQ_NAME, O_RDWR)) == (mqd_t)-1) {
		if (errno != ENOENT) {
			perror("mq_open");
			return ERROR;
		}
		usleep(100);
	}

	while (1) {
		target_data target_recv;
		validacion_data validacion_env;
		ssize_t nbytes = mq_receive(mq, (char *)&target_recv, MAX_MESSAGE, NULL);
		if (nbytes == -1) {
			perror("mq_receive");
			return ERROR;
		} else if (nbytes == 0) {
			fprintf(stdout, "Monitor closed comunication unexpectedly\n");
			return ERROR;
		}
		if (target_recv.target == -1) {

			/*Semáforos*/
			sem_wait(&sems->sem_empty);
			sem_wait(&sems->sem_mutex);

			/*Introducir bloque especial en memoria compartida*/
			validacion_env.target = target_recv.target;

			sems->buffer[sems->prod_idx] = validacion_env;
			sems->prod_idx = (sems->prod_idx+1) % TAM_BUFFER;

			/*Semáforos*/
			sem_post(&sems->sem_mutex);
			sem_post(&sems->sem_fill);

			fprintf(stdout, "Last miner exited, finishing comprobador\n");
			break;
		}

		/*Semáforos*/
		sem_wait(&sems->sem_empty);
		sem_wait(&sems->sem_mutex);

		/*Insertar en memoria compartida*/
		if(target_recv.votes_yes >= target_recv.votes_no){
			validacion_env.validacion = TRUE;
		} else {
			validacion_env.validacion = FALSE;
		}
		
		validacion_env.target = target_recv.target;
		validacion_env.resultado = target_recv.resultado;

		sems->buffer[sems->prod_idx] = validacion_env;
		sems->prod_idx = (sems->prod_idx+1) % TAM_BUFFER;

		/*Semáforos*/
		sem_post(&sems->sem_mutex);
		sem_post(&sems->sem_fill);

		usleep(lag_comprobador);
	}
	mq_close(mq);

	printf("Finishing comprobador\n");
	if (!comprobador_salir()) {
		return ERROR;
	}

	return 1;
}

int monitor_inicializar() {
	int fvalidate;
	validacion_data *validate_mem = NULL;

	/*Abrir memoria compartida con comprobador*/
	while ((fvalidate = shm_open(MEM_VALIDATE_NAME, O_RDONLY, MEM_VALIDACION_SIZE)) == -1 && errno == ENOENT)
	{
		usleep(100);
	}

	if(fvalidate == -1){
		perror("shm_open mem_validate");
		return ERROR;
	}

	validate_mem = mmap(NULL, MEM_VALIDACION_SIZE, PROT_READ, MAP_SHARED, fvalidate, 0);

	if (validate_mem == MAP_FAILED) {
    perror("mmap monitor");
    close(fvalidate);
    return ERROR;
  }

	close(fvalidate);

	return OK;
}

int monitor_salir() {

	/*Cerrar memoria compartida*/
	shm_unlink(MEM_VALIDATE_NAME);

	return OK;
}

int comprobador_inicializar(){

	int fpid, ftarget, fvot, fround, fvalidate;
	pids_data *pid_mem = NULL, *round_mem = NULL;
	vots_data *vot_mem = NULL;
	target_data *target_mem = NULL;
	validacion_data *validate_mem = NULL;
	struct mq_attr attributes;
	mqd_t mq;
	sem_t *mutex_pid = NULL;
	sem_t *mutex_target = NULL;
	sem_t *mutex_winner = NULL;
	sem_t *mutex_round = NULL;
	sem_t *mutex_vot = NULL;


	/*Abrir memoria compartida*/

	fpid = shm_open(MEM_PID_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	if (fpid == -1) {
		perror("shm_open mem_pid");
		return ERROR;
	}
	if (ftruncate(fpid, MEM_PID_SIZE) == -1) {
		perror("ftruncate mem_pid");
		close(fpid);
		return ERROR;
	}
	pid_mem = mmap(NULL, MEM_PID_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fpid, 0);
	if (pid_mem == MAP_FAILED) {
		perror("mmap pid");
		return ERROR;
	}
	pid_mem->num_pids = 0;
	close(fpid);

	ftarget = shm_open(MEM_TARGET_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	if (ftarget == -1) {
		perror("shm_open mem_target");
		close(fpid);
		return ERROR;
	}
	if (ftruncate(ftarget, MEM_TARGET_SIZE) == -1) {
		perror("ftruncate mem_target");
		close(fpid);
		close(ftarget);
		return ERROR;
	}
	target_mem = mmap(NULL, MEM_TARGET_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ftarget, 0);
	if (target_mem == MAP_FAILED) {
		perror("mmap target");
		return ERROR;
	}
	target_mem->target = FIRST_TARGET;
	close(ftarget);

	fvot = shm_open(MEM_VOT_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	if (fvot == -1) {
		perror("shm_open mem_vot");
		close(fpid);
		close(ftarget);
		return ERROR;
	}
	if (ftruncate(fvot, MEM_VOT_SIZE) == -1) {
		perror("ftruncate mem_vot");
		close(fpid);
		close(ftarget);
		close(fvot);
		return ERROR;
	}
	vot_mem = mmap(NULL, MEM_VOT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fvot, 0);
	if (vot_mem == MAP_FAILED) {
		perror("mmap vot");
		return ERROR;
	}
	vot_mem->num_vots = 0;
	vot_mem->num_yes = 0;
	vot_mem->num_no = 0;
	close(fvot);

	fround = shm_open(MEM_ROUND_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	if (fround == -1) {
		perror("shm_open mem_round");
		close(fpid);
		close(ftarget);
		close(fvot);
		return ERROR;
	}
	if (ftruncate(fround, MEM_ROUND_SIZE) == -1) {
		perror("ftruncate mem_round");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		return ERROR;
	}
	round_mem = mmap(NULL, MEM_ROUND_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fround, 0);
	if (round_mem == MAP_FAILED) {
		perror("mmap round");
		return ERROR;
	}
	round_mem->num_pids = 0;
	close(fround);

	fvalidate = shm_open(MEM_VALIDATE_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	if(fvalidate == -1){
		perror("shm_open mem_validate");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		return ERROR;
	}
	if(ftruncate(fvalidate, MEM_VALIDACION_SIZE) == -1){
		perror("ftruncate mem_validate");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		return ERROR;
	}
	validate_mem = mmap(NULL, MEM_VALIDACION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fvalidate, 0);
	validate_mem->validacion = FALSE;
	validate_mem->target = FIRST_TARGET;
	close(fvalidate);

	/*Abrir cola de mensajes*/

	attributes.mq_maxmsg = MAX_NUM_MSG; /*Capacidad máxima de la cola*/
	attributes.mq_msgsize = MAX_MESSAGE;
	if ((mq = mq_open(MQ_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR, &attributes)) == (mqd_t)-1) {
		perror(" mq_open ");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		return ERROR;
	}
	mq_close(mq);

	/*Abrir semáforos*/

	mutex_pid = sem_open(MUTEX_PID_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_pid == SEM_FAILED) {
		perror("sem_open mutex_pid");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		return ERROR;
	}
	sem_close(mutex_pid);

	mutex_target = sem_open(MUTEX_TARGET_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_target == SEM_FAILED) {
		perror("sem_open mutex_target");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		sem_close(mutex_pid);
		return ERROR;
	}
	sem_close(mutex_target);

	mutex_winner = sem_open(MUTEX_WINNER_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_winner == SEM_FAILED) {
		perror("sem_open mutex_winner");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		sem_close(mutex_pid);
		sem_close(mutex_target);
		return ERROR;
	}
	sem_close(mutex_winner);

	mutex_round = sem_open(MUTEX_ROUND_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_round == SEM_FAILED) {
		perror("sem_open mutex_round");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		sem_close(mutex_pid);
		sem_close(mutex_target);
		sem_close(mutex_winner);
		return ERROR;
	}
	sem_close(mutex_round);

	mutex_vot = sem_open(MUTEX_VOT_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_vot == SEM_FAILED) {
		perror("sem_open mutex_vot");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		sem_close(mutex_pid);
		sem_close(mutex_target);
		sem_close(mutex_winner);
		sem_close(mutex_round);
		return ERROR;
	}
	sem_close(mutex_vot);

	return OK;

}

int comprobador_salir() {
	/*Cerrar memoria compartida*/
	shm_unlink(MEM_PID_NAME);
	shm_unlink(MEM_TARGET_NAME);
	shm_unlink(MEM_VOT_NAME);
	shm_unlink(MEM_ROUND_NAME);
	shm_unlink(MEM_VALIDATE_NAME);

	/*Cerrar cola de mensajes*/
	mq_unlink(MQ_NAME);

	/*Cerrar semáforos*/
	sem_unlink(MUTEX_PID_NAME);
	sem_unlink(MUTEX_TARGET_NAME);
	sem_unlink(MUTEX_WINNER_NAME);
	sem_unlink(MUTEX_ROUND_NAME);
	sem_unlink(MUTEX_VOT_NAME);

	return OK;
}