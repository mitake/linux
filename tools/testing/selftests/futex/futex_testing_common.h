
#ifndef FUTEX_TESTING_COMMON_H
#define FUTEX_TESTING_COMMON_H

#include <pthread.h>

int create_rt_thread(pthread_t *pth, void *(*func)(void *), void *arg, int policy, int prio);

#endif	/* FUTEX_TESTING_COMMON_H */
