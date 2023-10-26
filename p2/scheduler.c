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

int scheduler_create(scheduler_fnc_t fnc, void *arg)
{
    thread *new_thread = (thread *)malloc(sizeof(thread));
    size_t page;
    if (new_thread == NULL)
    {
        return -1;
    }

    new_thread->status = STATUS_;
    new_thread->arg = arg;
    new_thread->fnc = fnc;
    new_thread->next = NULL;
    page = page_size();

    new_thread->stack.memory_ = malloc(page * 2);
    new_thread->stack.memory = memory_align(new_thread->stack.memory_, page);

    if (state.head_thread == NULL)
    {
        state.head_thread = new_thread;
        new_thread->next = state.head_thread;
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

static void destroy()
{
    thread *head_thread = state.head_thread;
    thread *destory_thread = head_thread->next;
    while (destory_thread != head_thread)
    {
        if (destory_thread->stack.memory_)
        {
            free(destory_thread->stack.memory_);
        }
        if (destory_thread)
        {
            free(destory_thread);
        }
        destory_thread = destory_thread->next;
    }

    if (head_thread->stack.memory_)
    {
        free(head_thread->stack.memory_);
    }
    if (head_thread)
    {
        free(head_thread);
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
    thread *next_thread = thread_candidate();
    if (next_thread == NULL)
    {
        return;
    }
    state.current_thread = next_thread;

    if (state.current_thread->status == STATUS_)
    {
        uint64_t *rsp = (uint64_t *)state.current_thread->stack.memory;
        __asm__ volatile("mov %[rs], %%rsp \n"
                         : [rs] "+r"(rsp)::);

        state.current_thread->status = STATUS_RUNNING;
        state.current_thread->fnc(state.current_thread->arg);
        state.current_thread->status = STATUS_TERMINATED;
        /*_thread_ process end, it would be back to main process, it would crash, so back to longjump back to main process*/
        longjmp(state.ctx, 2);
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
        destroy();
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
