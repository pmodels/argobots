# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	fibonacci_future \
	fibonacci_task \
	fibonacci_thread_task

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

fibonacci_future_SOURCES = fibonacci_future.c
fibonacci_task_SOURCES = fibonacci_task.c
fibonacci_thread_task_SOURCES = fibonacci_thread_task.c

testing:
	./fibonacci_future
	./fibonacci_task
	./fibonacci_thread_task
