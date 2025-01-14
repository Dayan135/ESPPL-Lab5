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

//this point!

/* Minimal write for printing errors, since we're not using libc: */
static void write_str(const char *s) {
    while (*s) {
        write(2, s, 1);  /* fd=2 is stderr */
        s++;
    }
}

/* Convert p_flags (PF_R, PF_W, PF_X) to mmap PROT_* flags. */
int phdr_to_prot(Elf32_Word p_flags) {
    int prot = 0;
    if (p_flags & PF_R) prot |= PROT_READ;
    if (p_flags & PF_W) prot |= PROT_WRITE;
    if (p_flags & PF_X) prot |= PROT_EXEC;
    return prot;
}

/* For each PT_LOAD header, do a MAP_FIXED mmap at p_vaddr (page-aligned). */
static void load_phdr(Elf32_Phdr *phdr, int fd)
{
    if (phdr->p_type == PT_LOAD) {
        /* Align everything to page boundary (0x1000). */
        unsigned long page_size = 0x1000;  /* or sysconf(_SC_PAGE_SIZE) */
        unsigned long vaddr_aligned  = phdr->p_vaddr & ~(page_size - 1);
        unsigned long offset_aligned = phdr->p_offset & ~(page_size - 1);

        /* Extra offset within the page. E.g. if p_vaddr = 0x08048004, 
           then 'padding' is 4. We'll map p_filesz + that. */
        unsigned long padding = phdr->p_vaddr & (page_size - 1);
        unsigned long length  = phdr->p_filesz + padding;  
        
        /* Convert flags from ELF to mmap protections. */
        int prot = phdr_to_prot(phdr->p_flags);

        /* Map from 'offset_aligned' in the file, at 'vaddr_aligned' in memory. */
        void *map_addr = mmap((void*)vaddr_aligned, length, prot,
                              MAP_PRIVATE | MAP_FIXED, fd, offset_aligned);
        if (map_addr == (void*)-1) {
            write_str("mmap failed\n");
            _exit(1);  /* _exit via syscall, or use your own error path */
        }

    }
}

/* Iterate over all program headers and call load_phdr on each. */
static void foreach_phdr(void *map_start, int fd)
{
    Elf32_Ehdr *ehdr = (Elf32_Ehdr*)map_start;
    Elf32_Phdr *phdr_table = (Elf32_Phdr*)((char*)map_start + ehdr->e_phoff);
    int i;
    for (i = 0; i < ehdr->e_phnum; i++) {
        load_phdr(&phdr_table[i], fd);
    }
}

/* Minimal entry point of the loader */
int main(int argc, char **argv)
{
    if (argc < 2) {
        write_str("Usage: loader <ELF file>\n");
        return 1;
    }

    printf("%d",argc);

    /* Open the ELF file */
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        write_str("open failed\n");
        return 1;
    }

    /* Get file size with fstat */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        write_str("fstat failed\n");
        close(fd);
        return 1;
    }

    /* mmap the entire file read-only (so we can read the headers). */
    void *map_start = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_start == (void*)-1) {
        write_str("mmap for file reading failed\n");
        close(fd);
        return 1;
    }

    /* Check the ELF header is for a 32-bit, etc. (Optional checks) */
    Elf32_Ehdr *ehdr = (Elf32_Ehdr*)map_start;
    if (ehdr->e_ident[0] != 0x7f || 
        ehdr->e_ident[1] != 'E'  ||
        ehdr->e_ident[2] != 'L'  ||
        ehdr->e_ident[3] != 'F') 
    {
        write_str("Not an ELF file\n");
        _exit(1);
    }

    /* Now map each PT_LOAD segment at its desired address. */
    foreach_phdr(map_start, fd);

    /* We have loaded the code. The entry point is e_entry. 
       We transfer control using the 'startup' assembly glue. */
    void (*entry_point)(void) = (void(*)(void))ehdr->e_entry;

    /* startup(argc-1, argv+1, entry_point):
       - The 'startup' function sets up a basic stack for the loaded code,
         and calls 'entry_point' with the requested argv (skipping loader's argv[0]). 
     */
    startup(argc - 1, argv + 1, (void*)entry_point);

    /* Usually won't reach here if the loaded program calls exit.
       But just in case: */
    _exit(0);
    return 0; /* unreachable */
}
