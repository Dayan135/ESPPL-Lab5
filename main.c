#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <elf.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

extern int startup(int argc, char **argv, void (*start)());

const char *phdr_type_to_string(uint32_t type) {
    switch (type) {
        case PT_NULL:    return "NULL";
        case PT_LOAD:    return "LOAD";
        case PT_DYNAMIC: return "DYNAMIC";
        case PT_INTERP:  return "INTERP";
        case PT_NOTE:    return "NOTE";
        case PT_SHLIB:   return "SHLIB";
        case PT_PHDR:    return "PHDR";
        case PT_TLS:     return "TLS";
        default:         return "UNKNOWN";
    }
}

// Function to print the flags of the program header
void phdr_flags_to_string(uint32_t flags, char *buf) {
    buf[0] = (flags & PF_R) ? 'R' : ' ';
    buf[1] = (flags & PF_W) ? 'W' : ' ';
    buf[2] = (flags & PF_X) ? 'E' : ' ';
    buf[3] = '\0'; // Null-terminate
}

//task 0
void print_phdr0(Elf32_Phdr *phdr, int i) {
    printf("Program header number %d at address %p\n", i, (void *)phdr);
}

// task 1a - Function to print a single program header
void print_phdr_info(Elf32_Phdr *phdr, int index) {
    char flags[4];
    phdr_flags_to_string(phdr->p_flags, flags);

    if(index == 0){
        printf("%-8s %-8s %-10s %-10s %-7s %-7s %-3s %-3s\n",
           "type",
           "offset",
           "VirtAddr",
           "PhysAddr",
           "FileSiz",
           "MemSiz",
           "Flg",
           "Align");
    }

    printf("%-8s 0x%06x 0x%08x 0x%08x 0x%05x 0x%05x %-3s 0x%x\n",
           phdr_type_to_string(phdr->p_type),
           phdr->p_offset,
           phdr->p_vaddr,
           phdr->p_paddr,
           phdr->p_filesz,
           phdr->p_memsz,
           flags,
           phdr->p_align);
}


int foreach_phdr(void *map_start, void (*func)(Elf32_Phdr *,int), int arg){
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)map_start;
    //assuming that got a 32-bit ELF file
    Elf32_Phdr *phdr_table = (Elf32_Phdr *)((char *)map_start + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        func(phdr_table + i, i); // Apply the function to each program header
    }

    return 0;
}

int protection_Map(int flag)
{
    if (flag == 0)
        return 0;
    if (flag == 1)
        return PROT_EXEC;
    if (flag == 2)
        return PROT_WRITE;
    if (flag == 3)
        return PROT_WRITE | PROT_EXEC;
    if (flag == 4)
        return PROT_READ;
    if (flag == 5)
        return PROT_READ | PROT_EXEC;
    if (flag == 6)
        return PROT_READ | PROT_WRITE;
    if (flag == 7)
        return PROT_READ | PROT_WRITE | PROT_EXEC;
    return -1;
}

void load_phdr(Elf32_Phdr *phdr, int fd) //map a program header segment into memory
{
    
    if (phdr->p_type == PT_LOAD)
    {
        print_phdr_info(phdr, 0);
        void *vadd = (void *)(phdr->p_vaddr & 0xfffff000);
        int offset = phdr->p_offset & 0xfffff000;
        int padding = phdr->p_vaddr & 0xfff;
        int convertedFlag = protection_Map(phdr->p_flags);
        if (mmap(vadd, phdr->p_memsz + padding, convertedFlag, MAP_FIXED | MAP_PRIVATE, fd, offset) == MAP_FAILED)
        {
            perror("mmap failed1");
            exit(-1);
        }
    }
}



int main(int argc, char** argv){
    if(argc != 2){
        printf("wrong arguments number!\n");
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if(fd < 0) {
        perror("open failed");
        return 1;
    }

    struct stat fd_stat;
    if (fstat(fd, &fd_stat) == -1)
    {
        perror("fstat");
        close(fd);
        return 1;
    }
    void *map_start =mmap(0, fd_stat.st_size, PROT_READ , MAP_PRIVATE, fd, 0);

    if (map_start == MAP_FAILED)
    {
        perror("mmap failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Type Offset VirtAddr PhysAddr FileSiz MemSiz Flg Align\n");
    Elf32_Ehdr *h = (Elf32_Ehdr *)map_start;

    foreach_phdr(map_start, load_phdr, fd); 
    startup(argc - 1, argv + 1, (void *)(h->e_entry));

    // FILE *file = fopen(argv[1], "rb");
    // if (!file) {
    //     perror("Error opening file");
    //     return 1;
    // }

    // // Get the file size
    // fseek(file, 0, SEEK_END);
    // long file_size = ftell(file);
    // fseek(file, 0, SEEK_SET);

    // void *map_start;
    // struct stat fd_stat;

    // if (fstat(file, &fd_stat) == -1)
    // {
    //     perror("fstat");
    //     close(file);
    // }

    // if ((map_start = mmap(0, fd_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0)) == MAP_FAILED)
    // {
    //     perror("mmap failed");
    //     exit(EXIT_FAILURE);
    // }

    // Elf32_Ehdr *h = (Elf32_Ehdr *)map_start;

    // foreach_phdr(map_start, load_phdr, file); 
    // startup(argc - 1, argv + 1, (void *)(h->e_entry));

    // Map the ELF file into memory
    // void *map_start = malloc(file_size);
    // if (!map_start) {
    //     perror("Error allocating memory");
    //     fclose(file);
    //     return 1;
    // }

    // fread(map_start, 1, file_size, file);
    // fclose(file);

    // // Apply the iterator function
    // int result = foreach_phdr(map_start, print_phdr_info, 0);

    // // Cleanup
    // free(map_start);
    // return result;

    return 0;
}