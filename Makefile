CC = gcc -ansi -pedantic
CFLAGS = -Wall
CLIBS = -lrt
EXE = miner monitor
OBJ_MINER = pow.o logger.o worker.o miner.o
OBJ_MONITOR = monitor.o

all : $(EXE)

.PHONY : clean
clean :
	rm -f *.o core $(EXE)

cleanlog :
	rm -f *.log

miner: $(OBJ_MINER)
	@echo "#---------------------------"
	@echo "# Generating $@"
	$(CC) $(CFLAGS) -o $@ $(OBJ_MINER) $(CLIBS)

monitor: $(OBJ_MONITOR)
	@echo "#---------------------------"
	@echo "# Generating $@"
	$(CC) $(CFLAGS) -o $@ $(OBJ_MONITOR) $(CLIBS)

%.o : %.c
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -c $^

run_monitor: cleanlog
	@echo Running miner
	@./monitor 100 10

runv_monitor: cleanlog
	@echo Running miner
	@valgrind --leak-check=full --track-origins=yes --trace-children=yes ./monitor 100 10

run: cleanlog
	@echo Running miner
	@./miner 10 10 & ./miner 12 1 & ./miner 13 5 & ./miner 15 5

runv: 
	@echo Running miner valgrind
	@valgrind --leak-check=full --track-origins=yes --trace-children=yes ./miner 4 10 & ./miner 6 1 