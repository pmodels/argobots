# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	init_finalize \
	xstream_create \
	xstream_affinity \
	xstream_barrier \
	thread_create \
	thread_create2 \
	thread_create_on_xstream \
	thread_revive \
	thread_attr \
	thread_yield \
	thread_yield_to \
	thread_self_suspend_resume \
	thread_migrate \
	thread_data \
	thread_id \
	task_create \
	task_create_on_xstream \
	task_revive \
	task_data \
	thread_task \
	thread_task_arg \
	thread_task_num \
	sched_basic \
	sched_prio \
	sched_randws \
	sched_set_main \
	sched_stack \
	sched_config \
	sched_user_ws \
	pool_access \
	mutex \
	mutex_prio \
	mutex_recursive \
	mutex_spinlock \
	mutex_unlock_se \
	cond_test \
	cond_join \
	cond_signal_in_main \
	cond_timedwait \
	rwlock_writer_excl \
	rwlock_reader_writer_excl \
	rwlock_reader_incl \
	future_create \
	eventual_create \
	eventual_test \
	barrier \
	self_type \
	ext_thread \
	timer \
	info_print

XFAIL_TESTS =
if ABT_CONFIG_DISABLE_POOL_ACCESS_CHECK
XFAIL_TESTS += pool_access
endif
if ABT_CONFIG_DISABLE_EXT_THREAD
XFAIL_TESTS += self_type ext_thread
endif

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

init_finalize_SOURCES = init_finalize.c
xstream_create_SOURCES = xstream_create.c
xstream_affinity_SOURCES = xstream_affinity.c
xstream_barrier_SOURCES = xstream_barrier.c
thread_create_SOURCES = thread_create.c
thread_create2_SOURCES = thread_create2.c
thread_create_on_xstream_SOURCES = thread_create_on_xstream.c
thread_revive_SOURCES = thread_revive.c
thread_attr_SOURCES = thread_attr.c
thread_yield_SOURCES = thread_yield.c
thread_yield_to_SOURCES = thread_yield_to.c
thread_self_suspend_resume_SOURCES = thread_self_suspend_resume.c
thread_migrate_SOURCES = thread_migrate.c
thread_data_SOURCES = thread_data.c
thread_id_SOURCES = thread_id.c
task_create_SOURCES = task_create.c
task_create_on_xstream_SOURCES = task_create_on_xstream.c
task_revive_SOURCES = task_revive.c
task_data_SOURCES = task_data.c
thread_task_SOURCES = thread_task.c
thread_task_arg_SOURCES = thread_task_arg.c
thread_task_num_SOURCES = thread_task_num.c
sched_basic_SOURCES = sched_basic.c
sched_prio_SOURCES = sched_prio.c
sched_randws_SOURCES = sched_randws.c
sched_set_main_SOURCES = sched_set_main.c
sched_stack_SOURCES = sched_stack.c
sched_config_SOURCES = sched_config.c
sched_user_ws_SOURCES = sched_user_ws.c
pool_access_SOURCES = pool_access.c
mutex_SOURCES = mutex.c
mutex_prio_SOURCES = mutex_prio.c
mutex_recursive_SOURCES = mutex_recursive.c
mutex_spinlock_SOURCES = mutex_spinlock.c
mutex_unlock_se_SOURCES = mutex_unlock_se.c
cond_test_SOURCES = cond_test.c
cond_join_SOURCES = cond_join.c
cond_signal_in_main_SOURCES = cond_signal_in_main.c
cond_timedwait_SOURCES = cond_timedwait.c
rwlock_writer_excl_SOURCES = rwlock_writer_excl.c
rwlock_reader_writer_excl_SOURCES = rwlock_reader_writer_excl.c
rwlock_reader_incl_SOURCES = rwlock_reader_incl.c
future_create_SOURCES = future_create.c
eventual_create_SOURCES = eventual_create.c
eventual_test_SOURCES = eventual_test.c
barrier_SOURCES = barrier.c
self_type_SOURCES = self_type.c
ext_thread_SOURCES = ext_thread.c
timer_SOURCES = timer.c
info_print_SOURCES = info_print.c

testing:
	./init_finalize
	./xstream_create
	./xstream_affinity
	./xstream_barrier
	./thread_create
	./thread_create2
	./thread_create_on_xstream
	./thread_revive
	./thread_attr
	./thread_yield
	./thread_yield_to
	./thread_self_suspend_resume
	./thread_migrate
	./thread_data
	./thread_id
	./task_create
	./task_create_on_xstream
	./task_revive
	./task_data
	./thread_task
	./thread_task_arg
	./thread_task_num
	./sched_basic
	./sched_prio
	./sched_randws
	./sched_set_main
	./sched_stack
	./sched_config
	./sched_user_ws
	./pool_access
	./mutex
	./mutex_prio
	./mutex_recursive
	./mutex_spinlock
	./mutex_unlock_se
	./cond_test
	./cond_join
	./cond_signal_in_main
	./cond_timedwait
	./rwlock_writer_excl
	./rwlock_reader_writer_excl
	./rwlock_reader_incl
	./future_create
	./eventual_create
	./eventual_test
	./barrier
	./self_type
	./ext_thread
	./timer
	./info_print
