/**
 * @file worker.c
 * @brief Este fichero contiene las funcionalidades de los mineros
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

/*Funciones privadas*/
int send_message(int writer, int round, target_data *target_send);
int read_message(int reader);

int abrir_recursos(pids_data **pid_mem, target_data **target_mem, vots_data **vot_mem, pids_data **round_mem, cartera_data **cartera_mem, mqd_t *mq, sem_t **mutex_pid, sem_t **mutex_target, sem_t **mutex_winner, sem_t **mutex_round, sem_t **mutex_vot, sem_t **mutex_cartera);
int entrar(pids_data *pid_mem, sem_t *mutex_pid, sem_t *mutex_target, sem_t *mutex_round, sem_t *mutex_cart, pids_data *round_mem, target_data *target_mem, cartera_data *cartera_mem);
int salir(pids_data *pid_mem, target_data *target_mem, pids_data *round_mem, vots_data *vot_mem, cartera_data *cartera_mem, mqd_t mq, sem_t *mutex_pid, sem_t *mutex_target, sem_t *mutex_winner, sem_t *mutex_round, sem_t *mutex_vot, sem_t *mutex_cart);
int fin_de_ronda(sem_t *mutex_pid, pids_data *pid_mem, sem_t *mutex_round, pids_data *round_mem, sem_t *mutex_vot, vots_data *vots_mem, sem_t *mutex_target, target_data *target_mem, sem_t *mutex_winner, mqd_t mq, target_data *target_send, sem_t *mutex_cart, cartera_data *cartera_mem);

void handler_SIGUSR1(int sig) {}
void handler_SIGUSR2(int sig) {}
void handler_ALRM(int sig) {
	flag = 1;
	printf(" - Se acabó mi tiempo\n");
}
void handler_SIGTERM(int sig){
	printf(" - El monitor se detuvo\n");
}

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

	for (i = info->from; i < info->to && found == 0; i++) {
		result = pow_hash(i);
		if (result == info->objective) {
			found = 1;		 /*Se marca que se ha encontrado la solución*/
			resultado = i; /*Se guarda la solución*/
		}
	}
	return NULL;
}

/**
 * @brief Hace la acción de un proceso minero
 * @author Duna Puente y Claudia Saiz
 *
 * @param secs Número de segundos que tiene el minero
 * @param num_threads Número de hilos que lanza el minero
 * @param reader Descriptor de fichero de la tubería de lectura con logger
 * @param writer Descriptor de fichero de la tubería de escritura con logger
 * @return OK si ejecuta correctamente, ERROR en caso contrario
 */
