
#include "futex_testing_common.h"
#include "logging.h"

#include <pthread.h>
#include <string.h>

int create_rt_thread(pthread_t *pth, void *(*func)(void *), void *arg,
		int policy, int prio)
{
	int ret;
	struct sched_param schedp;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	memset(&schedp, 0, sizeof(schedp));

	ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	if (ret) {
		error("pthread_attr_setinheritsched\n", ret);
		return -1;
	}

	ret = pthread_attr_setschedpolicy(&attr, policy);
	if (ret) {
		error("pthread_attr_setschedpolicy\n", ret);
		return -1;
	}

	schedp.sched_priority = prio;
	ret = pthread_attr_setschedparam(&attr, &schedp);
	if (ret) {
		error("pthread_attr_setschedparam\n", ret);
		return -1;
	}

	ret = pthread_create(pth, &attr, func, arg);
	if (ret) {
		error("pthread_create\n", ret);
		return -1;
	}

	return 0;
}

