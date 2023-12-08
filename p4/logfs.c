/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * logfs.c
 */

#include <pthread.h>
#include "device.h"
#include "logfs.h"

#define WCACHE_BLOCKS 32
#define RCACHE_BLOCKS 256
struct circular_buffer
{
    void *buffer;
    void *buffer_;
    size_t capacity;
    size_t size;
    size_t head; /* Write pointer*/
    size_t tail; /*Read pointer*/
};

struct logfs
{

    struct device *device;
    uint64_t next_write_offset;
    struct circular_buffer *write_cache;
    struct circular_buffer *read_cache;
    pthread_t consumerThread;
    pthread_mutex_t mutex;
    pthread_cond_t space_avail;
    pthread_cond_t item_avail;
    int done;
};

struct circular_buffer *circular_buffer_init(size_t len, size_t block)
{
    size_t capacity = len * block;
    struct circular_buffer *buffer = (struct circular_buffer *)malloc(sizeof(struct circular_buffer));
    if (buffer == NULL)
    {
        TRACE("out of memory");
        return NULL;
    }
    memset(buffer, 0, sizeof(struct circular_buffer));

    buffer->buffer_ = malloc(capacity + 2 * block);
    memset(buffer->buffer_, 0, capacity + 2 * block);

    if (buffer->buffer_ == NULL)
    {
        TRACE("out of memory");
        free(buffer);
        return NULL;
    }
    buffer->buffer = memory_align(buffer->buffer_, block);

    buffer->capacity = capacity;
    buffer->size = 0;
    buffer->head = 0;
    buffer->tail = 0;

    return buffer;
}

int circular_buffer_write(struct logfs *logfs, const void *data, size_t len)
{

    size_t to_copy;
    size_t end_space;
    struct circular_buffer *buffer = logfs->write_cache;

    pthread_mutex_lock(&logfs->mutex);
    while (buffer->size >= buffer->capacity)
    {
        pthread_cond_wait(&logfs->space_avail, &logfs->mutex);
    }


    to_copy = len;
    end_space = buffer->capacity - buffer->head;

    if (end_space > len)
    {

        memcpy((char *)buffer->buffer + buffer->head, data, len);
    }
    else
    {
        memcpy((char *)buffer->buffer + buffer->head, data, end_space);
        memcpy((char *)buffer->buffer, (char *)data + end_space, len - end_space);
    }


    buffer->head = (buffer->head + to_copy) % buffer->capacity;
    buffer->size += to_copy;
    logfs->write_cache = buffer;

    pthread_cond_signal(&logfs->item_avail);
    pthread_mutex_unlock(&logfs->mutex);

    return 0;
}

int circular_buffer_read(struct logfs *logfs, size_t len)
{

    struct circular_buffer *buffer = logfs->write_cache;
    uint64_t offset = logfs->next_write_offset;

    if(device_write(logfs->device, (char *)buffer->buffer + buffer->tail, offset, len)){
            TRACE("can't write to file");
            return -1;
    }

    

    logfs->write_cache->tail = (logfs->write_cache->tail + len) % logfs->write_cache->capacity;
    logfs->write_cache->size -= len;
    logfs->next_write_offset = logfs->next_write_offset + len;

    pthread_cond_signal(&logfs->space_avail);
    pthread_mutex_unlock(&logfs->mutex);

    return 0;
}

int circular_buffer_flush(struct logfs *logfs, size_t len)
{

    struct circular_buffer *buffer = logfs->write_cache;
    uint64_t offset = logfs->next_write_offset;
    

    if(device_write(logfs->device, (char *)buffer->buffer + buffer->tail, offset, len)){

            TRACE("can't write to file");
            return -1;
        }
    
    logfs->read_cache->head = offset+logfs->write_cache->size;

    return 0;
}