int worker_actions(int secs, int num_threads, int reader, int writer) {
	pthread_t *hilos = NULL;
	Datos *datos = NULL;
	int espacio, error, target;
	int i = 0, j, k, t;

	target_data target_send;
	sigset_t set, oldset;
	struct sigaction act, act2;

	pids_data *pid_mem = NULL;
	target_data *target_mem = NULL;
	vots_data *vot_mem = NULL;
	pids_data *round_mem = NULL;
	cartera_data *cartera_mem = NULL;
	mqd_t mq;
	sem_t *mutex_pid = NULL;
	sem_t *mutex_target = NULL;
	sem_t *mutex_winner = NULL;
	sem_t *mutex_round = NULL;
	sem_t *mutex_vot = NULL;
	sem_t *mutex_cartera = NULL;

	/*Asignar memoria para los hilos*/
	hilos = (pthread_t *)calloc(num_threads, sizeof(pthread_t));
	if (!hilos) {
		perror("calloc");
		fprintf(stdout, "Miner exited unexpectedly\n");
		return ERROR;
	}

	/*Asignar memoria para los datos de cada hilo*/
	datos = (Datos *)calloc(num_threads, sizeof(Datos));
	if (!datos) {
		perror("calloc");
		free(hilos);
		fprintf(stdout, "Miner exited unexpectedly\n");
		return ERROR;
	}

	error = abrir_recursos(&pid_mem, &target_mem, &vot_mem, &round_mem, &cartera_mem, 
												 &mq, &mutex_pid, &mutex_target, &mutex_winner, &mutex_round, &mutex_vot, &mutex_cartera);
	if (error == ERROR) {
		fprintf(stdout, "Miner exited unexpectedly\n");
		free(hilos);
		free(datos);
		return ERROR;
	} else if (error == EARLY) {
		fprintf(stdout, "Error: Miner entered before the monitor\n");
		free(hilos);
		free(datos);
		return ERROR;
	}

	/*Bloqueamos las señales SIGUSR durante las rondas*/
	signal(SIGUSR1, handler_SIGUSR1);
	signal(SIGUSR2, handler_SIGUSR2);
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGUSR2);
	sigprocmask(SIG_BLOCK, &set, &oldset);

	/*Señal terminación monitor*/
	act2.sa_handler = handler_SIGTERM;
	sigemptyset(&act2.sa_mask);
	act2.sa_flags = 0;
	if (sigaction(SIGTERM, &act2, NULL) < 0) {
		perror("sigaction");
		salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera);
		return ERROR;
	}

	/*Registrarse como nuevo proceso*/
	if (!entrar(pid_mem, mutex_pid, mutex_target, mutex_round, mutex_cartera, round_mem, target_mem, cartera_mem)) {
		fprintf(stdout, "Miner exited unexpectedly\n");
		free(hilos);
		free(datos);
		salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera);
		return ERROR;
	}

	/*Conteo segundos*/
	act.sa_handler = handler_ALRM;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGALRM, &act, NULL) < 0) {
		perror("sigaction");
		salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera);
		return ERROR;
	}
	alarm(secs);

	/*Empiezan las rondas*/
	while (!flag /*&& !pid_mem->monitor*/) {
		/*Resetear la variable global de 'found'*/
		found = 0;

		/*Leer el objetivo de la ronda*/
		sem_wait(mutex_target);
		target = target_mem->target;
		sem_post(mutex_target);

		/*Esperar mensaje de confirmacion de logger*/
		if (i != 0) {
			if (!read_message(reader)) {
				fprintf(stdout, "Miner exited unexpectedly\n");
				free(hilos);
				free(datos);
				salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera);
				return ERROR;
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
			} else if (j == num_threads - 1) {
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
				salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera);
				fprintf(stdout, "Miner exited unexpectedly\n");
				return ERROR;
			}
		}

		/*Esperar hilos*/
		for (j = 0; j < num_threads; j++) {
			error = pthread_join(hilos[j], NULL);
			if (error != 0) {
				free(hilos);
				free(datos);
				salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera);
				fprintf(stderr, "pthread_join: %s\n", strerror(error));
				fprintf(stdout, "Miner exited unexpectedly\n");
				return ERROR;
			}
		}

		if (!fin_de_ronda(mutex_pid, pid_mem, mutex_round, round_mem, mutex_vot, vot_mem, mutex_target, target_mem, mutex_winner, mq, &target_send, mutex_cartera, cartera_mem)) {
			fprintf(stdout, "Miner exited unexpectedly\n");
			free(hilos);
			free(datos);
			salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera);
			return ERROR;
		}

		/*Mandar mensaje a logger*/
		if (!send_message(writer, i + 1, &target_send)) {
			fprintf(stdout, "Miner exited unexpectedly\n");
			free(hilos);
			free(datos);
			salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera);
			return ERROR;
		}
		i++;
	}

	/*Se borra del fichero si ha terminado su tiempo*/
	if (flag) {
		if (!salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera)) {
			fprintf(stdout, "Miner exited unexpectedly\n");
			return ERROR;
		}
	}

	/*Se borra del fichero si el ejecutable del monitor ha detenido su ejecución*/
	/*if (pid_mem->monitor) {
		if (!salir(pid_mem, target_mem, round_mem, vot_mem, cartera_mem, mq, mutex_pid, mutex_target, mutex_winner, mutex_round, mutex_vot, mutex_cartera)) {
			fprintf(stdout, "Miner exited unexpectedly\n");
			return ERROR;
		}
	}*/

	/*Mandar señal de fin*/
	target_send.resultado = -1;
	if (!send_message(writer, i + 1, &target_send)) {
		fprintf(stdout, "Miner exited unexpectedly\n");
		free(hilos);
		free(datos);
		return ERROR;
	}

	free(hilos);
	free(datos);
	sem_close(mutex_vot);
	return OK;
}

