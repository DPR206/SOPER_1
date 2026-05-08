/**
 * @file monitor.c
 * @brief Programa de monitorización del sistema
 *
 *
 * @author Duna Puente y Claudia Saiz
 * @date 13/04/2026
 */

#include "monitor.h"

/*Funciones privadas*/
int monitor_actions(int lag_monitor, sem_PC *sems);
int monitor_inicializar();
int monitor_salir();

int comprobador_actions(int lag_comprobador, sem_PC *sems);
int comprobador_inicializar();
int comprobador_salir();

int semaforos_prod_cons(sem_PC *sems);
int limpiar_semaforos(sem_PC *b);

/*Variable global*/
pids_data *pid_mem_global = NULL;

void handler_comprobador(int sig) {
	int i;

	if (pid_mem_global != NULL) {
		printf("\n[Monitor] Parando el sistema...\n");
		pid_mem_global->monitor = 1;

		for (i = 0; i < pid_mem_global->num_pids; i++) {
			if (pid_mem_global->pids[i] > 1) {
				kill(pid_mem_global->pids[i], SIGTERM);
			}
		}
	}

	printf("Monitor exiting...\n");
	sleep(1);
	comprobador_salir();
	exit(EXIT_FAILURE);
}

void handler_monitor(int sig) {
	monitor_salir();
	exit(EXIT_FAILURE);
}

