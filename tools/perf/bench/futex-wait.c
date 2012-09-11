/*
 * futex-wait.c
 *
 * Measure FUTEX_WAIT operations per second.
 * based on futex_wait.c of futextest by Darren Hart <dvhart@linux.intel.com>
 * and Michel Lespinasse <walken@google.com>
 *
 * ported to perf bench by Hitoshi Mitake <h.mitake@gmail.com>
 *
 * original futextest:
 * http://git.kernel.org/?p=linux/kernel/git/dvhart/futextest.git
 */

#include "../perf.h"
#include "../util.h"
#include "../util/parse-options.h"

#include "../../include/tools/futex.h"
#include "../../include/tools/atomic.h"

#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/times.h>
#include <sys/poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <linux/futex.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <poll.h>

struct thread_barrier {
	futex_t threads;
	futex_t unblock;
};

struct worker_ctx {
	futex_t *futex;
	unsigned int iterations;

	int readyfd, wakefd;

	struct thread_barrier *barrier_before, *barrier_after;
};

static void fdpair(int fds[2])
{
	if (pipe(fds) == 0)
		return;

	die("pipe() failed");
}

static inline void futex_wait_lock(futex_t *futex)
{
	int status = *futex;
	if (status == 0)
		status = futex_cmpxchg(futex, 0, 1);
	while (status != 0) {
		if (status == 1)
			status = futex_cmpxchg(futex, 1, 2);
		if (status != 0) {
			futex_wait(futex, 2, NULL, FUTEX_PRIVATE_FLAG);
			status = *futex;
		}
		if (status == 0)
			status = futex_cmpxchg(futex, 0, 2);
	}
}

static inline void futex_cmpxchg_unlock(futex_t *futex)
{
	int status = *futex;
	if (status == 1)
		status = futex_cmpxchg(futex, 1, 0);
	if (status == 2) {
		futex_cmpxchg(futex, 2, 0);
		futex_wake(futex, 1, FUTEX_PRIVATE_FLAG);
	}
}

/* Called by main thread to initialize barrier */
static void barrier_init(struct thread_barrier *barrier, int threads)
{
	barrier->threads = threads;
	barrier->unblock = 0;
}

/* Called by worker threads to synchronize with main thread */
static int barrier_sync(struct thread_barrier *barrier)
{
	futex_dec(&barrier->threads);
	if (barrier->threads == 0)
		futex_wake(&barrier->threads, 1, FUTEX_PRIVATE_FLAG);
	while (barrier->unblock == 0)
		futex_wait(&barrier->unblock, 0, NULL, FUTEX_PRIVATE_FLAG);
	return barrier->unblock;
}

/* Called by main thread to wait for all workers to reach sync point */
static void barrier_wait(struct thread_barrier *barrier)
{
	int threads;
	while ((threads = barrier->threads) > 0)
		futex_wait(&barrier->threads, threads, NULL,
			   FUTEX_PRIVATE_FLAG);
}

/* Called by main thread to unblock worker threads from their sync point */
static void barrier_unblock(struct thread_barrier *barrier, int value)
{
	barrier->unblock = value;
	futex_wake(&barrier->unblock, INT_MAX, FUTEX_PRIVATE_FLAG);
}

static bool use_futex_for_sync;

static void *worker(void *arg)
{
	char dummy;
	int iterations;
	futex_t *futex;

	struct worker_ctx *ctx = (struct worker_ctx *)arg;
	struct pollfd pollfd = { .fd = ctx->wakefd, .events = POLLIN };

	iterations = ctx->iterations;
	futex = ctx->futex;
	/* currently, we have nothing to prepare */
	if (use_futex_for_sync) {
		barrier_sync(ctx->barrier_before);
	} else {
		if (write(ctx->readyfd, &dummy, 1) != 1)
			die("write() on readyfd failed");

		if (poll(&pollfd, 1, -1) != 1)
			die("poll() failed");
	}

	while (iterations--) {
		futex_wait_lock(futex);
		futex_cmpxchg_unlock(futex);
	}

	if (use_futex_for_sync)
		barrier_sync(ctx->barrier_after);

	return NULL;
}

static int iterations = 100000000;
static int threads = 256;
/* futexes are fairly distributed for threads */
static int futexes = 1;

