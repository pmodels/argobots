AM_CPPFLAGS = $(DEPS_CPPFLAGS)
AM_CPPFLAGS += -I../src
AM_LDFLAGS = $(DEPS_LDFLAGS)

LDADD = ../libabt.la
noinst_PROGRAMS =    		\
	xstream_create    		\
	thread_create    		\
	thread_create2   		\
	thread_attr      		\
	thread_yield     		\
	thread_yield_to  		\
	thread_migrate_to  		\
	thread_migrate  		\
	task_create      		\
	thread_task      		\
	name             		\
	scheduler_basic  		\
	scheduler_prio   		\
	scheduler_user   		\
	mutex            		\
	future_create    		\
	fibonacci_future 		\
	fibonacci_task   		\
	fibonacci_thread_task

xstream_create_SOURCES = xstream_create.c
thread_create_SOURCES = thread_create.c
thread_create2_SOURCES = thread_create2.c
thread_attr_SOURCES = thread_attr.c
thread_yield_SOURCES = thread_yield.c
thread_yield_to_SOURCES = thread_yield_to.c
thread_migrate_to_SOURCES = thread_migrate_to.c
thread_migrate_SOURCES = thread_migrate.c
task_create_SOURCES = task_create.c
thread_task_SOURCES = thread_task.c
name_SOURCES = name.c
scheduler_basic_SOURCES = scheduler_basic.c
scheduler_prio_SOURCES = scheduler_prio.c
scheduler_user_SOURCES = scheduler_user.c
mutex_SOURCES = mutex.c
future_create_SOURCES = future_create.c
fibonacci_future_SOURCES = fibonacci_future.c
fibonacci_task_SOURCES = fibonacci_task.c
fibonacci_thread_task_SOURCES = fibonacci_thread_task.c

testing:
	./xstream_create
	./thread_create
	./thread_create2
	./thread_attr
	./thread_yield
	./thread_yield_to
	./thread_migrate_to
	./thread_migrate
	./task_create
	./thread_task
	./name
	./scheduler_basic
	./scheduler_prio
	./scheduler_user
	./mutex
	./future_create
	./fibonacci_future
	./fibonacci_task
	./fibonacci_thread_task
