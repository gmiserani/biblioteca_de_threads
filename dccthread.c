#include <stdlib.h>
#include <assert.h>
#include <ucontext.h>
#include <string.h>

#include "dccthread.h"
#include "dlist.h"


typedef struct dccthread{
    char name[DCCTHREAD_MAX_NAME_SIZE];
    ucontext_t *context;
    unsigned int yielded; //0 if not, 1 otherwise
    unsigned int sleeping; //0 if not, 1 otherwise
    dccthread_t *waiting_for; //pointer to a thread that it is waiting to finish, NULL otherwise
} dccthread_t;

struct dlist *ready_list;
//putting manager as a global thread so it can be used during the whole process/
ucontext_t manager;

// initializing both manager and main threads/
void dccthread_init(void (func)(int), int param){
    ready_list = dlist_create();
    dccthread_t* main_thread = dccthread_create("main", func, param);
    getcontext(&manager);


    while(!dlist_empty(ready_list)){
        dccthread_t* current_thread = (dccthread_t *) dlist_get_index(ready_list, 0);
        if(current_thread->waiting_for != NULL){
            dlist_pop_left(ready_list);
            dlist_push_right(ready_list, current_thread);
        }
        swapcontext(&manager,(current_thread->context));
        dlist_pop_left(ready_list);
        
        if(current_thread->waiting_for != NULL){
            dlist_push_right(ready_list, current_thread);
        }
    }
    exit(0);
}


//function responsible to create the threads and putting them in the ready_list/
dccthread_t * dccthread_create(const char *name, void (*func)(int ), int param){
    dccthread_t* new_thread = malloc(sizeof(dccthread_t));
    strcpy(new_thread->name, name);
    new_thread->waiting_for=NULL;
    new_thread->context = malloc(sizeof(ucontext_t));
    getcontext(new_thread->context);
    dlist_push_right(ready_list, new_thread);
    new_thread->context->uc_link = &manager;
    new_thread->context->uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);
    new_thread->context->uc_stack.ss_size = THREAD_STACK_SIZE;
    new_thread->context->uc_stack.ss_flags = 0;
    makecontext(new_thread->context, (void*) func, 1, param);
    return new_thread;
}

void dccthread_yield(void){
    dccthread_t* current_thread = dccthread_self();
    dlist_push_right(ready_list, current_thread);
    swapcontext((current_thread->context),&manager);
}

const char * dccthread_name(dccthread_t *tid){
    return tid->name;
}

dccthread_t *dccthread_self(void){
    dccthread_t *nome = dlist_get_index(ready_list, 0);
    return nome;
}

void dccthread_exit(void){
    dccthread_t* current_thread = dccthread_self();

    for(int i = 0; i < ready_list->count; i++){
        dccthread_t* listed_thread = dlist_get_index(ready_list, i);
        if(listed_thread->waiting_for == current_thread){
            listed_thread->waiting_for = NULL;
        }
    }
    free(current_thread);
    setcontext(&manager);
}


void dccthread_wait(dccthread_t *tid){
    dccthread_t* current_thread = dccthread_self();
    int tid_in_readylist = 0;
    //verify if tid is an existing thread, which means, if it is in the ready list
    for(int i = 0; i < ready_list->count && tid_in_readylist==0; i++){
        dccthread_t* listed_thread = dlist_get_index(ready_list, i);
        if(listed_thread == tid){
            tid_in_readylist++;
            break;
        }
    }
    //if so, we classify the current thread as waiting for tid, and change the context back to the manager
    if (tid_in_readylist){
        current_thread->waiting_for = tid;
        swapcontext(current_thread->context, &manager);
    } 
}