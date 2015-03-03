# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	xstream_create \
	thread_create \
	thread_create2 \
	thread_create_on_xstream \
	thread_attr \
	thread_yield \
	thread_yield_to \
	thread_migrate \
	thread_id \
	task_create \
	thread_task \
	thread_task_num \
	sched_stack \
	pool_access \
	name \
	mutex \
	condition \
	future_create \
	eventual_create \
	self_type \
	timer

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

xstream_create_SOURCES = xstream_create.c
thread_create_SOURCES = thread_create.c
thread_create2_SOURCES = thread_create2.c
thread_create_on_xstream_SOURCES = thread_create_on_xstream.c
thread_attr_SOURCES = thread_attr.c
thread_yield_SOURCES = thread_yield.c
thread_yield_to_SOURCES = thread_yield_to.c
thread_migrate_SOURCES = thread_migrate.c
thread_id_SOURCES = thread_id.c
task_create_SOURCES = task_create.c
thread_task_SOURCES = thread_task.c
thread_task_num_SOURCES = thread_task_num.c
sched_stack_SOURCES = sched_stack.c
pool_access_SOURCES = pool_access.c
name_SOURCES = name.c
mutex_SOURCES = mutex.c
condition_SOURCES = condition.c
future_create_SOURCES = future_create.c
eventual_create_SOURCES = eventual_create.c
self_type_SOURCES = self_type.c
timer_SOURCES = timer.c

testing:
	./xstream_create
	./thread_create
	./thread_create2
	./thread_create_on_xstream
	./thread_attr
	./thread_yield
	./thread_yield_to
	./thread_migrate
	./thread_id
	./task_create
	./thread_task
	./thread_task_num
	./sched_stack
	./pool_access
	./name
	./mutex
	./condition
	./future_create
	./eventual_create
	./self_type
	./timer
