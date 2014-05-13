AM_CPPFLAGS = $(DEPS_CPPFLAGS)
AM_CPPFLAGS += -I../src
AM_LDFLAGS = $(DEPS_LDFLAGS)

LDADD = ../libabt.la
noinst_PROGRAMS = thread_create thread_yield thread_yield_to name task_create thread_task scheduler

thread_create_SOURCES = thread_create.c
thread_yield_SOURCES = thread_yield.c
thread_yield_to_SOURCES = thread_yield_to.c
name_SOURCES = name.c
task_create_SOURCES = task_create.c
thread_task_SOURCES = thread_task.c
scheduler_SOURCES = scheduler.c

testing:
	./thread_create
	./thread_yield
	./thread_yield_to
	./name
	./task_create
	./thread_task
	./scheduler
