#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include "system.h"
#include "scheduler.h"

struct Thread {
    jmp_buf context;
    enum thread_state {
        STATUS_RUNNING,
        STATUS_TERMINATED,
        STATUS_SLEEPING
    } state;
    struct {
        void *memory_;
        void *memory;
    } stack;
    struct Thread *next;
};

struct Thread *head_thread = NULL;
struct Thread *current_thread = NULL;

int num_threads = 0;

int scheduler_create(scheduler_fnc_t fnc, void *arg) {
    struct Thread *new_thread = (struct Thread *)malloc(sizeof(struct Thread));
    size_t page;
    if (new_thread == NULL) {
        return -1;
    }
    num_threads++;
    new_thread->state = STATUS_SLEEPING;
    page = page_size();

    new_thread->stack.memory_ = malloc(sizeof(scheduler_fnc_t));
    new_thread->stack.memory = memory_align(new_thread->stack.memory_, page);
    *(scheduler_fnc_t *)(new_thread->stack.memory) = fnc;
    *((void **)((char *)new_thread->stack.memory + sizeof(scheduler_fnc_t))) = arg;

    if (head_thread == NULL) {
        head_thread = new_thread;
        new_thread->next = head_thread;
        current_thread = head_thread;
        printf("first ");
    } else {
        new_thread->next = head_thread;
        current_thread->next = new_thread;
        current_thread = new_thread;
        printf("notfirst ");
    }
    return 0;
}

static void destroy(struct Thread *thread) {
    free(thread->stack.memory_);
    free(thread->stack.memory);
    free(thread);
}

static struct Thread *thread_candidate() {
    struct Thread* next_thread = current_thread->next;
    
    while (next_thread != NULL) {
        if (next_thread->state != STATUS_TERMINATED) {
            return next_thread;
        } 
        next_thread = next_thread->next;
    }
    current_thread ->next = head_thread;
    return NULL; 
}

void scheduler_execute() {
    while (1) {
        struct Thread* next_thread = thread_candidate();
        printf("scheduler_execute ");
       if (current_thread == NULL) {
            break;
        }
     
        current_thread = next_thread;
        if (setjmp(current_thread->context) == 0) {
            scheduler_fnc_t function = *(scheduler_fnc_t *)(current_thread->stack.memory);
            void *function_arg = *((void **)((char *)current_thread->stack.memory + sizeof(scheduler_fnc_t)));
            current_thread->state=STATUS_RUNNING;
            printf("fnc ");
            function(function_arg);
            current_thread->state = STATUS_TERMINATED;
            printf("Thread terminated");
            destroy(current_thread);
        }
    }
    printf("All threads terminated\n");
    return;
}

void scheduler_yield() {
    printf("yield ");

    /*current_thread->state = STATUS_SLEEPING;*/
    /*printf("state%u ",current_thread->next->state);*/
    longjmp(current_thread->context, 1);
    if(setjmp(current_thread->context)==0){
        current_thread->state = STATUS_SLEEPING;
        longjmp(current_thread->context, 1);
    }
}
