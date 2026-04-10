CC = gcc -ansi -pedantic
CFLAGS = -Wall
EXE = miner
OBJ = pow.o logger.o worker.o miner.o

all : $(EXE)

.PHONY : clean
clean :
	rm -f *.o core $(EXE)

cleanlog :
	rm -f *.log *.pid *.tgt

$(EXE) : $(OBJ)
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o : %.c
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -c $^

run: cleanlog
	@echo Running miner
	@./miner 2 10 & ./miner 3 1 & ./miner 4 5

runv: cleanlog
	@echo Running miner valgrind
	@valgrind --leak-check=full --track-origins=yes --trace-children=yes ./miner 10 5