# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	hello_world \
	hello_world_thread \
	fibonacci_future \
	fibonacci_task \
	fibonacci_thread_task \
	sched_predef \
	sched_shared_pool \
	sched_stack \
	sched_user \
	sched_and_pool_user \
	stencil_task \
	stencil_thread \
	stencil_thread_cond

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

hello_world_SOURCES = hello_world.c
hello_world_thread_SOURCES = hello_world_thread.c
fibonacci_future_SOURCES = fibonacci_future.c
fibonacci_task_SOURCES = fibonacci_task.c
fibonacci_thread_task_SOURCES = fibonacci_thread_task.c
sched_predef_SOURCES = sched_predef.c
sched_shared_pool_SOURCES = sched_shared_pool.c
sched_stack_SOURCES = sched_stack.c
sched_user_SOURCES = sched_user.c
sched_and_pool_user_SOURCES = sched_and_pool_user.c
stencil_task_SOURCES = stencil_task.c
stencil_thread_SOURCES = stencil_thread.c
stencil_thread_cond_SOURCES = stencil_thread_cond.c

testing:
	./hello_world
	./hello_world_thread
	./fibonacci_future
	./fibonacci_task
	./fibonacci_thread_task
	./sched_predef
	./sched_shared_pool
	./sched_stack
	./sched_user
	./sched_and_pool_user
	./stencil_task
	./stencil_thread
	./stencil_thread_cond
