# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

abt_sources += src/stream.c \
	src/thread.c \
	src/sched.c \
	src/pool.c \
	src/unit.c \
	src/abtd.c \
	src/abtu.c \
	src/task.c \
	src/global.c

include_HEADERS = src/abt.h
