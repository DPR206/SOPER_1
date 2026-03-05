CC = gcc -ansi -pedantic
CFLAGS = -Wall
EXE = miner

all : $(EXE)

.PHONY : clean
clean :
	rm -f *.o core $(EXE)

$(EXE) : % : %.o pow.o
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depepends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -o $@ $@.o pow.o

pow.o : pow.c pow.h
	@echo "#---------------------------"
	@echo "# Generating $@ "
	@echo "# Depepends on $^"
	@echo "# Has changed $<"
	$(CC) $(CFLAGS) -c $<

run:
	@echo Running miner
	@./miner 0 5 1

runv:
	@echo Running miner valgrind
	@valgrind --leak-check=full --track-origins=yes --trace-children=yes ./miner 0 5 3