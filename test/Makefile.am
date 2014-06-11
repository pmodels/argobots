AM_CPPFLAGS = $(DEPS_CPPFLAGS)
AM_CPPFLAGS += -I../src
AM_LDFLAGS = $(DEPS_LDFLAGS)

LDADD = ../libabt.la
noinst_PROGRAMS = thread_create thread_create2 thread_yield thread_yield_to task_create thread_task name scheduler mutex future_create

thread_create_SOURCES = thread_create.c
thread_create2_SOURCES = thread_create2.c
thread_yield_SOURCES = thread_yield.c
thread_yield_to_SOURCES = thread_yield_to.c
task_create_SOURCES = task_create.c
thread_task_SOURCES = thread_task.c
name_SOURCES = name.c
scheduler_SOURCES = scheduler.c
mutex_SOURCES = mutex.c
future_create_SOURCES = future_create.c

testing:
	./thread_create
	./thread_create2
	./thread_yield
	./thread_yield_to
	./task_create
	./thread_task
	./name
	./scheduler
	./mutex
	./future_create
