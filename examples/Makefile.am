# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS = $(DEPS_CPPFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/src/include
AM_LDFLAGS = $(DEPS_LDFLAGS)

libabt = $(top_srcdir)/src/libabt.la

$(libabt):
	$(MAKE) -C $(top_srcdir)/src

LDADD = $(libabt)

noinst_PROGRAMS =           \
	fibonacci_future        \
	fibonacci_task          \
	fibonacci_thread_task

fibonacci_future_SOURCES = fibonacci_future.c
fibonacci_task_SOURCES = fibonacci_task.c
fibonacci_thread_task_SOURCES = fibonacci_thread_task.c

testing:
	./fibonacci_future
	./fibonacci_task
	./fibonacci_thread_task
