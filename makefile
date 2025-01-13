all: main

# main: main.c
# 	gcc -m32 -o main main.c

main: main.c start.o
	gcc -m32 -D_FILE_OFFSET_BITS=64 -c main.c -o main.o
	ld -o main main.o startup.o start.o -L/usr/lib32 -lc -T linking_script -dynamic-linker /lib32/ld-linux.so.2 

startup.o: startup.s
	nasm -f elf32 startup.s -o startup.o


start.o: start.s
	nasm -f elf32 start.s -o start.o