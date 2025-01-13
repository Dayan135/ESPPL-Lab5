all: main

main: main.c
	gcc -m32 -o main main.c

loader: loader.c start.o
	gcc -m32 -c loader.c -o loader.o
	ld -o loader loader.o start.o -L/usr/lib32 -lc -T linking_script -dynamic-linker /lib32/ld-linux.so.2 

startup.o: startup.s
	nasm -f elf32 startup.s -o startup.o


start.o: start.s
	nasm -f elf32 start.s -o start.o