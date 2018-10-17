#include "ut.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>

#define QUANTOM 1
#define INTERVAL_TIMER_MILLISECONDS 100
#define MILLISECOND_TO_MICRO 1000
#define ARGS_COUNT 1
#define SS_FLAGS 0
#define INITIAL_VTIME 0
#define GENEREAL_ERROR_MESSAGE "Error"
#define SIG_HNDL_EXC_RET_VAL 1
#define FIRST_CELL_INDEX 0
#define SUCCESS_RET_CODE 0
#define INITIAL_THREAD_NUMBER 0

// Threads' table
static ut_slot* threads_table = NULL;
static unsigned int threads_counter = 0;
static int threads_table_size = 0;

// variables to control the context switching
static ucontext_t mainThread;
static volatile tid_t currThreadNum = 0;

int ut_init(int tab_size) {
    // Validating the input for table size. Setting the size to the maximum if it is out of valid bounds
    if (tab_size < MIN_TAB_SIZE || tab_size > MAX_TAB_SIZE) {
        printf("Info: the table size is not in the valide range. Setting table size to be %d\n", MAX_TAB_SIZE);
        tab_size = MAX_TAB_SIZE;
    }

    // Allocating space for the ut_slot array
    threads_table_size = tab_size;
    threads_table = (ut_slot*)calloc(tab_size, sizeof(ut_slot));

    if (NULL == threads_table) {
        fprintf(stderr, "Error: Failed to allocate thread's table");
        return SYS_ERR;
    }

    return SUCCESS_RET_CODE;
}

tid_t ut_spawn_thread(void (*func)(int), int arg) {
    char * new_thread_stack; // The stack of the new ut_slot
    tid_t current_thread_number = threads_counter; // The thread id of the new ut_slot

    if (NULL == func) {
        fprintf(stderr, "Error - Bad input: function parameter for the thread is not valid");
        return SYS_ERR;
    }

    // ut_spawn_thread was called before ut_init
    if (NULL == threads_table) {
        fprintf(stderr, "Error: Thread's table is not initialized. Make sure you have called \"ut_init()\" prior to spawining your threads");
        return SYS_ERR;
    }

    // Checking if there is a room for a new thread to spawn
    if (threads_counter >= threads_table_size) {
        printf("Error: No more room for new threads\n");
        return TAB_FULL;
    }

    // Allocating thread for the new thread
    new_thread_stack = (char*)calloc(STACKSIZE, sizeof(char));

    // Allocating new ut_slot
    threads_table[threads_counter] = (ut_slot)malloc(sizeof(ut_slot_t));

    if (NULL == threads_table[threads_counter]) {
        fprintf(stderr, "Error: Failed to allocate new thread");
        return SYS_ERR;
    }

    // Getting a new context for the new thread
    if (getcontext(&(threads_table[threads_counter]->uc)) == -1) {
        perror(GENEREAL_ERROR_MESSAGE);
        exit(SIG_HNDL_EXC_RET_VAL);
    }

    // Setting up the new_ucontext's data
    threads_table[threads_counter]->uc.uc_stack.ss_sp = new_thread_stack;
    threads_table[threads_counter]->uc.uc_stack.ss_size = STACKSIZE * sizeof(char);
    threads_table[threads_counter]->uc.uc_stack.ss_flags = SS_FLAGS;
    threads_table[threads_counter]->uc.uc_link = &mainThread;

    // Setting up the new ut_slot
    threads_table[threads_counter]->stack = new_thread_stack;
    threads_table[threads_counter]->vtime = INITIAL_VTIME;
    threads_table[threads_counter]->func = func;
    threads_table[threads_counter]->arg = arg;
    (void)makecontext(&(threads_table[threads_counter]->uc),
        (void(*)(void))threads_table[threads_counter]->func, ARGS_COUNT,
        threads_table[threads_counter]->arg);

    // Informing about spawning a new thread
    printf("Info: new thread table record in (tid: %d)\n", threads_counter);
    threads_counter++;

    // Return the tid
    return current_thread_number;
}

/* This is the signal handler which swaps between the threads and profiles the time of the running threads*/
void handler(int signal) {
	switch (signal){
        // If Sigalrm received
        case SIGALRM:

            // Restart the alarm signal
            alarm(QUANTOM);

            // Getting the next tid in line
            currThreadNum = ((currThreadNum + 1) % threads_counter);

            // Swap context to the follwing thread on the array (circular)
            if (SYS_ERR == swapcontext(&(threads_table[currThreadNum]->uc), &(threads_table[(currThreadNum + 1) % threads_counter]->uc))) {
                perror(GENEREAL_ERROR_MESSAGE);
                exit(SIG_HNDL_EXC_RET_VAL);
            }
        break;

        case SIGVTALRM:
            // Adding to the total time of thread running
            threads_table[currThreadNum]->vtime += INTERVAL_TIMER_MILLISECONDS;
        break;
	}
}

int ut_start(void) {
    // Defining the signal and interval variables
    struct sigaction sa;
    struct itimerval itv;

    // Setting up the signal action struct
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);
    sa.sa_handler = handler;

    // Setting up the timer interval struct
    itv.it_interval.tv_sec = INITIAL_VTIME;
    itv.it_interval.tv_usec = INTERVAL_TIMER_MILLISECONDS * MILLISECOND_TO_MICRO;
    itv.it_value = itv.it_interval;

    // Install the signal handler for the signals and intervals
    if (sigaction(SIGALRM, &sa, NULL) < 0)
		return SYS_ERR;

    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
		return SYS_ERR;

	if (setitimer(ITIMER_VIRTUAL, &itv, NULL) < 0)
		return SYS_ERR;

    // Starting to switch threads
    alarm(QUANTOM);
    currThreadNum = INITIAL_THREAD_NUMBER;
    threads_table[FIRST_CELL_INDEX]->vtime += INTERVAL_TIMER_MILLISECONDS;

	if (SYS_ERR == swapcontext(&mainThread, &(threads_table[FIRST_CELL_INDEX]->uc))) {
        perror(GENEREAL_ERROR_MESSAGE);
        exit(SIG_HNDL_EXC_RET_VAL);
    }

	// Should never return
    return SUCCESS_RET_CODE;
}

unsigned long ut_get_vtime(tid_t tid) {

    // Validating the received tid
    if (tid > threads_counter) {
        printf("Thread does not exist!\n");
        return SYS_ERR;
    }

    // Returning the total time that the thread run
    return threads_table[tid]->vtime;
}
