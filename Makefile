CC = gcc -ansi -pedantic
CFLAGS = -Wall
EXE = miner
OBJ = miner.o pow.o logger.o

all : $(EXE)

.PHONY : clean
clean :
	rm -f *.o core $(EXE)

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

run:
	@echo Running miner
	@./miner 0 5 1

runv:
	@echo Running miner valgrind
	@valgrind --leak-check=full --track-origins=yes --trace-children=yes ./miner 0 5 3