all: main

main: main.c
	gcc -m32 -o main main.c