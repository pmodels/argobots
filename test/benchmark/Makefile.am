# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	init_finalize \
	xstream_ops \
	thread_ops \
	thread_ops_all \
	thread_many_ops \
	task_ops \
	task_ops_all \
	sync_ops

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

init_finalize_SOURCES = init_finalize.c
xstream_ops_SOURCES = xstream_ops.c
thread_ops_SOURCES = thread_ops.c
thread_ops_all_SOURCES = thread_ops_all.c
thread_many_ops_SOURCES = thread_many_ops.c
task_ops_SOURCES = task_ops.c
task_ops_all_SOURCES = task_ops_all.c
sync_ops_SOURCES = sync_ops.c

testing:
	./init_finalize -i 100
	./xstream_ops -e 4 -i 100
	./thread_ops -e 4 -u 10 -i 100
	./thread_ops_all -e 4 -u 10 -i 100
	./thread_many_ops -e 4 -u 10 -i 100
	./task_ops -e 4 -t 10 -i 100
	./task_ops_all -e 4 -t 10 -i 100
	./sync_ops -e 4 -u 10 -i 100
