/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scheduler.c
 */
#undef _FORTIFY_SOURCE

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include "system.h"
#include "scheduler.h"

typedef struct Thread
{
    jmp_buf ctx;
    enum thread_state
    {
        STATUS_,
        STATUS_RUNNING,
        STATUS_TERMINATED,
        STATUS_SLEEPING
    } status;
    struct
    {
        void *memory_;
        void *memory;
    } stack;
    scheduler_fnc_t fnc;
    void *arg;
    struct Thread *next;
} thread;

static struct
{
    thread *head_thread;
    thread *current_thread;
    jmp_buf ctx;
} state;

int stack_size = 0;
int num_threads = 0;
int terminate_counter = 0;

int scheduler_create(scheduler_fnc_t fnc, void *arg)
{
    thread *new_thread = (thread *)malloc(sizeof(thread));
    size_t page;
    if (new_thread == NULL)
    {
        return -1;
    }
    num_threads++;
    new_thread->status = STATUS_;
    new_thread->arg = arg;
    new_thread->fnc = fnc;
    page = page_size();
    stack_size = page * 4;

    new_thread->stack.memory_ = malloc(stack_size + page);
    new_thread->stack.memory = memory_align(new_thread->stack.memory_, page);

    if (state.head_thread == NULL)
    {
        state.head_thread = new_thread;
        state.head_thread->next = state.head_thread;
        state.current_thread = state.head_thread;
    }
    else
    {
        new_thread->next = state.head_thread;
        state.current_thread->next = new_thread;
        state.current_thread = new_thread;
    }
    return 0;
}

static void destroy_all()
{
    thread *destory_thread = state.current_thread;
    thread *next_thread;
    while (num_threads > 0)
    {
        next_thread = destory_thread->next;
        if (destory_thread->stack.memory_)
        {
            free(destory_thread->stack.memory_);
        }
        if (destory_thread)
        {
            free(destory_thread);
        }
        destory_thread = next_thread;
        num_threads--;
    }

    state.head_thread = NULL;
    state.current_thread = NULL;
}

static thread *thread_candidate()
{
    thread *next_thread;
    thread *cursor;

    if (!state.current_thread)
    {
        return state.head_thread;
    }

    next_thread = state.current_thread->next;
    cursor = state.current_thread;

    while (next_thread != cursor)
    {
        if (next_thread->status == STATUS_ || next_thread->status == STATUS_SLEEPING)
        {
            return next_thread;
        }
        next_thread = next_thread->next;
    }
    return NULL;
}

static void schedule()
{
    uint64_t rsp;

    thread *next_thread = thread_candidate();

    state.current_thread = next_thread;

    if (state.current_thread->status == STATUS_)
    {

        rsp = (uint64_t)state.current_thread->stack.memory + (uint64_t)stack_size;

        __asm__ volatile("mov %[rs], %%rsp \n"
                         : [rs] "+r"(rsp)::);

        state.current_thread->status = STATUS_RUNNING;
        state.current_thread->fnc(state.current_thread->arg);
        state.current_thread->status = STATUS_TERMINATED;
        longjmp(state.ctx, 2); /*finished,jump back to schedule_execute()*/
    }
    else
    {
        longjmp(state.current_thread->ctx, 1);
    }
}

void scheduler_execute()
{
    int jumpFlag = setjmp(state.ctx);

    if (jumpFlag == 0 || jumpFlag == 1)
    {
        schedule();
    }
    else if (jumpFlag == 2)
    {
        terminate_counter++;

        if (terminate_counter < num_threads)
        {
            state.current_thread = thread_candidate();
            longjmp(state.current_thread->ctx, 1);
        }
        else if(terminate_counter==num_threads)
        {
            destroy_all();
            return;
        }
    }
}

void scheduler_yield()
{
    if (setjmp(state.current_thread->ctx) == 0)
    {
        state.current_thread->status = STATUS_SLEEPING;
        longjmp(state.ctx, 1);
    }
}
