
all: main.o art.o
	gcc main.o art.o -o all

main.o: main.c
	gcc -c main.c

art.o: art.c art.h
	gcc -c art.c


