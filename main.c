#include <stdio.h>
#include <elf.h>
#include <stdlib.h>

int foreach_phdr(void *map_start, void (*func)(Elf32_Phdr *,int), int arg){
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)map_start;
    //assuming that got a 32-bit ELF file
    Elf32_Phdr *phdr_table = (Elf32_Phdr *)((char *)map_start + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        func(&phdr_table[i], i); // Apply the function to each program header
    }

    return 0;
}

void print_phdr(Elf32_Phdr *phdr, int i) {
    printf("Program header number %d at address %p\n", i, (void *)phdr);
}

int main(int argc, char** argv){
    if(argc != 2){
        printf("wrong arguments number!\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Map the ELF file into memory
    void *map_start = malloc(file_size);
    if (!map_start) {
        perror("Error allocating memory");
        fclose(file);
        return 1;
    }

    fread(map_start, 1, file_size, file);
    fclose(file);

    // Apply the iterator function
    int result = foreach_phdr(map_start, print_phdr, 0);

    // Cleanup
    free(map_start);
    return result;

    return 0;
}