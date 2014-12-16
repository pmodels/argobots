# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

abt_sources = \
	stream.c \
	thread.c \
	thread_attr.c \
	task.c \
	mutex.c \
	cond.c \
	futures.c \
	eventual.c \
	unit.c \
	pool.c \
	global.c \
	local.c \
	sched/sched.c \
	sched/fifo.c \
	sched/lifo.c \
	sched/prio.c \
	error.c

include $(top_srcdir)/src/arch/Makefile.mk
include $(top_srcdir)/src/include/Makefile.mk

lib_LTLIBRARIES = libabt.la
libabt_la_SOURCES = $(abt_sources)
libabt_la_CPPFLAGS = -I$(top_srcdir)/src/include -pthread
libabt_la_CCASFLAGS = -I$(top_srcdir)/src/include
libabt_la_LDFLAGS = -version-info @libabt_so_version@ -lpthread

