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
    -T ABT_Stream -T ABT_Stream_state -T ABT_Thread -T ABT_Thread_state \
    -T ABT_Task -T ABT_Task_state -T ABT_Mutex -T ABT_Condition \
    -T ABT_Scheduler -T ABT_Unit_type -T ABT_Unit -T ABT_Pool \
    -T ABT_Scheduler_funcs \
    -T ABTI_Stream -T ABTI_Stream_type -T ABTI_Thread -T ABTI_Thread_type \
    -T ABTI_Task -T ABTI_Mutex -T ABTI_Condition -T ABTI_Scheduler \
    -T ABTI_Scheduler_type -T ABTI_Unit -T ABTI_Pool \
    -T ABTI_Stream_pool -T ABTI_Task_pool -T ABTI_Global -T ABTI_Local \
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
