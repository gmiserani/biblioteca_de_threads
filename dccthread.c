#include <stdlib.h>
#include <assert.h>
#include <ucontext.h>
#include <string.h>

#include "dccthread.h"
#include "dlist.h"


typedef struct dccthread{
    char name[100];
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
        dccthread_t* current_thread = (dccthread_t *) dlist_pop_left(ready_list);
        swapcontext(&manager,(current_thread->context));
    }
}

//function responsible to create the threads and putting them in the ready_list/
dccthread_t * dccthread_create(const char *name, void (*func)(int ), int param){
    dccthread_t* new_thread = (dccthread_t*) malloc(sizeof(dccthread_t));

    strcpy(new_thread->name, name);


    getcontext(new_thread->context);
    new_thread->context->uc_link = &manager;
    dlist_push_right(ready_list, new_thread);
    makecontext(new_thread->context, (void*) func, 1, param);
    //new_thread->context->uc_stack.ss_sp = malloc(sizeof(new_thread->context->uc_stack.ss_sp));
    //new_thread->context->uc_stack.ss_size = sizeof(new_thread->context->uc_stack.ss_sp);
    //new_thread->context->uc_stack.ss_flags = 0;
    return new_thread;
}

void dccthread_yield(void){
    dccthread_t* current_thread = dccthread_self();
    swapcontext(&manager,(current_thread->context));
}

const char * dccthread_name(dccthread_t *tid){
    return tid->name;
}

dccthread_t *dccthread_self(void){
    return ready_list->head->data;;
}