static const struct option options[] = {
	OPT_INTEGER('i', "iterations", &iterations,
		    "number of locking and unlocking"),
	OPT_INTEGER('t', "threads", &threads,
		    "number of worker threads"),
	OPT_INTEGER('f', "futexes", &futexes,
		"number of futexes, the condition"
		"threads % futexes == 0 must be true"),
	OPT_BOOLEAN('s', "futex-for-sync", &use_futex_for_sync,
		"use futex for sync between main thread and worker threads"),
	OPT_END()
};

static const char * const bench_futex_wait_usage[] = {
	"perf bench futex wait <options>",
	NULL
};

int bench_futex_wait(int argc, const char **argv,
		const char *prefix __maybe_unused)
{
	int i;
	char buf;
	int wakefds[2], readyfds[2];
	pthread_t *pth_tab;
	struct worker_ctx *ctx_tab;
	futex_t *futex_tab;

	struct thread_barrier barrier_before, barrier_after;

	clock_t before, after;
	struct tms tms_before, tms_after;
	int wall, user, system_time;
	double tick;

	argc = parse_options(argc, argv, options,
			     bench_futex_wait_usage, 0);

	if (threads % futexes)
		die("threads %% futexes must be 0");

	if (use_futex_for_sync) {
		barrier_init(&barrier_before, threads);
		barrier_init(&barrier_after, threads);
	} else {
		fdpair(wakefds);
		fdpair(readyfds);
	}

	pth_tab = calloc(sizeof(pthread_t), threads);
	if (!pth_tab)
		die("calloc() for pthread descriptors failed");
	ctx_tab = calloc(sizeof(struct worker_ctx), threads);
	if (!ctx_tab)
		die("calloc() for worker contexts failed");
	futex_tab = calloc(sizeof(futex_t), futexes);
	if (!futex_tab)
		die("calloc() for futexes failed");

	for (i = 0; i < threads; i++) {
		ctx_tab[i].futex = &futex_tab[i % futexes];
		ctx_tab[i].iterations = iterations / threads;

		ctx_tab[i].readyfd = readyfds[1];
		ctx_tab[i].wakefd = wakefds[0];

		if (use_futex_for_sync) {
			ctx_tab[i].barrier_before = &barrier_before;
			ctx_tab[i].barrier_after = &barrier_after;
		}

		if (pthread_create(&pth_tab[i], NULL, worker, &ctx_tab[i]))
			die("pthread_create() for creating workers failed");
	}

	if (use_futex_for_sync) {
		barrier_wait(&barrier_before);
	} else {
		for (i = 0; i < threads; i++) {
			if (read(readyfds[0], &buf, 1) != 1)
				die("read() for ready failed");
		}
	}

	before = times(&tms_before);

	if (use_futex_for_sync) {
		barrier_unblock(&barrier_before, 1);
	} else {
		if (write(wakefds[1], &buf, 1) != 1)
			die("write() for waking up workers failed");
	}

	if (use_futex_for_sync) {
		barrier_wait(&barrier_after);
	} else {
		for (i = 0; i < threads; i++)
			pthread_join(pth_tab[i], NULL);
	}

	after = times(&tms_after);

	wall = after - before;
	user = tms_after.tms_utime - tms_before.tms_utime;
	system_time = tms_after.tms_stime - tms_before.tms_stime;
	tick = 1.0 / sysconf(_SC_CLK_TCK);

	switch (bench_format) {
	case BENCH_FORMAT_DEFAULT:
		printf("# %d threads and %d futexes (%d threads for 1 futex)\n",
			threads, futexes, threads / futexes);
		printf("%.2fs user, %.2fs system, %.2fs wall, %.2f cores\n",
			user * tick, system_time * tick, wall * tick,
			wall ? (user + system_time) * 1. / wall : 1.);
		printf("Result: %.0f Kiter/s\n",
			iterations / (wall * tick * 1000));
		break;
	case BENCH_FORMAT_SIMPLE:
		printf("%.0f Kiter/s\n",
			iterations / (wall * tick * 1000));
		break;
	default:
		/* reaching here is something disaster */
		die("Unknown format:%d\n", bench_format);
		break;
	}

	free((void *)pth_tab);
	free((void *)ctx_tab);
	free((void *)futex_tab);

	return 0;
}
