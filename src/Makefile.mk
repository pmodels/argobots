# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

abt_sources += src/stream.c \
	src/thread.c \
	src/thread_attr.c \
	src/task.c \
	src/mutex.c \
	src/futures.c \
	src/condition.c \
	src/unit.c \
	src/pool.c \
	src/global.c \
	src/local.c \
	src/abtd.c \
	src/abtu.c \
	src/sched/sched.c \
	src/sched/fifo.c \
	src/sched/lifo.c \
	src/sched/prio.c

include_HEADERS = src/abt.h