/**
 * @brief Manda un mensaje a logger con la información de la ronda
 * @author Duna Puente y Claudia Saiz
 *
 * @param writer Descriptor de fichero de la tubería de escritura con logger
 * @param round Ronda actual
 * @param target Objetivo de la ronda actual
 * @param resultado Resultado de la ronda actual
 * @param validated Indica si se ha ganado esta ronda o no
 * @param votes Número de votos a favor esta ronda
 * @param num_procs Número de procesos que han votado en total en esta ronda
 * @return OK si ejecuta correctamente, ERROR en caso contrario
 */
int send_message(int writer, int round, target_data *target_send) {
	int nbytes = 0;
	char buffer[SIZE];
	nbytes = sprintf(buffer, "%04d|%08d|%08d|%01d|%02d|%02d", round, target_send->target, target_send->resultado, target_send->winner == getpid() ? 1 : 0, target_send->votes_yes, target_send->votes_yes + target_send->votes_no);
	if (nbytes <= 0) {
		perror("sprintf");
		fprintf(stdout, "Miner exited unexpectedly\n");
		return ERROR;
	}
	buffer[MESSAGE] = '\0';
	nbytes = write(writer, buffer, MESSAGE);
	if (nbytes <= 0) {
		perror("write");
		fprintf(stdout, "Miner exited unexpectedly\n");
		return ERROR;
	}
	return OK;
}

/**
 * @brief Lee el mensaje de CONTINUE del logger
 * @author Duna Puente y Claudia Saiz
 *
 * @param reader Descriptor de fichero de la tubería de lectura con logger
 * @return OK si ejecuta correctamente, ERROR en caso contrario
 */
int read_message(int reader) {
	int nbytes = 0;
	char buffer[SIZE];
	nbytes = read(reader, buffer, CONTINUE);
	if (nbytes == -1) {
		perror("read");
		fprintf(stdout, "Miner exited unexpectedly\n");
		return ERROR;
	} else if (nbytes != CONTINUE) {
		fprintf(stdout, "Logger closed comunication unexpectedly\n");
		return ERROR;
	}
	return OK;
}

/**
 * @brief Abre los recursos compartidos necesarios para el minero
 * @author Duna Puente y Claudia Saiz
 *
 * @param pid_mem Memoria compartida de pids
 * @param target_mem Memoria compartida de target
 * @param vot_mem Memoria compartida de votos
 * @param round_mem Memoria compartida de ronda
 * @param mutex_pid Semáforo para la memoria de pids
 * @param mutex_target Semáforo para la memoria de target
 * @param mutex_winner Semáforo de ganador
 * @param mutex_round Semáforo para la memoria de ronda
 * @param mutex_vot Semáforo para la memoria de votos
 * @return OK si ejecuta correctamente, EARLY si el minero ha entrado antes que el monitor, ERROR en caso de error
 */
