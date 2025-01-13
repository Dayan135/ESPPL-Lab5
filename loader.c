#include <stdio.h>
#include <stdlib.h>
#include <elf.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define PAGE_SIZE 0x1000
#define PAGE_MASK (~(PAGE_SIZE - 1))

// Helper to align addresses to page boundaries
#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))
#define ALIGN_UP(addr, align) (((addr) + ((align) - 1)) & ~((align) - 1))

// Convert ELF segment flags to mmap protection flags
int get_prot(uint32_t flags) {
    int prot = 0;
    if (flags & PF_R) prot |= PROT_READ;
    if (flags & PF_W) prot |= PROT_WRITE;
    if (flags & PF_X) prot |= PROT_EXEC;
    return prot;
}

// Load a single program header
void load_phdr(Elf32_Phdr *phdr, int fd) {
    if (phdr->p_type != PT_LOAD) return;

    // Calculate aligned addresses and offsets
    Elf32_Addr vaddr_aligned = ALIGN_DOWN(phdr->p_vaddr, PAGE_SIZE);
    Elf32_Off offset_aligned = ALIGN_DOWN(phdr->p_offset, PAGE_SIZE);
    size_t padding = phdr->p_vaddr & (PAGE_SIZE - 1);

    size_t map_size = ALIGN_UP(phdr->p_memsz + padding, PAGE_SIZE);
    int prot = get_prot(phdr->p_flags);

    // Map the segment into memory
    void *map = mmap((void *)vaddr_aligned, map_size, prot,
                     MAP_PRIVATE | MAP_FIXED, fd, offset_aligned);
    if (map == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        exit(1);
    }

    // If p_memsz > p_filesz, zero out the extra memory
    if (phdr->p_memsz > phdr->p_filesz) {
        size_t diff = phdr->p_memsz - phdr->p_filesz;
        memset((void *)(phdr->p_vaddr + phdr->p_filesz), 0, diff);
    }
}

// Iterate over program headers and load them
int foreach_phdr(void *map_start, void (*func)(Elf32_Phdr *, int), int fd) {
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)map_start;

    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not a valid ELF file\n");
        return -1;
    }

    Elf32_Phdr *phdr_table = (Elf32_Phdr *)((char *)map_start + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        func(&phdr_table[i], fd);
    }

    return 0;
}

// Main loader function
void load_elf(const char *filename) {
    // Open the ELF file
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        exit(1);
    }

    // Map the ELF header into memory
    Elf32_Ehdr *ehdr = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ehdr == MAP_FAILED) {
        perror("Failed to mmap ELF header");
        close(fd);
        exit(1);
    }

    // Load all program headers
    foreach_phdr(ehdr, load_phdr, fd);

    // Unmap the ELF header and close the file
    munmap(ehdr, PAGE_SIZE);
    close(fd);

    // Jump to the entry point
    void (*entry_point)() = (void (*)())ehdr->e_entry;
    entry_point();
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ELF file>\n", argv[0]);
        return 1;
    }

    load_elf(argv[1]);
    return 0;
}
