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

	/*Fork()*/
	pid_reg = fork();
	if (pid_reg < 0) {
		perror("fork");
		exit(EXIT_FAILURE);

	} else if (pid_reg == 0) {

		/*Monitor*/
		if (!monitor_actions()) {
			exit(EXIT_FAILURE);
		}

	} else {

		/*Comprobador*/
		if (!comprobador_actions()) {
			fprintf(stdout, "Comprobador exited unexpectedly\n");
			exit(EXIT_FAILURE);
		}

		/*Esperar al proceso hijo*/
		if (waitpid(pid_reg, &status, 0) == -1) {
			perror("waitpid");
			exit(EXIT_FAILURE);
		}
		if (WIFEXITED(status)) {
			fprintf(stdout, "Monitor exited with status %d\n", WEXITSTATUS(status));
		} else {
			fprintf(stdout, "Monitor exited unexpectedly\n");
		}

		/*Mensaje de salida*/
		fprintf(stdout, "Comprobador exited with status %d\n", EXIT_SUCCESS);
		exit(EXIT_SUCCESS);
	}

	exit(EXIT_SUCCESS);
}

int monitor_actions() {

	/*Empezar programa: crear memoria compartida y semáforos*/
	if (!monitor_inicializar()) {
		monitor_salir();
		/* Salir y hacer unlink de todo
		Comunicar al resto que he acabado?? Como??*/
		return ERROR;
	}

	printf("Monitor started\n");
	printf("Waiting for miners to join the system...\n");
	/*Comunicar mediante mensajes con comprobador*/
	sleep(10);
	printf("Finishing monitor\n");

	/*Salir: borrar memoria compartida y semáforos*/
	if (!monitor_salir()) {
		return ERROR;
	}

	return 1;
}

int comprobador_actions() {
	mqd_t mq;

	while ((mq = mq_open(MQ_NAME, O_RDWR)) == (mqd_t)-1) {
		if (errno != ENOENT) {
			perror("mq_open");
			return ERROR;
		}
		usleep(100);
	}

	mq_close(mq);
	sleep(4);

	return 1;
}

int monitor_inicializar() {
	int fpid, ftarget, fvot, fround;
	pids_data *pid_mem = NULL, *round_mem = NULL;
	vots_data *vot_mem = NULL;
	target_data *target_mem = NULL;
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

	/*Abrir cola de mensajes*/

	attributes.mq_maxmsg = 10;
	attributes.mq_msgsize = MAX_MESSAGE;
	if ((mq = mq_open(MQ_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR, &attributes)) == (mqd_t)-1) {
		perror(" mq_open ");
		close(fpid);
		close(ftarget);
		close(fvot);
		close(fround);
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
		sem_close(mutex_pid);
		sem_close(mutex_target);
		sem_close(mutex_winner);
		sem_close(mutex_round);
		return ERROR;
	}
	sem_close(mutex_vot);

	return 1;
}

int monitor_salir() {

	/*Cerrar memoria compartida*/
	shm_unlink(MEM_PID_NAME);
	shm_unlink(MEM_TARGET_NAME);
	shm_unlink(MEM_VOT_NAME);
	shm_unlink(MEM_ROUND_NAME);

	/*Cerrar cola de mensajes*/
	mq_unlink(MQ_NAME);

	/*Cerrar semáforos*/
	sem_unlink(MUTEX_PID_NAME);
	sem_unlink(MUTEX_TARGET_NAME);
	sem_unlink(MUTEX_WINNER_NAME);
	sem_unlink(MUTEX_ROUND_NAME);
	sem_unlink(MUTEX_VOT_NAME);

	return 1;
}