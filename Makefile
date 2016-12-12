all: test

gc.o: gc.c gc.h
	$(CC) -Wall -O3 -g -o $@ -c gc.c

test.o : test.c
	$(CC) -Wall -O3 -o $@ -c test.c

test: test.o gc.o
	$(CC) -o $@ $+


clean:
	rm -f test *.o