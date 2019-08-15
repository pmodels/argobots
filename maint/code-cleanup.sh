#! /bin/bash

if test ! -z "`which gindent`" ; then
        indent=gindent
else
        indent=indent
fi

indent_code()
{
    file=$1

    if [[ "$file" == *"abt.h.in" ]]; then
        return
    fi
    $indent --k-and-r-style --line-length80 --else-endif-column1 \
            --start-left-side-of-comments --break-after-boolean-operator \
            --dont-format-comments --case-indentation4 \
            --comment-indentation1 --indent-level4 --no-tabs \
            --no-space-after-casts --dont-format-comments \
            -T ABT_xstream -T ABT_xstream_state -T ABT_xstream_barrier \
            -T ABT_sched -T ABT_sched_config -T ABT_sched_predef \
            -T ABT_sched_def -T ABT_sched_state -T ABT_sched_type \
            -T ABT_pool -T ABT_pool_config -T ABT_pool_kind \
            -T ABT_pool_access -T ABT_pool_def -T ABT_unit -T ABT_unit_type \
            -T ABT_thread -T ABT_thread_attr -T ABT_thread_state \
            -T ABT_thread_id -T ABT_task -T ABT_task_state -T ABT_key \
            -T ABT_mutex -T ABT_mutex_attr -T ABT_cond -T ABT_rwlock \
            -T ABT_eventual -T ABT_future -T ABT_barrier -T ABT_timer \
            -T ABT_bool -T ABT_event_kind -T ABTI_global -T ABTI_local \
            -T ABTI_contn -T ABTI_elem -T ABTI_xstream -T ABTI_xstream_type \
            -T ABTI_xstream_contn -T ABTI_sched -T ABTI_sched_config \
            -T ABTI_sched_used -T ABTI_sched_id -T ABTI_sched_kind \
            -T ABTI_pool -T ABTI_unit -T ABTI_thread_attr -T ABTI_thread \
            -T ABTI_thread_type -T ABTI_stack_type -T ABTI_thread_req_arg \
            -T ABTI_thread_list -T ABTI_thread_entry -T ABTI_thread_htable \
            -T ABTI_thread_queue -T ABTI_task -T ABTI_key -T ABTI_ktelem \
            -T ABTI_ktable -T ABTI_mutex_attr -T ABTI_mutex -T ABTI_cond \
            -T ABTI_rwlock -T ABTI_eventual -T ABTI_future -T ABTI_barrier \
            -T ABTI_timer -T ABTI_stack_header -T ABTI_page_header \
            -T ABTI_sp_header -T ABTI_event_info -T ABTI_blk_header \
            -T ABTI_valgrind_id_list -T ABTI_log -T ABTI_spinlock \
            -T ABTI_xstream_barrier -T ABTD_time -T ABTD_xstream_context \
            -T ABTD_xstream_mutex -T ABTD_thread_context \
            -T ABTD_xstream_barrier -T ucontext_t -T fcontext_t -T intptr_t \
            -T uintptr_t -T size_t -T rt1_data_t -T pthread_t -T data_t \
            -T unit_t -T sched_data_t -T cpu_set_t -T int8_t -T uint8_t \
            -T int16_t -T uint16_t -T int32_t -T uint32_t -T int64_t \
            -T uint64_t -T arg_t -T barrier_t -T launch_t -T seq_state_t \
            -T thread_arg_t -T task_arg_t -T unit_arg_t -T test_arg_t \
            -T time_t -T FILE -T sched_data -T thread_args -T task_args \
            -T exp_task_args -T agg_task_args -T thread_func_list \
            ${file}
    rm -rf ${file}~
    cp ${file} /tmp/${USER}.__tmp__ && \
	cat ${file} | sed -e 's/ *$//g' -e 's/( */(/g' -e 's/ *)/)/g' \
	-e 's/if(/if (/g' -e 's/while(/while (/g' -e 's/do{/do {/g' -e 's/}while/} while/g' > \
	/tmp/${USER}.__tmp__ && mv /tmp/${USER}.__tmp__ ${file}
}

usage()
{
    echo "Usage: $1 [filename | --all] {--recursive} {--debug}"
}

# Check usage
if [ -z "$1" ]; then
    usage $0
    exit
fi

# Make sure the parameters make sense
all=0
recursive=0
got_file=0
debug=
ignore=0
ignore_list="__I_WILL_NEVER_FIND_YOU__"
for arg in $@; do
    if [ "$ignore" = "1" ] ; then
	ignore_list="$ignore_list|$arg"
	ignore=0
	continue;
    fi

    if [ "$arg" = "--all" ]; then
	all=1
    elif [ "$arg" = "--recursive" ]; then
	recursive=1
    elif [ "$arg" = "--debug" ]; then
	debug="echo"
    elif [ "$arg" = "--ignore" ] ; then
	ignore=1
    else
	got_file=1
    fi
done
if [ "$recursive" = "1" -a "$all" = "0" ]; then
    echo "--recursive cannot be used without --all"
    usage $0
    exit
fi
if [ "$got_file" = "1" -a "$all" = "1" ]; then
    echo "--all cannot be used in conjunction with a specific file"
    usage $0
    exit
fi

if [ "$recursive" = "1" ]; then
    for i in `find . \! -type d | egrep '(\.c$|\.h$|\.c\.in$|\.h\.in$|\.cpp$|\.cpp.in$)' | \
	egrep -v "($ignore_list)"` ; do
	${debug} indent_code $i
    done
elif [ "$all" = "1" ]; then
    for i in `find . -maxdepth 1 \! -type d | egrep '(\.c$|\.h$|\.c\.in$|\.h\.in$|\.cpp$|\.cpp.in$)' | \
	egrep -v "($ignore_list)"` ; do
	${debug} indent_code $i
    done
else
    ${debug} indent_code $@
fi
