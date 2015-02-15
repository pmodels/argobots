# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	xstream_create \
	thread_create \
	thread_create2 \
	thread_attr \
	thread_yield \
	thread_yield_to \
	thread_migrate_to \
	thread_migrate \
	thread_id \
	task_create \
	thread_task \
	thread_task_num \
	name \
	scheduler_test \
	scheduler_basic \
	scheduler_prio \
	scheduler_user \
	mutex \
	condition \
	future_create \
	eventual_create \
	self_type

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

xstream_create_SOURCES = xstream_create.c
thread_create_SOURCES = thread_create.c
thread_create2_SOURCES = thread_create2.c
thread_attr_SOURCES = thread_attr.c
thread_yield_SOURCES = thread_yield.c
thread_yield_to_SOURCES = thread_yield_to.c
thread_migrate_to_SOURCES = thread_migrate_to.c
thread_migrate_SOURCES = thread_migrate.c
thread_id_SOURCES = thread_id.c
task_create_SOURCES = task_create.c
thread_task_SOURCES = thread_task.c
thread_task_num_SOURCES = thread_task_num.c
name_SOURCES = name.c
scheduler_test_SOURCES = scheduler_test.c
scheduler_basic_SOURCES = scheduler_basic.c
scheduler_prio_SOURCES = scheduler_prio.c
scheduler_user_SOURCES = scheduler_user.c
mutex_SOURCES = mutex.c
condition_SOURCES = condition.c
future_create_SOURCES = future_create.c
eventual_create_SOURCES = eventual_create.c
self_type_SOURCES = self_type.c

testing:
	./xstream_create
	./thread_create
	./thread_create2
	./thread_attr
	./thread_yield
	./thread_yield_to
	./thread_migrate_to
	./thread_migrate
	./thread_id
	./task_create
	./thread_task
	./thread_task_num
	./name
	./scheduler_test
	./scheduler_basic
	./scheduler_prio
	./scheduler_user
	./mutex
	./condition
	./future_create
	./eventual_create
	./self_type
