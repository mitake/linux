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
 *      logging.h
 *
 * DESCRIPTION
 *      Glibc independent futex library for testing kernel functionality.
 *
 * AUTHOR
 *      Darren Hart <dvhltc@us.ibm.com>
 *
 * HISTORY
 *      2009-Nov-6: Initial version by Darren Hart <dvhltc@us.ibm.com>
 *
 *****************************************************************************/

#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/futex.h>

/* Verbosity setting for INFO messages */
#define VQUIET    0
#define VCRITICAL 1
#define VINFO     2
#define VMAX      VINFO

/* Functional test return codes */
#define RET_PASS   0
#define RET_ERROR -1
#define RET_FAIL  -2

void log_color(int use_color);
void log_verbosity(int level);
void print_result(int ret);

extern int _verbose;

extern const char *INFO;
extern const char *PASS;
extern const char *ERROR;
extern const char *FAIL;

/* log level macros */
#define info(message, vargs...)						\
	do {								\
		if (_verbose >= VINFO)					\
			fprintf(stderr, "\t%s: "message, INFO, ##vargs); \
	} while (0)

#define error(message, err, args...)					\
	do {								\
		if (_verbose >= VCRITICAL) {				\
			if (err)					\
				fprintf(stderr, "\t%s: %s: "message,	\
					ERROR, strerror(err), ##args);	\
			else						\
				fprintf(stderr, "\t%s: "message,	\
					ERROR, ##args);			\
		}							\
	} while (0)

#define fail(message, args...)						\
	do {								\
		if (_verbose >= VCRITICAL)				\
			fprintf(stderr, "\t%s: "message, FAIL, ##args); \
	} while (0)

#endif