int abrir_recursos(pids_data **pid_mem, target_data **target_mem, vots_data **vot_mem, pids_data **round_mem, cartera_data **cartera_mem, mqd_t *mq, sem_t **mutex_pid, sem_t **mutex_target, sem_t **mutex_winner, sem_t **mutex_round, sem_t **mutex_vot, sem_t **mutex_cart) {
	int fpid, ftarget, fvot, fround, fcartera;

	/*Abrir memoria pids*/
	fpid = shm_open(MEM_PID_NAME, O_RDWR, 0);
	if (fpid == -1) {
		if (errno == ENOENT) {
			return EARLY;
		} else {
			perror("shm_open");
		}
		return ERROR;
	}
	*pid_mem = mmap(NULL, MEM_PID_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fpid, 0);
	if (*pid_mem == MAP_FAILED) {
		perror("mmap pid");
		return ERROR;
	}
	close(fpid);

	/*Abrir memoria target*/
	ftarget = shm_open(MEM_TARGET_NAME, O_RDWR, 0);
	if (ftarget == -1) {
		if (errno == ENOENT) {
			close(fpid);
			return EARLY;
		} else {
			perror("shm_open");
		}
		close(fpid);
		return ERROR;
	}
	*target_mem = mmap(NULL, MEM_TARGET_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ftarget, 0);
	if (*target_mem == MAP_FAILED) {
		perror("mmap target");
		return ERROR;
	}
	close(ftarget);

	/*Abrir memoria voting*/
	fvot = shm_open(MEM_VOT_NAME, O_RDWR, 0);
	if (fvot == -1) {
		if (errno == ENOENT) {
			close(fpid);
			close(ftarget);
			return EARLY;
		} else {
			perror("shm_open");
		}
		close(fpid);
		close(ftarget);
		return ERROR;
	}
	*vot_mem = mmap(NULL, MEM_VOT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fvot, 0);
	if (*vot_mem == MAP_FAILED) {
		perror("mmap vot");
		return ERROR;
	}
	close(fvot);

	/*Abrir memoria round*/
	fround = shm_open(MEM_ROUND_NAME, O_RDWR, 0);
	if (fround == -1) {
		if (errno == ENOENT) {
			close(fpid);
			close(ftarget);
			close(fvot);
			return EARLY;
		} else {
			perror("shm_open");
		}
		close(fpid);
		close(ftarget);
		close(fvot);
		return ERROR;
	}
	*round_mem = mmap(NULL, MEM_ROUND_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fround, 0);
	if (*round_mem == MAP_FAILED) {
		perror("mmap round");
		return ERROR;
	}
	close(fround);

	/*Abrir memoria cartera*/
	fcartera = shm_open(MEM_CARTERA_NAME, O_RDWR, 0);
	if(fcartera == -1){
		if (errno == ENOENT) {
			close(fpid);
			close(ftarget);
			close(fvot);
			close(fround);
			return EARLY;
		} else {
			perror("shm_open");
		}
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		return ERROR;
	}
	*cartera_mem = mmap(NULL, MEM_CARTERA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fcartera, 0);
	if(cartera_mem == MAP_FAILED){
		perror("mmap cartera");
		return ERROR;
	}
	close(fcartera);

	/*Abrir cola de mensajes*/
	if ((*mq = mq_open(MQ_NAME, O_RDWR)) == (mqd_t)-1) {
		if (errno != ENOENT) {
			close(fpid);
			close(ftarget);
			close(fvot);
			close(fround);
			close(fcartera);
			return EARLY;
		}
		perror("mq_open");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
		close(fcartera);
		return ERROR;
	}

	/*Abrir semáforo mutex_pid*/
	if ((*mutex_pid = sem_open(MUTEX_PID_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
		perror("sem_open mutex_pid");
		return EARLY;
	}

	/*Abrir semáforo mutex_target*/
	if ((*mutex_target = sem_open(MUTEX_TARGET_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
		perror("sem_open mutex_target");
		sem_close(*mutex_pid);
		return EARLY;
	}

	/*Abrir semáforo mutex_winner*/
	if ((*mutex_winner = sem_open(MUTEX_WINNER_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
		perror("sem_open mutex_winner");
		sem_close(*mutex_pid);
		sem_close(*mutex_target);
		return EARLY;
	}

	/*Abrir semáforo mutex_round*/
	if ((*mutex_round = sem_open(MUTEX_ROUND_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
		perror("sem_open mutex_round");
		sem_close(*mutex_pid);
		sem_close(*mutex_target);
		sem_close(*mutex_winner);
		return EARLY;
	}

	/*Abrir semáforo mutex_vot*/
	if ((*mutex_vot = sem_open(MUTEX_VOT_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
		perror("sem_open mutex_vot");
		sem_close(*mutex_pid);
		sem_close(*mutex_target);
		sem_close(*mutex_winner);
		sem_close(*mutex_round);
		return EARLY;
	}

	if((*mutex_cart = sem_open(MUTEX_CART_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
		perror("sem_open mutex_cart");
		sem_close(*mutex_pid);
		sem_close(*mutex_target);
		sem_close(*mutex_winner);
		sem_close(*mutex_round);
		sem_close(*mutex_cart);
		return EARLY;
	}

	return OK;
}

/**
 * @brief Hace la acción de entrar en el programa y registrarse
 * @author Duna Puente y Claudia Saiz
 *
 * @param target Primer objetivo actualizado
 * @return OK si ejecuta correctamente, ERROR en caso contrario
 */
int entrar(pids_data *pid_mem, sem_t *mutex_pid, sem_t *mutex_target, sem_t *mutex_round, sem_t *mutex_cart, pids_data *round_mem, target_data *target_mem, cartera_data *cartera_mem) {
	sigset_t espera_usr1;
	int i;

	while (sem_wait(mutex_pid) == -1)
		;

		/*Registrarse en la cartera*/
	while(sem_wait(mutex_cart) == -1)
		;

	cartera_mem[pid_mem->num_pids].propietario = getpid();
	sem_post(mutex_cart);

	if (pid_mem->num_pids == 0) {
		/*Primer proceso*/

		pid_mem->pids[0] = getpid();
		pid_mem->num_pids = 1;

		/*Imprimir mensaje de entrada*/
		fprintf(stdout, "Miner %d added to the system (first process)\n", getpid());
		for (i = 0; i < pid_mem->num_pids; i++) {
			fprintf(stdout, " - %d\n", pid_mem->pids[i]);
		}
		sem_post(mutex_pid);

		/* Manda señal de SIGUSR1 para empezar la ronda*/
		while (1) {
			usleep(100);
			while (sem_wait(mutex_pid) == -1)
				;

			/*Nos aseguramos que una vez que empezamos la ronda, no se apuntaran más*/
			if (pid_mem->num_pids > 1) {
				while (sem_wait(mutex_round) == -1)
					;
				if (memcpy(round_mem, pid_mem, sizeof(pids_data)) == NULL) {
					perror("memcpy");
					sem_post(mutex_round);
					sem_post(mutex_pid);
				}
				sem_post(mutex_round);

				for (i = 0; i < pid_mem->num_pids; i++) {
					kill(pid_mem->pids[i], SIGUSR1);
				}
				sem_post(mutex_pid);
				break;
			}
			fflush(stdout);
			sem_post(mutex_pid);
		}

	} else {
		/*No primer proceso*/
		pid_mem->pids[pid_mem->num_pids] = getpid();
		pid_mem->num_pids++;

		/*Imprimir mensaje de entrada*/
		fprintf(stdout, "Miner %d added to the system\n", getpid());
		for (i = 0; i < pid_mem->num_pids; i++) {
			fprintf(stdout, " - %d\n", pid_mem->pids[i]);
		}
		fflush(stdout);
		sem_post(mutex_pid);

		/*Esperar señal SIGUSR1*/
		sigemptyset(&espera_usr1);
		sigaddset(&espera_usr1, SIGUSR2);
		sigsuspend(&espera_usr1);

		return OK;
	}

	return OK;
}

/**
 * @brief Hace la acción de salir del registro de pids y del programa
 * @author Duna Puente y Claudia Saiz
 *
 * @return OK si ejecuta correctamente, ERROR en caso contrario
 */
int salir(pids_data *pid_mem, target_data *target_mem, pids_data *round_mem, vots_data *vot_mem, cartera_data *cartera_mem, mqd_t mq, sem_t *mutex_pid, sem_t *mutex_target, sem_t *mutex_winner, sem_t *mutex_round, sem_t *mutex_vot, sem_t *mutex_cart) {
	int pos, i;
	pid_t pids_array[MAX_PROCESOS];
	target_data target_send;

	while (sem_wait(mutex_pid) == -1)
		;

	/*Mirar si es el último*/
	if (pid_mem->num_pids - 1 == 0) {
		/*Mandar mensaje a comprobador de ultimo proceso*/
		memset(&target_send, 0, sizeof(target_data));
		target_send.target = -1;
		if (mq_send(mq, (char *)&target_send, sizeof(target_data), 0) == -1) {
			perror("mq_send");
			sem_post(mutex_pid);
			return ERROR;
		}

		munmap(pid_mem, MEM_PID_SIZE);
		munmap(target_mem, MEM_TARGET_SIZE);
		munmap(vot_mem, MEM_VOT_SIZE);
		munmap(round_mem, MEM_ROUND_SIZE);
		munmap(cartera_mem, MEM_CARTERA_SIZE);
		mq_close(mq);
		sem_close(mutex_pid);
		sem_close(mutex_target);
		sem_close(mutex_winner);
		sem_close(mutex_round);
		sem_close(mutex_vot);
		sem_close(mutex_cart);
		fprintf(stdout, "Miner %d exited system (last process)\n", getpid());
		return OK;
	}

	/*Reescribir fichero sin mi PID*/
	for (i = 0; i < pid_mem->num_pids; i++) {
		pids_array[i] = pid_mem->pids[i];
		if (pid_mem->pids[i] == getpid())
			pos = i;
	}
	for (i = pos; i < pid_mem->num_pids - 1; i++) {
		pids_array[i] = pids_array[i + 1];
	}
	pid_mem->num_pids--;
	if (memcpy(pid_mem->pids, pids_array, sizeof(pid_mem->pids)) == NULL) {
		perror("memcpy");
		sem_post(mutex_pid);
		return ERROR;
	}

	/*Salir*/
	fprintf(stdout, "Miner %d exited system\n", getpid());
	sem_post(mutex_pid);

	munmap(pid_mem, MEM_PID_SIZE);
	munmap(target_mem, MEM_TARGET_SIZE);
	munmap(vot_mem, MEM_VOT_SIZE);
	munmap(round_mem, MEM_ROUND_SIZE);
	munmap(cartera_mem, MEM_CARTERA_SIZE);
	mq_close(mq);
	sem_close(mutex_pid);
	sem_close(mutex_target);
	sem_close(mutex_winner);
	sem_close(mutex_round);
	sem_close(mutex_vot);
	sem_close(mutex_cart);
	return OK;
}

/**
 * @brief Gestiona la lógica del final de una ronda
 * @author Duna Puente y Claudia Saiz
 *
 * @param target Objetivo actualizado para la siguiente ronda
 * @param validated Indica si se ha ganado la ronda o no
 * @param votes Número de votos "Y" que ha tenido
 * @param num_procs Número de procesos en total que participaron
 * @return OK si ejecuta correctamente, ERROR en caso contrario
 */
int fin_de_ronda(sem_t *mutex_pid, pids_data *pid_mem, sem_t *mutex_round, pids_data *round_mem, sem_t *mutex_vot, vots_data *vots_mem, sem_t *mutex_target, target_data *target_mem, sem_t *mutex_winner, mqd_t mq, target_data *target_send, sem_t *mutex_cart, cartera_data *cartera_mem) {
	sigset_t espera_usr1, espera_usr2;
	int try = 0, i;
	int num_pids_round, p = 0;
	int num_vots;
	int num_y, num_n;
	char str_votes[SIZE];

	target_send->target = target_mem->target;
	target_send->resultado = resultado;
	if (sem_trywait(mutex_winner) == 0) {
		/*Ganador de la ronda*/
		target_send->winner = getpid();

		while (sem_wait(mutex_target) == -1)
			;
		target_mem->target = resultado;
		sem_post(mutex_target);

		/*Resetear votos*/
		while (sem_wait(mutex_vot) == -1)
			;
		vots_mem->num_vots = 0;
		vots_mem->num_yes = 0;
		vots_mem->num_no = 0;
		sem_post(mutex_vot);

		/*Leer los procesos que han participado en esta ronda*/
		while (sem_wait(mutex_round) == -1)
			;

		for (i = 0; i < round_mem->num_pids; i++) {
			kill(round_mem->pids[i], SIGUSR2);
		}
		num_pids_round = round_mem->num_pids;
		sem_post(mutex_round);

		/*Esperar a que voten todos los procesos participantes*/
		do {
			while (sem_wait(mutex_vot) == -1)
				;
			num_vots = vots_mem->num_vots;
			sem_post(mutex_vot);

			usleep(100);
			try++;
		} while (num_vots != num_pids_round - 1 && try < MAX_TRIES);

		/*Imprimir comprobación*/
		sem_wait(mutex_vot);
		num_y = vots_mem->num_yes;
		num_n = vots_mem->num_no;
		sem_post(mutex_vot);
		str_votes[p++] = ' ';
		for (i = 0; i < num_y; i++) {
			str_votes[p++] = 'Y';
			str_votes[p++] = ' ';
		}
		for (i = 0; i < num_n; i++) {
			str_votes[p++] = 'N';
			str_votes[p++] = ' ';
		}
		str_votes[p] = '\0';

		fprintf(stdout, "Winner %d => [%s] => %s\n", getpid(), str_votes, num_y >= num_n? "Accepted" : "Rejected");
		fflush(stdout);

		/*Mandar mensaje a comprobador*/
		target_send->votes_yes = num_y;
		target_send->votes_no = num_n;
		if (mq_send(mq, (char *)target_send, sizeof(target_data), 0) == -1) {
			perror("mq_send");
			return ERROR;
		}

		/*Apuntar monedas*/
		sem_wait(mutex_cart);
		for(i = 0; i < num_pids_round; i++)
		{
			if(cartera_mem[i].propietario == getpid()){
				if(num_vots > ((num_y+num_n)-num_vots)){
					cartera_mem[i].monedas++;
				}
			}
		}
		sem_post(mutex_cart);

		/* Manda señal de SIGUSR1 para empezar la ronda*/
		while (1) {
			while (sem_wait(mutex_pid) == -1)
				;

			/*Nos aseguramos que una vez que empezamos la ronda, no se apuntan más*/
			if (pid_mem->num_pids > 1) {
				if (memcpy(round_mem, pid_mem, sizeof(pids_data)) == NULL) {
					perror("memcpy");
					sem_post(mutex_pid);
				}
				sem_post(mutex_pid);
				break;
			}

			/*Si somos el último proceso y se acaba nuestro tiempo nos salimos*/
			if (flag) {
				sem_post(mutex_pid);
				break;
			}
			sem_post(mutex_pid);
			usleep(100);
		}

		sem_wait(mutex_pid);
		for (i = 0; i < pid_mem->num_pids; i++) {
			kill(pid_mem->pids[i], SIGUSR1);
		}
		sem_post(mutex_pid);
		sem_post(mutex_winner);
	} else {
		/*Votante*/
		target_send->winner = -1;
		target_send->votes_yes = 0;
		target_send->votes_no = 0;

		/*Esperar señal SIGUSR2*/
		sigemptyset(&espera_usr2);
		sigaddset(&espera_usr2, SIGUSR1);
		sigsuspend(&espera_usr2);

		/*Escribir mi voto en fichero de votos*/
		while (sem_wait(mutex_target) == -1)
			;
		while (sem_wait(mutex_vot) == -1)
			;
		vots_mem->num_vots++;
		if (target_mem->target == resultado) {
			vots_mem->num_yes++;
		} else {
			vots_mem->num_no++;
		}
		sem_post(mutex_vot);
		sem_post(mutex_target);

		/*Salen si ha terminado su tiempo*/
		if (flag) {
			return OK;
		}

		/*Espera señal SIGUSR1 de empezar nueva ronda*/
		sigemptyset(&espera_usr1);
		sigaddset(&espera_usr1, SIGUSR2);
		sigsuspend(&espera_usr1);
	}
	return OK;
}