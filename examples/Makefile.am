# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	hello_world \
	fibonacci_future \
	fibonacci_task \
	fibonacci_thread_task

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

hello_world_SOURCES = hello_world.c
fibonacci_future_SOURCES = fibonacci_future.c
fibonacci_task_SOURCES = fibonacci_task.c
fibonacci_thread_task_SOURCES = fibonacci_thread_task.c

testing:
	./hello_world
	./fibonacci_future
	./fibonacci_task
	./fibonacci_thread_task
