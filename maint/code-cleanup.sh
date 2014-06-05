#! /bin/bash

if test ! -z "`which gindent`" ; then
        indent=gindent
else
        indent=indent
fi

indent_code()
{
    file=$1

    $indent --k-and-r-style --line-length80 --else-endif-column1 --start-left-side-of-comments \
	--break-after-boolean-operator --dont-cuddle-else --dont-format-comments \
	--comment-indentation1 --indent-level4 --no-tabs --no-space-after-casts \
    -T ABT_stream -T ABT_stream_state -T ABT_thread -T ABT_thread_state \
    -T ABT_task -T ABT_task_state -T ABT_mutex -T ABT_condition \
    -T ABT_scheduler -T ABT_unit_type -T ABT_unit -T ABT_pool \
    -T ABT_scheduler_funcs \
    -T ABTI_stream -T ABTI_stream_type -T ABTI_thread -T ABTI_thread_type \
    -T ABTI_task -T ABTI_mutex -T ABTI_condition -T ABTI_scheduler \
    -T ABTI_scheduler_type -T ABTI_unit -T ABTI_pool \
    -T ABTI_stream_pool -T ABTI_task_pool -T ABTI_global -T ABTI_local \
	${file}
    rm -f ${file}~
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
