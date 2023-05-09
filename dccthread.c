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

    //context of the thread given by the ucontext library
    ucontext_t *context;

    //pointer to a thread that it is waiting to finish, NULL if it is not waiting for no one
    dccthread_t *waiting_for; 
} dccthread_t;

//declaring both ready and waiting list
struct dlist *ready_list;
struct dlist *sleeping_list;

//putting manager as a global thread so it can be used during the whole process
ucontext_t manager;

timer_t timerp;
timer_t sleep_timer;
struct sigaction s_action;
struct sigaction sleep_action;
struct sigevent s_event;
struct sigevent sleep_event;
struct itimerspec t_spec;
struct itimerspec sleep_spec;

//initializing the preemption time
void times(){
    s_event.sigev_signo = SIGRTMIN;
    s_event.sigev_notify = SIGEV_SIGNAL;
    s_event.sigev_notify_attributes = NULL;
    s_event.sigev_value.sival_ptr = &timerp;
    s_action.sa_flags = 0;
    s_action.sa_handler = (void *) dccthread_yield;

    t_spec.it_interval.tv_sec = TIMER_INTERVAL_SEC;
    t_spec.it_interval.tv_nsec = TIMER_INTERVAL_NSEC;
    t_spec.it_value.tv_sec = TIMER_INTERVAL_SEC;
    t_spec.it_value.tv_nsec = TIMER_INTERVAL_NSEC;

    sigaction(SIGRTMIN, &s_action, NULL);
    timer_create(CLOCK_PROCESS_CPUTIME_ID, &s_event, &timerp);
    timer_settime(timerp, 0, &t_spec, NULL);
}

// initializing both manager and main threads/
void dccthread_init(void (*func)(int), int param){

    //creating the lists
    sleeping_list = dlist_create();
    ready_list = dlist_create();

    dccthread_create("main", func, param);
    getcontext(&manager);

    //mask for signals
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    sigemptyset(&sleepmask);
    sigaddset(&sleepmask, SIGRTMAX);

    sigprocmask(SIG_BLOCK, &mask, NULL);

    //initializing the preemption time
    times();

    //starting the process of each thread as long as there is any in either of the lists
    while(!dlist_empty(ready_list) || !dlist_empty(sleeping_list)){
        sigprocmask(SIG_UNBLOCK, &sleepmask, NULL);
        sigprocmask(SIG_BLOCK, &sleepmask, NULL);

        dccthread_t* current_thread = dlist_get_index(ready_list, 0);

        //verify if the thread is waiting for any other. If so, we put it on the end of the line
        if(current_thread->waiting_for != NULL){
            dlist_pop_left(ready_list);
            dlist_push_right(ready_list, current_thread);
            continue;
        }

        swapcontext(&manager,(current_thread->context));
        dlist_pop_left(ready_list);
        
        if(current_thread->waiting_for != NULL){
            dlist_push_right(ready_list, current_thread);
        }

    }

    timer_delete(timerp);

	dlist_destroy(sleeping_list, NULL);
	dlist_destroy(ready_list, NULL);

    exit(0);
}


//function responsible to create the threads and putting them in the ready_list
dccthread_t * dccthread_create(const char *name, void (*func)(int ), int param){
    dccthread_t* new_thread = malloc(sizeof(dccthread_t));
    new_thread->context = malloc(sizeof(ucontext_t));
    getcontext(new_thread->context);

    sigprocmask(SIG_BLOCK, &mask, NULL);

    new_thread->waiting_for=NULL;
    strcpy(new_thread->name, name);
    new_thread->context->uc_link = &manager;
    new_thread->context->uc_stack.ss_flags = 0;
    new_thread->context->uc_stack.ss_size = THREAD_STACK_SIZE;
    new_thread->context->uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);

    //put the new inicialized thread in the ready list
    dlist_push_right(ready_list, new_thread);
    makecontext(new_thread->context, (void*) func, 1, param);
    
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    return new_thread;
}

//yield the CPU (from the current thread to another).
void dccthread_yield(void){
    sigprocmask(SIG_BLOCK, &mask, NULL);

    //get the thread that is currently in the cpu
    dccthread_t* current_thread = dccthread_self();
    dlist_push_right(ready_list, current_thread);
    swapcontext((current_thread->context),&manager);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

//gets the name of a thread
const char * dccthread_name(dccthread_t *tid){
    return tid->name;
}

//gets the thread that is currently at the cpu
dccthread_t *dccthread_self(void){
    return dlist_get_index(ready_list, 0); 
}

//terminates the thread that is currently at the cpu
void dccthread_exit(void){
    sigprocmask(SIG_BLOCK, &mask, NULL);

    //gets the thread that is about to be freed
    dccthread_t* current_thread = dccthread_self();

    //verify if there is any thread either in the ready or sleep list that is waiting for this thread
    for(int i = 0; i < sleeping_list->count; i++){
        dccthread_t* listed_thread = dlist_get_index(sleeping_list, i);
        if(listed_thread->waiting_for == current_thread){
            listed_thread->waiting_for = NULL;
        }
    }
    for(int i = 0; i < ready_list->count; i++){
        dccthread_t* listed_thread = dlist_get_index(ready_list, i);
        if(listed_thread->waiting_for == current_thread){
            //set them as not waiting anymore
            listed_thread->waiting_for = NULL;
        }
    }
    
    //free the thread
    free(current_thread);
    setcontext(&manager);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

//block the current thread until tid exits
void dccthread_wait(dccthread_t *tid){
    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_t* current_thread = dccthread_self();

    int tid_in_readylist = 0;
    //verify if tid is an existing thread, which means, if it is in the ready or sleep list
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
        current_thread->waiting_for = tid;
        swapcontext(current_thread->context, &manager);
    } 

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

// wakes up the thread that is in the sleeping list
void dccthread_wakeup(int signo, siginfo_t *si, void *uc) {
    dlist_pop_left(sleeping_list);
    dlist_push_right(ready_list, si->si_value.sival_ptr);
}

void sleep_times(dccthread_t* current_thread, struct timespec ts){
    sleep_action.sa_flags = SA_SIGINFO;
    sleep_action.sa_sigaction = dccthread_wakeup;
    sleep_action.sa_mask = mask;
    sigaction(SIGRTMAX, &sleep_action, NULL);

    sleep_event.sigev_notify = SIGEV_SIGNAL;
    sleep_event.sigev_signo = SIGRTMAX;
    sleep_event.sigev_value.sival_ptr = current_thread;
    sleep_spec.it_value = ts;
    sleep_spec.it_interval.tv_sec = 0;
    sleep_spec.it_interval.tv_nsec = 0;

    timer_create(CLOCK_REALTIME, &sleep_event, &sleep_timer);
    timer_settime(sleep_timer, 0, &sleep_spec, NULL);
}

//stops the thread for ts time period
void dccthread_sleep(struct timespec ts){
    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_t* current_thread = dccthread_self();

    //initialize the sleeptime
    sleep_times(current_thread, ts);

    //puts the current thread at the sleeping list
    dlist_push_right(sleeping_list, current_thread);   
 	swapcontext((current_thread->context), &manager);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}