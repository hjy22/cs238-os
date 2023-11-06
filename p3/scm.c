/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scm.c
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "scm.h"
#define VIRT_ADDR 0x600000000000
/**
 * Needs:
 *   fd - file descriptor
 *   fstat(fd,&s)&s-struct, check the struct state
 *   S_ISREG(st.mode)
 *   open(path_name,O_RDWR-Open for reading and writing)
 *   close(fd)
 *   current <- sbrk(VIRT_ADDR), move the break line
 *   ADDR <- mmap(VIRT_ADDR,lenth,PROT_READ|PROT_WRITE,MAP_FIXED|MAP_SHARED,fd,0)
 *                      -get the hint the address you want, check address
 *                      -for failure, the function returns MAP_FAILED
 *   munmap()
 *   msync()
 *
 *   MAPPED_REGION      0-current
 *   UN MAPPED_REGION   current - (ADDR-1)
 *   SCM REGION         ADDR - ADDR+LENGTH-1
 */

struct scm
{
    int fd;
    size_t size;
    size_t utilized;
    char *memory;
};

static int scm_truncate(struct scm *scm)
{
    printf("scm_truncate1\n");
    if (ftruncate(scm->fd, scm->size) == -1)
    {
        perror("ftruncate");
        return -1;
    }
    printf("scm_truncate2\n");
    return 0;
}

static size_t scm_getCapacity(struct scm *scm)
{
    size_t utilized;
    size_t size_to_utilized;
    size_to_utilized = sizeof(size_t);
    if (lseek(scm->fd, -size_to_utilized, SEEK_END) == -1)
    {
        perror("lseek");
    }
    if (read(scm->fd, &utilized, size_to_utilized) == -1)
    {
        perror("read");
    }
    printf("read end\n");

    printf("utilized%lu\n", utilized);

    /* if (lseek(scm->fd, -size_to_utilized, SEEK_END) == -1)
    {
        perror("lseek");
        close(scm->fd);
        return -1;
    }

    if (ftruncate(scm->fd, scm->size) == -1)
    {
        perror("ftruncate");
        close(scm->fd);
        return -1;
    }
    printf("scm->size%ld\n", scm->size);
    printf("ftruncate end\n"); */
    return utilized;
}

struct scm *scm_open(const char *pathname, int truncate)
{
    int flags;
    struct stat st;
    off_t file_size;
    size_t utilized;
    void *current;
    struct scm *scm = (struct scm *)malloc(sizeof(struct scm));
    if (scm == NULL)
    {
        perror("malloc");
        return NULL;
    }

    printf("malloc end\n");

    flags = O_RDWR | O_CREAT;
    if (!truncate)
    {
        flags = O_RDWR | O_APPEND;
    }

    scm->fd = open(pathname, flags, S_IRUSR | S_IWUSR);
    if (scm->fd == -1)
    {
        perror("open");
        free(scm);
        return NULL;
    }
    printf("open end\n");

    if (fstat(scm->fd, &st) != 0)
    {
        perror("fstat");
        free(scm);
        return NULL;
    }
    scm->size = st.st_size;
    printf("utilzed%lu\n", scm->utilized);
    printf("size%lu\n", scm->size);
    file_size = st.st_size;
    printf("File size: %ld bytes\n", (long)file_size);

    if (!S_ISREG(st.st_mode))
    {
        printf("%s is not a regular file.\n", pathname);
        free(scm);
        return NULL;
    }
    printf("S_ISREG end\n");
    printf("truncate%i\n", truncate);
    if (truncate && scm_truncate(scm) == -1)
    {
        close(scm->fd);
        free(scm);
        return NULL;
    }
    printf("scm_truncate end\n");

    if (!truncate)
    {
        current = sbrk(0);
        if (current == (void *)-1)
        {
            close(scm->fd);
            free(scm);
            return NULL;
        }
        if (current < (void *)VIRT_ADDR)
        {
            sbrk((char *)VIRT_ADDR - (char *)current);
        }

        scm->memory = (char *)mmap((void *)VIRT_ADDR, scm->size, PROT_READ | PROT_WRITE, MAP_SHARED, scm->fd, 0);
        utilized = scm_getCapacity(scm);
        printf("truncate utilized%ld\n", utilized);
        scm->utilized = utilized;

        printf("truncate end\n");
    }
    else
    {
        scm->utilized = 0;
        scm->memory = (char *)mmap((void *)VIRT_ADDR, scm->size, PROT_READ | PROT_WRITE, MAP_SHARED, scm->fd, 0);
    }

    if (scm->memory == MAP_FAILED)
    {
        perror("mmap");
        close(scm->fd);
        free(scm);
        return NULL;
    }

    printf("mmap end");

    return scm;
}

void scm_close(struct scm *scm)
{
    if (scm == NULL)
    {
        return;
    }

    if (ftruncate(scm->fd, scm->size - sizeof(scm->utilized)) == -1)
    {
        perror("ftruncate");
        close(scm->fd);
        return;
    }
    printf("ftruncate: scm->size%ld\n", scm->size);
    if (lseek(scm->fd, 0, SEEK_END) == -1)
    {
        perror("lseek");
    }
    printf("sizeof(scm->utilized)%ld\n", sizeof(scm->utilized));

    if (write(scm->fd, &scm->utilized, sizeof(scm->utilized)) == -1)
    {
        perror("write");
    }
    printf("write end\n");

    if (msync(scm->memory, scm->size, MS_SYNC) == -1)
    {
        perror("msync");
    }
    printf("msync end\n");

    if (munmap(scm->memory, scm->size) == -1)
    {
        perror("munmap");
    }
    printf("close: scm->size%ld\n", scm->size);
    if (close(scm->fd) == -1)
    {
        TRACE("close");
    }

    free(scm);
}

void *scm_malloc(struct scm *scm, size_t n)
{
    void *memory;
    if (scm == NULL || n == 0)
    {
        return NULL;
    }

    if (scm->utilized + n > scm->size)
    {
        return NULL;
    }

    memory = scm->memory + scm->utilized;
    scm->utilized += n;
    return memory;
}

char *scm_strdup(struct scm *scm, const char *s)
{
    size_t len = strlen(s) + 1;
    char *str = (char *)scm_malloc(scm, len);
    if (str == NULL)
    {
        return NULL;
    }

    strcpy(str, s);
    return str;
}

void scm_free(struct scm *scm, void *p);

size_t scm_utilized(const struct scm *scm)
{
    if (scm == NULL)
    {
        return 0;
    }
    return scm->utilized;
}

size_t scm_capacity(const struct scm *scm)
{
    if (scm == NULL)
    {
        return 0;
    }
    return scm->size;
}

void *scm_mbase(struct scm *scm)
{
    if (scm == NULL)
    {
        return NULL;
    }
    return scm->memory;
}
