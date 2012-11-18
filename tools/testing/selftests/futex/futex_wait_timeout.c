/******************************************************************************
 *
 *   Copyright © International Business Machines  Corp., 2009
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * NAME
 *      futex_wait.c
 *
 * DESCRIPTION
 *      Block on a futex and wait for timeout.
 *
 * AUTHOR
 *      Darren Hart <dvhltc@us.ibm.com>
 *
 * HISTORY
 *      2009-Nov-6: Initial version by Darren Hart <dvhltc@us.ibm.com>
 *      2012-Oct-14:
 *       ported to tools/testing by Hitoshi Mitake <h.mitake@gmail.com>
 *
 *****************************************************************************/

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "futex.h"
#include "logging.h"

static long timeout_ns = 100000;	/* 100us default timeout */

static void usage(char *prog)
{
	printf("Usage: %s\n", prog);
	printf("  -c	Use color\n");
	printf("  -h	Display this help message\n");
	printf("  -t N	Timeout in nanoseconds (default: 100,000)\n");
	printf("  -v L	Verbosity level: %d=QUIET %d=CRITICAL %d=INFO\n",
	       VQUIET, VCRITICAL, VINFO);
}

int main(int argc, char *argv[])
{
	futex_t f1 = FUTEX_INITIALIZER;
	struct timespec to;
	int res, ret = RET_PASS;
	int c;

	while ((c = getopt(argc, argv, "cht:v:")) != -1) {
		switch (c) {
		case 'c':
			log_color(1);
			break;
		case 'h':
			usage(basename(argv[0]));
			exit(0);
		case 't':
			timeout_ns = atoi(optarg);
			break;
		case 'v':
			log_verbosity(atoi(optarg));
			break;
		default:
			usage(basename(argv[0]));
			exit(1);
		}
	}

	printf("%s: Block on a futex and wait for timeout\n",
		basename(argv[0]));
	printf("\tArguments: timeout=%ldns\n", timeout_ns);

	/* initialize timeout */
	to.tv_sec = 0;
	to.tv_nsec = timeout_ns;

	info("Calling futex_wait on f1: %u @ %p\n", f1, &f1);
	res = futex_wait(&f1, f1, &to, FUTEX_PRIVATE_FLAG);
	if (!res || errno != ETIMEDOUT) {
		fail("futex_wait returned %d\n", ret < 0 ? errno : ret);
		ret = RET_FAIL;
	}

	print_result(ret);
	return ret;
}
