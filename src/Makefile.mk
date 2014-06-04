# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

abt_sources += src/stream.c \
	src/thread.c \
	src/task.c \
	src/mutex.c \
	src/futures.c \
	src/condition.c \
	src/sched.c \
	src/unit.c \
	src/pool.c \
	src/global.c \
	src/local.c \
	src/abtd.c \
	src/abtu.c

include_HEADERS = src/abt.h
