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
#define STR_LENTH 20

struct scm
{
    int fd;
    char *memory;
    size_t capacity;
    size_t utilized;
    char *base;
};

struct scm *scm_open(const char *pathname, int truncate)
{
    struct stat file_stat;
    long size_value;
    char size_str[STR_LENTH];
    const char *strInitial = "S";

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

    if (fstat(scm->fd, &file_stat) == -1)
    {
        TRACE("fstat");
        close(scm->fd);
        free(scm);
        return NULL;
    }

    if (!S_ISREG(file_stat.st_mode))
    {
        TRACE("not a regular file");
        close(scm->fd);
        free(scm);
        return NULL;
    }
    scm->capacity = file_stat.st_size;

    scm->memory = (char *)mmap((void *)VIRT_ADDR, scm->capacity, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, scm->fd, 0);
    if (scm->memory == MAP_FAILED)
    {
        close(scm->fd);
        free(scm);
        return NULL;
    }

    if (truncate)
    {
        scm->utilized = 0;

        /* strcpy(scm->memory, strInitial); */
        if(strcpy(scm->memory, strInitial)==NULL){
            TRACE("copy failed");
            close(scm->fd);
            free(scm);
            return NULL;
        }
        scm->base = scm->memory + sizeof(strInitial);

        if(snprintf(size_str, sizeof(size_str), "%ld", scm->utilized)<0){
            TRACE("copy failed");
            close(scm->fd);
            free(scm);
            return NULL;
        }
        /* strcpy(scm->base, size_str); */
        if( strcpy(scm->base, size_str)==NULL){
            TRACE("copy failed");
            close(scm->fd);
            free(scm);
            return NULL;
        }
        scm->base = scm->base + sizeof(size_str);
    }
    else
    {

        if (strcmp(strInitial, scm->memory) != 0)
        {
            TRACE("please truncate");
            close(scm->fd);
            free(scm);
            return NULL;
        }

        scm->base = scm->memory + sizeof(strInitial);

        /* sscanf(scm->base, "%ld", &size_value); */
        if(sscanf(scm->base, "%ld", &size_value)==0){
            TRACE("sscanf failed");
            close(scm->fd);
            free(scm);
            return NULL;
        }
        scm->utilized = size_value;
        scm->base = scm->base + sizeof(size_str);
    }

    return scm;
}

void scm_close(struct scm *scm)
{
    char str_size[STR_LENTH];
    if (scm == NULL)
    {
        TRACE("close");
        return;
    }

    if(snprintf(str_size, sizeof(str_size), "%ld", scm->utilized)<0){
        TRACE("copy failed");
        close(scm->fd);
        free(scm);
        return;

    }

    /* strcpy(scm->base - STR_LENTH, str_size); */
    if(strcpy(scm->base - STR_LENTH, str_size)==NULL){
        TRACE("copy failed");
        close(scm->fd);
        free(scm);
        return;
    }

    if (msync(scm->memory, scm->capacity, MS_SYNC) == -1)
    {
        TRACE("msync");
    }

    if (munmap(scm->memory, scm->capacity) == -1)
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
    void *ptr;

    if (scm == NULL || n == 0)
    {
        return NULL;
    }

    if (scm->utilized + n > scm->capacity)
    {
        return NULL;
    }

    ptr = (size_t *)scm->base + scm->utilized;
    scm->utilized += n;

    return ptr;
}

char *scm_strdup(struct scm *scm, const char *s)
{
    size_t len;
    char *str_copy;

    if (scm == NULL || s == NULL)
    {
        TRACE("scm");
        return NULL;
    }

    len = strlen(s) + 1;
    str_copy = (char *)scm_malloc(scm, len);
    if (str_copy == NULL)
    {
        TRACE("str_copy");
        return NULL;
    }

    strcpy(str_copy, s);

    return str_copy;
}

void scm_free(struct scm *scm, void *p)
{

    (void)scm;
    (void)p;
}

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
    return scm->capacity;
}

void *scm_mbase(struct scm *scm)
{
    if (scm == NULL)
    {
        return NULL;
    }
    return scm->base;
}
