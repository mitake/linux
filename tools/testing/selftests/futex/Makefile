HEADER_DIR := ../../../include/tools
CFLAGS := $(CFLAGS) -g -O2 -Wall -D_GNU_SOURCE -I$(HEADER_DIR)
LDFLAGS := $(LDFLAGS) -lpthread -lrt

HEADERS := $(HEADER_DIR)/futex.h $(HEADER_DIR)/atomic.h \
	logging.h futex_testing_common.h

TARGETS := \
	futex_wait_timeout \
	futex_wait_wouldblock \
	futex_requeue_pi \
	futex_requeue_pi_signal_restart \
	futex_requeue_pi_mismatched_ops \
	futex_wait_uninitialized_heap \
	futex_wait_private_mapped_file

.PHONY: all clean run_test
all: $(TARGETS)

$(TARGETS): $(HEADERS) futex_testing_common.o logging.o

futex_testing_common.o: futex_testing_common.c futex_testing_common.h
	gcc -c futex_testing_common.c

logging.o: logging.c logging.h
	gcc -c logging.c

run_tests:
	./run.sh

clean:
	rm -f $(TARGETS) *.o