void *consumer(void *arg)
{
    struct logfs *logfs = arg;
    size_t len = device_block(logfs->device);

    while (logfs->done == 0)

    {
        pthread_mutex_lock(&logfs->mutex);

        if (logfs->write_cache->size < len)
        {
            pthread_mutex_unlock(&logfs->mutex);
        }

        else
        {

            if (0 != circular_buffer_read(logfs, len))
            {
                TRACE("can't write");
                pthread_exit(NULL);
            }
        }
    }

    if (logfs->done == 1)
    {

        circular_buffer_flush(logfs, len);
        pthread_exit(NULL);
    }
    pthread_exit(NULL);
}

struct logfs *logfs_open(const char *pathname)
{
    struct logfs *logfs = (struct logfs *)malloc(sizeof(struct logfs));
    if (!logfs)
    {
        TRACE("Memory allocation failed");
        return NULL;
    }

    memset(logfs, 0, sizeof(struct logfs));

    logfs->device = device_open(pathname);
    if (!logfs->device)
    {

        TRACE("open device failed");
        FREE(logfs);
    }

    logfs->write_cache = circular_buffer_init(WCACHE_BLOCKS, device_block(logfs->device));

    if (!logfs->write_cache)
    {
        TRACE("buffer Initialization failed");
        FREE(logfs);
        return NULL;
    }
    logfs->read_cache = circular_buffer_init(RCACHE_BLOCKS, device_block(logfs->device));

    if (!logfs->read_cache)
    {
        TRACE("buffer Initialization failed");
        FREE(logfs->read_cache);
        FREE(logfs);
        return NULL;
    }

    logfs->next_write_offset = 0;
    pthread_mutex_init(&(logfs->mutex), NULL);

    logfs->done = 0;
    pthread_cond_init(&(logfs->space_avail), NULL);
    pthread_cond_init(&(logfs->item_avail), NULL);

    if (pthread_create(&logfs->consumerThread, NULL, consumer, logfs) != 0)
    {
        TRACE("Consumer thread creation failed");
        logfs_close(logfs);
        return NULL;
    }

    return logfs;
}

void logfs_close(struct logfs *logfs)
{

    pthread_mutex_lock(&logfs->mutex);
    logfs->done = 1;
    pthread_mutex_unlock(&logfs->mutex);

    pthread_cond_signal(&logfs->item_avail);

    pthread_join(logfs->consumerThread, NULL);
    FREE(logfs->write_cache->buffer_);
    FREE(logfs->read_cache->buffer_);
    FREE(logfs->write_cache);
    FREE(logfs->read_cache);

    device_close(logfs->device);

    pthread_mutex_destroy(&(logfs->mutex));
    pthread_cond_destroy(&(logfs->space_avail));
    pthread_cond_destroy(&(logfs->item_avail));



    FREE(logfs);
}

int logfs_read(struct logfs *logfs, void *buf, uint64_t off, size_t len)
{

    uint64_t block_size = device_block(logfs->device);
    uint64_t off_read = off / block_size * block_size;
    uint64_t buf_new = off - off_read;
    size_t read_len = len / block_size * block_size + 2 * block_size;

    if((off>=logfs->read_cache->tail)&(off+len<=MIN(logfs->read_cache->head,logfs->read_cache->tail+logfs->read_cache->size))){


        buf_new = off-logfs->read_cache->tail;

        memcpy(buf, (char *)logfs->read_cache->buffer + buf_new, len);
        return 0;

    }
    else{

    pthread_mutex_lock(&logfs->mutex);
    while (logfs->write_cache->size > block_size)
    {


        pthread_cond_wait(&logfs->space_avail, &logfs->mutex);
    }
    if (0 != circular_buffer_flush(logfs, block_size))
    {
        TRACE("can't write");
        return -1;
    }
    if (-1 == device_read(logfs->device, logfs->read_cache->buffer, off_read, read_len))
    {
        TRACE("can't read");
        return -1;
    }
    logfs->read_cache->tail = off_read;
    logfs->read_cache->size = read_len;
    memcpy(buf, (char *)logfs->read_cache->buffer + buf_new, len);

    pthread_mutex_unlock(&logfs->mutex);
    

    return 0;
}}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len)
{
    if(!len){
        return 0;}

    if (circular_buffer_write(logfs, buf, len))
    {
        TRACE("fail to append");
        return -1;
    }
    return 0;
}
