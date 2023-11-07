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

struct scm
{
    int fd;
    size_t size;
    size_t utilized;
    char *memory;
};

static size_t scm_getCapacity(struct scm *scm)
{
    size_t utilized;
    size_t size_to_utilized = sizeof(size_t);
    if (lseek(scm->fd, -size_to_utilized, SEEK_END) == -1)
    {
        TRACE("lseek");
    }
    if (read(scm->fd, &utilized, size_to_utilized) == -1)
    {
        TRACE("read");
    }

    return utilized;
}

struct scm *scm_open(const char *pathname, int truncate)
{
    struct stat file_state;
    void *current_address;
    struct scm *scm = (struct scm *)malloc(sizeof(struct scm));
    if (scm == NULL)
    {
        TRACE("malloc");
        return NULL;
    }

    scm->fd = open(pathname, O_RDWR, S_IRUSR | S_IWUSR);
    if (scm->fd == -1)
    {
        TRACE("open");
        free(scm);
        return NULL;
    }
    printf("open end\n");

    if (fstat(scm->fd, &file_state) != 0)
    {
        TRACE("fstat");
        free(scm);
        return NULL;
    }
    scm->size = file_state.st_size;

    if (!S_ISREG(file_state.st_mode))
    {
        printf("%s is not a regular file.\n", pathname);
        TRACE("S_ISREG");
        free(scm);
        return NULL;
    }

    if (!truncate)
    {
        current_address = sbrk(0);
        if (current_address == (void *)-1)
        {
            close(scm->fd);
            free(scm);
            return NULL;
        }
        if (current_address < (void *)VIRT_ADDR)
        {
            sbrk((char *)VIRT_ADDR - (char *)current_address);
        }
        scm->utilized  = scm_getCapacity(scm);
    }
    else
    {
        scm->utilized = 0;
    }
    scm->memory = (char *)mmap((void *)VIRT_ADDR, scm->size, PROT_READ | PROT_WRITE, MAP_SHARED, scm->fd, 0);
    if (scm->memory == MAP_FAILED)
    {
        TRACE("mmap");
        close(scm->fd);
        free(scm);
        return NULL;
    }
    return scm;
}

void scm_close(struct scm *scm)
{
    if (scm == NULL)
    {
        return;
    }

    if (lseek(scm->fd, -sizeof(size_t), SEEK_END) == (off_t)-1)
    {
        TRACE("lseek");
    }

    if (write(scm->fd, &scm->utilized, sizeof(scm->utilized)) == -1)
    {
        TRACE("write");
    }

    if (msync(scm->memory, scm->size, MS_SYNC) == -1)
    {
        TRACE("msync");
    }

    if (munmap(scm->memory, scm->size) == -1)
    {
        TRACE("munmap");
    }
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
    char *str;

    if (scm->utilized + len > scm->size)
    {
        return NULL;
    }

    str = (char *)scm_malloc(scm, len);
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