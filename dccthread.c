#include <stdlib.h>
#include <assert.h>
#include <ucontext.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <malloc.h>

#include "dccthread.h"
#include "dlist.h"

#define TIMER_INTERVAL_SEC 0
#define TIMER_INTERVAL_NSEC 10000000

sigset_t mask;
sigset_t sleepmask;

typedef struct dccthread{
    char name[DCCTHREAD_MAX_NAME_SIZE];
    ucontext_t *context;
    dccthread_t *waiting_for; //pointer to a thread that it is waiting to finish, NULL otherwise
} dccthread_t;

struct dlist *sleeping_list;
struct dlist *ready_list;
//putting manager as a global thread so it can be used during the whole process/
ucontext_t manager;

void print_all_threads() {
    printf("threads: %d, sleeping_threads: %d\n", ready_list->count, sleeping_list->count);
    for (int i = 0; i < ready_list->count; i++) {
        dccthread_t* thread = dlist_get_index(ready_list, i);
        printf("thread %s\n", thread->name);
    }
    printf("__\n");
    for (int i = 0; i < sleeping_list->count; i++) {
        dccthread_t* thread = dlist_get_index(sleeping_list, i);
        printf("sleeping thread %s\n", thread->name);
    }
}

// initializing both manager and main threads/
void dccthread_init(void (func)(int), int param){
    //printf("oi\n");
    sleeping_list = dlist_create();
    ready_list = dlist_create();
    dccthread_create("main", func, param);
    getcontext(&manager);

    timer_t timerp;
    struct sigaction sa1;
    struct sigevent se;
    struct itimerspec ts;

    se.sigev_signo = SIGRTMIN;
    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_notify_attributes = NULL;
    se.sigev_value.sival_ptr = &timerp;
    sa1.sa_flags = 0;
    sa1.sa_handler = (void *)dccthread_yield;

    ts.it_interval.tv_sec = TIMER_INTERVAL_SEC;
    ts.it_interval.tv_nsec = TIMER_INTERVAL_NSEC;
    ts.it_value.tv_sec = TIMER_INTERVAL_SEC;
    ts.it_value.tv_nsec = TIMER_INTERVAL_NSEC;

    sigaction(SIGRTMIN, &sa1, NULL);

    timer_create(CLOCK_PROCESS_CPUTIME_ID, &se, &timerp);
    timer_settime(timerp, 0, &ts, NULL);

    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);

    sigprocmask(SIG_BLOCK, &mask, NULL);
    while(!dlist_empty(ready_list) || !dlist_empty(sleeping_list)){
        sigprocmask(SIG_UNBLOCK, &sleepmask, NULL);
        sigprocmask(SIG_BLOCK, &sleepmask, NULL);
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
        //print_all_threads();
    }
    exit(0);
}


//function responsible to create the threads and putting them in the ready_list/
dccthread_t * dccthread_create(const char *name, void (*func)(int ), int param){
    dccthread_t* new_thread = malloc(sizeof(dccthread_t));
    new_thread->context = malloc(sizeof(ucontext_t));
    getcontext(new_thread->context);

    sigprocmask(SIG_BLOCK, &mask, NULL);

    strcpy(new_thread->name, name);
    new_thread->waiting_for=NULL;
    new_thread->context->uc_link = &manager;
    new_thread->context->uc_stack.ss_flags = 0;
    new_thread->context->uc_stack.ss_size = THREAD_STACK_SIZE;
    new_thread->context->uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);

    dlist_push_right(ready_list, new_thread);
    makecontext(new_thread->context, (void*) func, 1, param);
    
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    return new_thread;
}

void dccthread_yield(void){
    sigprocmask(SIG_BLOCK, &mask, NULL);
    dccthread_t* current_thread = dccthread_self();
    printf("\n yield %s", current_thread->name);
    dlist_push_right(ready_list, current_thread);
    swapcontext((current_thread->context),&manager);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

const char * dccthread_name(dccthread_t *tid){
    return tid->name;
}

dccthread_t *dccthread_self(void){
    return dlist_get_index(ready_list, 0); 
}

void dccthread_exit(void){
    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_t* current_thread = dccthread_self();
    printf("\n%s\n", current_thread->name);
    for(int i = 0; i < ready_list->count; i++){
        dccthread_t* listed_thread = dlist_get_index(ready_list, i);
        if(listed_thread->waiting_for == current_thread){
            listed_thread->waiting_for = NULL;
        }
    }
    for(int i = 0; i < sleeping_list->count; i++){
        dccthread_t* listed_thread = dlist_get_index(sleeping_list, i);
        if(listed_thread->waiting_for == current_thread){
            listed_thread->waiting_for = NULL;
        }
    }
    
    free(current_thread);
    setcontext(&manager);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}


void dccthread_wait(dccthread_t *tid){

    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_t* current_thread = dccthread_self();
    int tid_in_readylist = 0;
    //verify if tid is an existing thread, which means, if it is in the ready list
    for(int i = 0; i < ready_list->count; i++){
        dccthread_t* listed_thread = dlist_get_index(ready_list, i);
        if(listed_thread == tid){
            tid_in_readylist++;
            break;
        }
    }
    for(int i = 0; i < sleeping_list->count; i++){
        dccthread_t* listed_thread = dlist_get_index(sleeping_list, i);
        if(listed_thread == tid){
            tid_in_readylist++;
            break;
        }
    }
    //if so, we classify the current thread as waiting for tid, and change the context back to the manager
    if (tid_in_readylist){
        //printf("waiting");
        current_thread->waiting_for = tid;
        swapcontext(current_thread->context, &manager);
    } 
    return;

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

// void resume_thread_execution(int sig, siginfo_t *si, void *uc) {
//     dccthread_t* thread_to_resume = (dccthread_t *)si->si_value.sival_ptr;
//     dlist_push_right(ready_list, thread_to_resume);
// }

int cmp(const void *e1, const void *e2, void *userdata){
	return (dccthread_t *)e1 != (dccthread_t *)e2;
}

// Função para ser chamada quando o timer de um thread dormindo expirar
//int sig, siginfo_t *si, void *uc
void dccthread_wakeup(int signo, siginfo_t *si, void *uc) {
    printf("OI");

    dccthread_t* sleeping1 = (dccthread_t *)si->si_value.sival_ptr;

    //dlist_find_remove(sleeping_list, sleeping1, cmp, NULL);
    
    //dlist_get_index(sleeping_list, 0);
    //(dccthread_t *)si->si_value.sival_ptr;
    //si->si_value.sival_ptr->sleeping = 0;
    dlist_pop_left(sleeping_list);
    dlist_push_right(ready_list, sleeping1);

}

void dccthread_sleep(struct timespec ts){

    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_t* current_thread = dccthread_self();
    //current_thread->sleeping = 1;

    timer_t timerp;
    struct sigaction s1;
    struct sigevent sev;
    struct itimerspec its;

    //dlist_push_right(ready_list, current_thread);

    
    s1.sa_flags = SA_SIGINFO;
    s1.sa_sigaction  = dccthread_wakeup;
    s1.sa_mask = mask;
    sigaction(SIGRTMAX, &s1, NULL);

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMAX;
    sev.sigev_value.sival_ptr = current_thread;
    timer_create(CLOCK_REALTIME, &sev, &timerp);

    its.it_value = ts;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    timer_settime(timerp, 0, &its, NULL);
    //dlist_push_right(ready_list, current_thread);
    //dlist_pop_left(ready_list);
    dlist_push_right(sleeping_list, current_thread);   

 	swapcontext((current_thread->context), &manager);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}