# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	hello_world \
	fibonacci_future

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

hello_world_SOURCES = hello_world.c
fibonacci_future_SOURCES = fibonacci_future.c

testing:
	./hello_world
	./fibonacci_future