/**
 * @brief Ejecuta el programa principal
 * @author Duna Puente y Claudia Saiz
 *
 * @param argv número de argumentos de entrada
 + @param argc argumentos de entrada
 * @return EXIT_SUCESS en caso de éxito, EXIT_FAILURE en caso contrario
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

	/*Mapear semáforos sin nombre productor-consumidor*/
	sems = mmap(NULL, MEM_SEMS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (sems == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	/*Crear semaforos sin nombre productor-consumidor */
	if (!semaforos_prod_cons(sems)) {
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

/**
 * @brief Inicilizar y crear los recursos de productor-consumidor
 * @author Claudia Saiz y Duna Puente
 *
 * @param sems estructura con los semáforos e índices
 * @return OK en caso de éxito, ERROR en caso de error
 */
int semaforos_prod_cons(sem_PC *sems) {

	/*Abrir semáforos sin nombre*/
	if (sem_init(&sems->sem_empty, 1, TAM_BUFFER) != 0) {
		perror("Error vacios");
		return ERROR;
	}

	if (sem_init(&sems->sem_fill, 1, 0) != 0) {
		perror("Error llenos");
		return ERROR;
	}

	if (sem_init(&sems->sem_mutex, 1, 1) != 0) {
		perror("Error mutex");
		return ERROR;
	}

	/*Inicializar índices*/
	sems->prod_idx = 0;
	sems->cons_idx = 0;

	return OK;
}

/**
 * @brief Limpia los recursos de productor-consumidor
 * @author Claudia Saiz y Duna Puente
 *
 * @param b estructura con los recursos e índices
 * @return OK en caso de éxito, ERROR en caso de error
 */
int limpiar_semaforos(sem_PC *b) {

	sem_destroy(&b->sem_empty);
	sem_destroy(&b->sem_fill);
	sem_destroy(&b->sem_mutex);
	munmap(b, MEM_SEMS_SIZE);

	return OK;
}

/**
 * @brief Hace la acción del monitor
 * @author Claudia Saiz y Duna Puente
 *
 * @param lag_monitor tiempo de espera del monitor
 * @param sems estructura con los semáforo e índices del productor-consumidor
 * @return OK en caso de éxito, ERROR en caso de error
 */
int monitor_actions(int lag_monitor, sem_PC *sems) {
	struct sigaction sa;

	sa.sa_handler = handler_monitor;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/*Abrir memoria compartida*/
	if (!monitor_inicializar()) {
		monitor_salir();
		perror("Error starting monitor");
		return ERROR;
	}

	/*Comunicar mediante memoria con comprobador*/
	while (1) {
		validacion_data validacion_rec;

		/*El consumidor espera a que llegue información y luego la extrae*/
		sem_wait(&sems->sem_fill);
		sem_wait(&sems->sem_mutex);

		validacion_rec = sems->buffer[sems->cons_idx];
		sems->cons_idx = (sems->cons_idx + 1) % TAM_BUFFER;

		/*Comprobar que no es un bloque especial*/
		if (validacion_rec.target == -1) {
			fprintf(stdout, "Last miner exited, finishing monitor\n");
			break;
		}

		/*Comprobar la bandera del resultado */
		if (validacion_rec.validacion == TRUE) {
			fprintf(stdout, "Solution %s: %08d --> %08d\n", "accepted", validacion_rec.target, validacion_rec.resultado);
		} else {
			fprintf(stdout, "Solution %s: %08d !-> %08d\n", "rejected", validacion_rec.target, validacion_rec.resultado);
		}
		fflush(stdout);

		/*Semáforos*/
		sem_post(&sems->sem_mutex);
		sem_post(&sems->sem_empty);

		/*Realiza espera*/
		usleep(lag_monitor);
	}

	/*Salir: borrar memoria compartida y semáforos*/
	printf("Finishing monitor\n");
	if (!monitor_salir()) {
		return ERROR;
	}

	return OK;
}

/**
 * @brief Hace la acción del comprobador
 * @author Claudia Saiz y Duna Puente
 *
 * @param lag_comprobador tiempo de espera del comprobador
 * @param sems estructura con los semáforo e índices del productor-consumidor
 * @return OK en caso de éxito, ERROR en caso de error
 */
int comprobador_actions(int lag_comprobador, sem_PC *sems) {
	struct sigaction sa;
	mqd_t mq;

	/*Inicializar recursos: memoria compartida, mensajes y semáforos*/
	if (!comprobador_inicializar()) {
		comprobador_salir();
		perror("Error starting comprobador");
		return ERROR;
	}

	sa.sa_handler = handler_comprobador;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/*Abre la cola de mensajes*/
	while ((mq = mq_open(MQ_NAME, O_RDWR)) == (mqd_t)-1) {
		if (errno != ENOENT) {
			comprobador_salir();
			perror("mq_open");
			return ERROR;
		}
		usleep(100);
	}

	while (1) {
		target_data target_recv;
		validacion_data validacion_env;

		/*Recibe el bloque de mensaje*/
		ssize_t nbytes = mq_receive(mq, (char *)&target_recv, MAX_MESSAGE, NULL);
		if (nbytes == -1) {
			perror("mq_receive");
			return ERROR;
		} else if (nbytes == 0) {
			fprintf(stdout, "Monitor closed comunication unexpectedly\n");
			return ERROR;
		}

		/*Comprueba si es un bloque especial (fin del sistema)*/
		if (target_recv.target == -1) {

			/*Espera a tener hueco para mandar el bloque y luego accede*/
			sem_wait(&sems->sem_empty);
			sem_wait(&sems->sem_mutex);

			/*Introducir bloque especial en memoria compartida*/
			validacion_env.target = target_recv.target;

			sems->buffer[sems->prod_idx] = validacion_env;
			sems->prod_idx = (sems->prod_idx + 1) % TAM_BUFFER;

			/*Semáforos*/
			sem_post(&sems->sem_mutex);
			sem_post(&sems->sem_fill);

			fprintf(stdout, "Last miner exited, finishing comprobador\n");
			break;
		}

		/*Espera a tener hueco para mandar el bloque y luego accede*/
		sem_wait(&sems->sem_empty);
		sem_wait(&sems->sem_mutex);

		/*Comprobar y traspasar resultados*/
		if (target_recv.votes_yes >= target_recv.votes_no) {
			validacion_env.validacion = TRUE;
		} else {
			validacion_env.validacion = FALSE;
		}

		validacion_env.target = target_recv.target;
		validacion_env.resultado = target_recv.resultado;

		/*Insertar en memoria compartida*/
		sems->buffer[sems->prod_idx] = validacion_env;
		sems->prod_idx = (sems->prod_idx + 1) % TAM_BUFFER;

		/*Semáforos*/
		sem_post(&sems->sem_mutex);
		sem_post(&sems->sem_fill);

		/*Realiza la espera*/
		usleep(lag_comprobador);
	}
	mq_close(mq);

	/*Ejecuta la salida del programa: libera los recursos pertinentes*/
	printf("Finishing comprobador\n");
	if (!comprobador_salir()) {
		return ERROR;
	}

	return OK;
}

/**
 * @brief Inicializa los recursos asociados al monitor
 * @author Claudia Saiz y Duna Puente
 *
 * @return OK en caso de éxito, ERROR en caso de error
 */
int monitor_inicializar() {
	int fvalidate;
	validacion_data *validate_mem = NULL;

	/*Abrir memoria compartida con comprobador*/
	while ((fvalidate = shm_open(MEM_VALIDATE_NAME, O_RDONLY, MEM_VALIDACION_SIZE)) == -1 && errno == ENOENT) {
		usleep(100);
	}

	if (fvalidate == -1) {
		perror("shm_open mem_validate");
		return ERROR;
	}

	/*Mapea la memoria compartida*/
	validate_mem = mmap(NULL, MEM_VALIDACION_SIZE, PROT_READ, MAP_SHARED, fvalidate, 0);

	if (validate_mem == MAP_FAILED) {
		perror("mmap monitor");
		close(fvalidate);
		return ERROR;
	}

	close(fvalidate);

	return OK;
}

/**
 * @brief Libera los recursos pertinentes al monitor
 * @author Claudia Saiz y Duna Puente
 *
 * @return OK en caso de éxito, ERROR en caso de error
 */
int monitor_salir() {

	/*Cerrar memoria compartida*/
	shm_unlink(MEM_VALIDATE_NAME);

	return OK;
}

/**
 * @brief Inicializa los recursos pertinentes al comprobador
 * @author Claudia Saiz y Duna Puente
 *
 * @return OK en caso de éxito, ERROR en caso de error
 */
int comprobador_inicializar() {

	int fpid, ftarget, fvot, fround, fvalidate, fcartera;
	pids_data *pid_mem = NULL, *round_mem = NULL;
	vots_data *vot_mem = NULL;
	target_data *target_mem = NULL;
	validacion_data *validate_mem = NULL;
	cartera_data *cartera_mem = NULL;
	struct mq_attr attributes;
	mqd_t mq;
	sem_t *mutex_pid = NULL;
	sem_t *mutex_target = NULL;
	sem_t *mutex_winner = NULL;
	sem_t *mutex_round = NULL;
	sem_t *mutex_vot = NULL;
	sem_t *mutex_cart = NULL;

	/*Abrir memoria compartida*/

	/*Memoria de los pids*/
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
	pid_mem->monitor = 0;
	pid_mem_global = pid_mem;
	close(fpid);

	/*Memoria de target*/
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

	/*Memoria de las votaciones*/
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

	/*Memoria de las rondas*/
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

	/*Memoria de la validación*/
	fvalidate = shm_open(MEM_VALIDATE_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	if (fvalidate == -1) {
		perror("shm_open mem_validate");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		return ERROR;
	}
	if (ftruncate(fvalidate, MEM_VALIDACION_SIZE) == -1) {
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

	/*Memoria de la cartera*/
	fcartera = shm_open(MEM_CARTERA_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	if (fcartera == -1) {
		perror("shm_open mem_cartera");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		return ERROR;
	}
	if (ftruncate(fcartera, MEM_CARTERA_SIZE) == -1) {
		perror("ftruncate mem_validate");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		close(fcartera);
		return ERROR;
	}
	cartera_mem = mmap(NULL, MEM_CARTERA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fcartera, 0);
	if (cartera_mem == MAP_FAILED) {
		perror("mmap mem_cartera");
		close(fcartera);
		return ERROR;
	}
	memset(cartera_mem, 0, MEM_CARTERA_SIZE);
	close(fcartera);

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
		close(fcartera);
		return ERROR;
	}
	mq_close(mq);

	/*Abrir semáforos*/

	/*Semáforo de los pids*/
	mutex_pid = sem_open(MUTEX_PID_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_pid == SEM_FAILED) {
		perror("sem_open mutex_pid");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		close(fcartera);
		return ERROR;
	}
	sem_close(mutex_pid);

	/*Semáforo de target*/
	mutex_target = sem_open(MUTEX_TARGET_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_target == SEM_FAILED) {
		perror("sem_open mutex_target");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		close(fcartera);
		sem_close(mutex_pid);
		return ERROR;
	}
	sem_close(mutex_target);

	/*Semáforo de winner*/
	mutex_winner = sem_open(MUTEX_WINNER_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_winner == SEM_FAILED) {
		perror("sem_open mutex_winner");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		close(fcartera);
		sem_close(mutex_pid);
		sem_close(mutex_target);
		return ERROR;
	}
	sem_close(mutex_winner);

	/*Semáforo de rondas*/
	mutex_round = sem_open(MUTEX_ROUND_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_round == SEM_FAILED) {
		perror("sem_open mutex_round");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		close(fcartera);
		sem_close(mutex_pid);
		sem_close(mutex_target);
		sem_close(mutex_winner);
		return ERROR;
	}
	sem_close(mutex_round);

	/*Semáforo de votaciones*/
	mutex_vot = sem_open(MUTEX_VOT_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_vot == SEM_FAILED) {
		perror("sem_open mutex_vot");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		close(fcartera);
		sem_close(mutex_pid);
		sem_close(mutex_target);
		sem_close(mutex_winner);
		sem_close(mutex_round);
		return ERROR;
	}
	sem_close(mutex_vot);

	/*Semáforo de cartera*/
	mutex_cart = sem_open(MUTEX_CART_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
	if (mutex_cart == SEM_FAILED) {
		perror("sem_open mutex_cart");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fvalidate);
		close(fcartera);
		sem_close(mutex_pid);
		sem_close(mutex_target);
		sem_close(mutex_winner);
		sem_close(mutex_round);
		sem_close(mutex_vot);
		return ERROR;
	}
	sem_close(mutex_cart);

	return OK;
}

/**
 * @brief Libera los recursos pertinentes al comprobador
 * @author Claudia Saiz y Duna Puente
 *
 * @return OK en caso de éxito, ERROR en caso de error
 */
int comprobador_salir() {
	/*Cerrar memoria compartida*/
	shm_unlink(MEM_PID_NAME);
	shm_unlink(MEM_TARGET_NAME);
	shm_unlink(MEM_VOT_NAME);
	shm_unlink(MEM_ROUND_NAME);
	shm_unlink(MEM_VALIDATE_NAME);
	shm_unlink(MEM_CARTERA_NAME);

	/*Cerrar cola de mensajes*/
	mq_unlink(MQ_NAME);

	/*Cerrar semáforos*/
	sem_unlink(MUTEX_PID_NAME);
	sem_unlink(MUTEX_TARGET_NAME);
	sem_unlink(MUTEX_WINNER_NAME);
	sem_unlink(MUTEX_ROUND_NAME);
	sem_unlink(MUTEX_VOT_NAME);
	sem_unlink(MUTEX_CART_NAME);

	return OK;
}