# -*- Mode: Autoconf; -*-
#
# See COPYRIGHT in top-level directory.
#

AC_PREREQ(2.63)

m4_include([maint/version.m4])
AC_INIT([argobots],ABT_VERSION_m4)

ABT_VERSION=ABT_VERSION_m4
AC_SUBST([ABT_VERSION])

libabt_so_version="libabt_so_version_m4"
AC_SUBST([libabt_so_version])

# Produce a numeric version assuming the following format:
# Version: [MAJ].[MIN].[REV][EXT][EXT_NUMBER]
# Example: 1.0.7rc1 has
#          MAJ = 1
#          MIN = 0
#          REV = 7
#          EXT = rc
#          EXT_NUMBER = 1
#
# Converting to numeric version will convert EXT to a format number:
#          ALPHA (a) = 0
#          BETA (b)  = 1
#          RC (rc)   = 2
#          PATCH (p) = 3
# Regular releases are treated as patch 0
#
# Numeric version will have 1 digit for MAJ, 2 digits for MIN,
# 2 digits for REV, 1 digit for EXT and 2 digits for EXT_NUMBER.
changequote(<<,>>)
V1=`expr $ABT_VERSION : '\([0-9]*\)\.[0-9]*\.*[0-9]*[a-zA-Z]*[0-9]*'`
V2=`expr $ABT_VERSION : '[0-9]*\.\([0-9]*\)\.*[0-9]*[a-zA-Z]*[0-9]*'`
V3=`expr $ABT_VERSION : '[0-9]*\.[0-9]*\.*\([0-9]*\)[a-zA-Z]*[0-9]*'`
V4=`expr $ABT_VERSION : '[0-9]*\.[0-9]*\.*[0-9]*\([a-zA-Z]*\)[0-9]*'`
V5=`expr $ABT_VERSION : '[0-9]*\.[0-9]*\.*[0-9]*[a-zA-Z]*\([0-9]*\)'`
changequote([,])

if test "$V2" -le 9 ; then V2="0$V2" ; fi
if test "$V3" = "" ; then V3="0"; fi
if test "$V3" -le 9 ; then V3="0$V3" ; fi
if test "$V4" = "a" ; then
    V4=0
elif test "$V4" = "b" ; then
    V4=1
elif test "$V4" = "rc" ; then
    V4=2
elif test "$V4" = "" ; then
    V4=3
    V5=0
elif test "$V4" = "p" ; then
    V4=3
fi
if test "$V5" -le 9 ; then V5="0$V5" ; fi

ABT_NUMVERSION=`expr $V1$V2$V3$V4$V5 + 0`
AC_SUBST(ABT_NUMVERSION)

AC_CONFIG_AUX_DIR(m4)
AC_CONFIG_MACRO_DIR(m4)


dnl ----------------------------------------------------------------------------
dnl empty CFLAGS, CXXFLAGS, CCASFLAGS
dnl ----------------------------------------------------------------------------
: ${CFLAGS=""}
: ${CXXFLAGS=""}
: ${CCASFLAGS=""}
dnl ----------------------------------------------------------------------------

dnl ----------------------------------------------------------------------------
dnl check the compiler and standard headers
dnl ----------------------------------------------------------------------------
PAC_PROG_CC
AC_HEADER_STDC
dnl ----------------------------------------------------------------------------

dnl ----------------------------------------------------------------------------
dnl configure options
dnl ----------------------------------------------------------------------------
# --enable-visibility
PAC_CHECK_VISIBILITY
AC_SUBST(ABT_VISIBILITY_CFLAGS)

# --enable-affinity
AC_ARG_ENABLE([affinity],
    AS_HELP_STRING([--enable-affinity],
        [enable ES affinity]),,
        [enable_affinity=no])

# --enable-debug
AC_ARG_ENABLE([debug],
[  --enable-debug@<:@=OPTS@:>@   control the level of debugging. "OPTS" is a list of
                          comma separated names below. Default is "yes".
        yes   - add compiler flags, -g -Wall
        log   - enable debug event logging
        all   - all of the above choices
        most  - same as "all" but not print logs by default
                (set ABT_ENV_USE_LOG to 1 for logging)
        none  - no debugging, i.e., --disable-debug
],,[enable_debug=none])

# --enable-fast
AC_ARG_ENABLE(fast,
[  --enable-fast@<:@=OPTS@:>@    control the level of fast execution in the Argobots
                          implementation. "OPTS" is a list of comma separated
                          names including
        O<n>     - append default optimization flags, -O<n>, to CFLAGS
                   (default is -O2)
        ndebug   - append -DNDEBUG to CFLAGS
        all|yes  - "O2" and "ndebug" are enabled
        none     - none of above options, i.e., --disable-fast
],,enable_fast=O2)

# --enable-tls-model
AC_ARG_ENABLE(tls-model,
[  --enable-tls-model@<:@=OPT@:>@  change the thread-local storage model used by
                          the compiler. This affects the performance of ES's
                          local storage. "OPT" can be one of values below.
                          Please refer to the compiler documentation for
                          details.
        global-dynamic, local-dynamic, initial-exec, local-exec
        yes      - same as "initial-exec"
        none|no  - use the compiler's default model
],,enable_tls_model=none)

# --enable-perf-opt
AC_ARG_ENABLE([perf-opt],
    AS_HELP_STRING([--enable-perf-opt], [enable performance optimization]))

# --enable-valgrind
AC_ARG_ENABLE([valgrind],
    AS_HELP_STRING([--enable-valgrind], [enable valgrind support]))

# --enable-checks
AC_ARG_ENABLE([checks],
[  --enable-checks@<:@=OPTS@:>@  control the amount of sanity checking.
                          "OPTS" is a list of comma separated names including
        all               - checking always enabled (default)
        no-error          - disable error checks
        no-pool-producer  - disable pool producer checks
        no-pool-consumer  - disable pool consumer checks
        none|no           - no checking
],,[enable_checks=all])

# --disable-fcontext
AC_ARG_ENABLE([fcontext],
    AS_HELP_STRING([--disable-fcontext],
        [do not use fcontext even though it is supported. If you disable
         fcontext, ucontext in libc is used.]),,
        [enable_fcontext=yes])

# --disable-preserve-fpu
AC_ARG_ENABLE([preserve-fpu],
    AS_HELP_STRING([--disable-preserve-fpu],
        [do not preserve fpu registers when fcontext is used]),,
        [enable_preserve_fpu=yes])

# --disable-mem-pool
AC_ARG_ENABLE([mem-pool],
    AS_HELP_STRING([--disable-mem-pool],
        [do not use memory pools for ULT and tasklet creation]),,
        [enable_mem_pool=yes])

# --disable-aligned-alloc
AC_ARG_ENABLE([aligned-alloc],
    AS_HELP_STRING([--disable-aligned-alloc],
        [do not use aligned memory allocation]),,
        [enable_aligned_alloc=yes])

# --enable-feature
AC_ARG_ENABLE([feature],
[  --enable-feature@<:@=OPTS@:>@  enable/disable features.
                          "OPTS" is a list of comma separated names including
        all                 - all features always enabled (default)
        no-thread-cancel    - disable ULT cancellation
        no-task-cancel      - disable tasklet cancellation
        no-migration        - disable ULT migrationtask
        no-stackable-sched  - disable stackable scheduler
        no-ext-thread       - disable supporting external threads
        none|no             - disable all features above
],,[enable_feature=all])

# --enable-sched-sleep
AC_ARG_ENABLE([sched-sleep],
    AS_HELP_STRING([--enable-sched-sleep], [enable scheduler sleep]))

# --enable-simple-mutex
AC_ARG_ENABLE([simple-mutex],
    AS_HELP_STRING([--enable-simple-mutex], [use a simple mutex implementation]))

# --with-lts
AC_ARG_WITH([lts],
    AS_HELP_STRING([--with-lts=PATH],
        [specify path where lts include directory and lib directory can be found]))

# --with-hugetlbfs
AC_ARG_WITH([hugetlbfs],
    AS_HELP_STRING([--with-hugetlbfs=PATH],
        [specify path where hugetlbfs include directory and lib directory can be found]))

# --enable-power-event
AC_ARG_ENABLE([power-event],
    AS_HELP_STRING([--enable-power-event],
        [enable handling power management events]))

# --enable-publish-info
AC_ARG_ENABLE([publish-info],
    AS_HELP_STRING([--enable-publish-info],
        [enable publishing execution information]))

# --with-beacon
AC_ARG_WITH([beacon],
    AS_HELP_STRING([--with-beacon=PATH],
        [specify path where beacon include directory and lib directory can be found]))

# --with-intercoolr
AC_ARG_WITH([intercoolr],
    AS_HELP_STRING([--with-intercoolr=PATH],
        [specify path where intercoolr include directory and lib directory can be found]))
dnl ----------------------------------------------------------------------------


dnl ----------------------------------------------------------------------------
dnl check the environment and the function availability
dnl ----------------------------------------------------------------------------
# compute canonical system types
AC_CANONICAL_HOST

# check architecture and OS for fcontext
fctx_arch_bin=""
AS_CASE([$host_cpu],
    [i?86],   [AS_CASE([$host_os],
              [linux*],  [fctx_arch_bin="i386_sysv_elf_gas"],
              [darwin*], [fctx_arch_bin="i386_sysv_macho_gas"],
              [freebsd*], [fctx_arch_bin="i386_sysv_elf_gas"])],
    [x86_64], [AS_CASE([$host_os],
              [linux*],  [fctx_arch_bin="x86_64_sysv_elf_gas"],
              [darwin*], [fctx_arch_bin="x86_64_sysv_macho_gas"],
              [freebsd*], [fctx_arch_bin="x86_64_sysv_elf_gas"])],
    [p*pc32], [AS_CASE([$host_os],
              [linux*],  [fctx_arch_bin="ppc32_sysv_elf_gas"],
              [darwin*], [fctx_arch_bin="ppc32_sysv_macho_gas"])],
    [p*pc64], [AS_CASE([$host_os],
              [linux*],  [fctx_arch_bin="ppc64_sysv_elf_gas"],
              [darwin*], [fctx_arch_bin="ppc64_sysv_macho_gas"])],
    [aarch64],[AS_CASE([$host_os],
              [linux*],  [fctx_arch_bin="arm64_aapcs_elf_gas"],
              [darwin*], [fctx_arch_bin="arm64_aapcs_macho_gas"])])
AC_SUBST([fctx_arch_bin])
AM_SUBST_NOTMAKE([fctx_arch_bin])
dnl ----------------------------------------------------------------------------


dnl ----------------------------------------------------------------------------
dnl setup top-level argument handling
dnl ----------------------------------------------------------------------------
# --enable-affinity
AS_IF([test "x$enable_affinity" != "xno"], [
       AC_CHECK_LIB(pthread, pthread_setaffinity_np,
                    [AC_DEFINE(HAVE_PTHREAD_SETAFFINITY_NP, 1,
                     [Define if pthread_setaffinity_np is available])])
       ])


# --enable-debug: debug options
# strip off multiple options, separated by commas
save_IFS="$IFS"
IFS=","
for option in $enable_debug ; do
    case "$option" in
        yes)
            debug_flags=yes
        ;;
        log)
            debug_log=yes
            debug_log_print=yes
        ;;
        all)
            debug_flags=yes
            debug_log=yes
            debug_log_print=yes
        ;;
        most)
            debug_flags=yes
            debug_log=yes
            debug_log_print=no
        ;;
        no|none)
            debug_flags=no
            debug_log=no
        ;;
        *)
            IFS=$save_IFS
            AC_MSG_WARN([Unknown value $option for --enable-debug])
            IFS=","
        ;;
    esac
done
IFS="$save_IFS"

if test "x$debug_flags" = "xyes" ; then
    PAC_APPEND_FLAG([-g], [CFLAGS])
    PAC_APPEND_FLAG([-Wall], [CFLAGS])
    PAC_APPEND_FLAG([-g], [CXXFLAGS])
    PAC_APPEND_FLAG([-Wall], [CXXFLAGS])
    PAC_APPEND_FLAG([-g], [CCASFLAGS])
    PAC_APPEND_FLAG([-Wall], [CCASFLAGS])
fi
AS_IF([test "x$debug_log" = "xyes"],
    [AC_DEFINE(ABT_CONFIG_USE_DEBUG_LOG, 1, [Define to enable debug logging])])
AS_IF([test "x$debug_log_print" = "xyes"],
    [AC_DEFINE(ABT_CONFIG_USE_DEBUG_LOG_PRINT, 1,
        [Define to enable printing debug log messages])])


# --enable-fast: compiler optimization flags
# strip off multiple options, separated by commas
save_IFS="$IFS"
IFS=","
for option in $enable_fast ; do
    case "$option" in
        O*)
            enable_fast_opts=$option
        ;;
        ndebug)
            enable_fast_ndebug=yes
        ;;
        all|yes)
            enable_fast_ndebug=yes
            enable_fast_opts=O2
        ;;
        none|no)
            enable_fast_ndebug=no
            enable_fast_opts=O0
        ;;
        *)
            IFS="$save_IFS"
            AC_MSG_WARN([Unknown value $option for --enable-fast])
            IFS=","
        ;;
    esac
done
IFS="$save_IFS"

if test -n "$enable_fast_opts" ; then
    # Allows O<n> where <n> can be [0-9sz] or ' '.
    opt_flags=`echo $enable_fast_opts | sed -e 's%\(O[0-9sz] \)%\1%g'`
    if test -n "$opt_flags" ; then
        CXXFLAGS="$CXXFLAGS -$enable_fast_opts"
        CCASFLAGS="$CCASFLAGS -$enable_fast_opts"
        PAC_C_CHECK_COMPILER_OPTION( [$ABT_DEFAULT_COPTS],
            [CFLAGS="$CFLAGS -$enable_fast_opts"] )
    else
        AC_MSG_WARN([Unknown value $enable_fast_opts for --enable-fast])
    fi
fi

if test "$enable_fast_ndebug" = "yes" ; then
    CFLAGS="$CFLAGS -DNDEBUG -DNVALGRIND"
    CXXFLAGS="$CXXFLAGS -DNDEBUG -DNVALGRIND"
    CCASFLAGS="$CFLAGS -DNDEBUG -DNVALGRIND"
fi


# --enable-tls-model: compiler's tls-model flag
case "$enable_tls_model" in
    global-dynamic|local-dynamic|initial-exec|local-exec)
        tls_model_flag=$enable_tls_model
    ;;
    yes)
        tls_model_flag=initial-exec;
    ;;
    none|no)
    ;;
    *)
        AC_MSG_WARN([Unknown value $option for --enable-tls-model])
    ;;
esac
if test -n "$tls_model_flag" ; then
    PAC_APPEND_FLAG([-ftls-model=$tls_model_flag], [CFLAGS])
    PAC_APPEND_FLAG([-ftls-model=$tls_model_flag], [CXXFLAGS])
    PAC_APPEND_FLAG([-ftls-model=$tls_model_flag], [CCASFLAGS])
fi


# --enable-perf-opt: set CFLAGS for the performance optimization
if test "x$enable_perf_opt" = "xyes"; then
    CFLAGS="$CFLAGS -O3 -ftls-model=initial-exec"
    CXXFLAGS="$CFLAGS -O3 -ftls-model=initial-exec"
    CCASFLAGS="$CFLAGS -O3 -ftls-model=initial-exec"
fi


# --enable-valgrind: enable valgrind support if requested
AS_IF([test "x$enable_valgrind" = "xyes"], [
       AC_DEFINE(HAVE_VALGRIND_SUPPORT, 1, [Define valgrind support])
       ])


# --enable-checks
# strip off multiple options, separated by commas
save_IFS="$IFS"
IFS=","
for option in $enable_checks ; do
    case "$option" in
        all|yes)
            enable_error_check=yes
            enable_pool_producer_check=yes
            enable_pool_consumer_check=yes
        ;;
        no-error)
            enable_error_check=no
        ;;
        no-pool-producer)
            enable_pool_producer_check=no
        ;;
        no-pool-consumer)
            enable_pool_consumer_check=no
        ;;
        none|no)
            enable_error_check=no
            enable_pool_producer_check=no
            enable_pool_consumer_check=no
        ;;
        *)
            IFS="$save_IFS"
            AC_MSG_WARN([Unknown value $option for --enable-checks])
            IFS=","
        ;;
    esac
done
IFS="$save_IFS"

AS_IF([test "x$enable_error_check" = "xno"],
    [AC_DEFINE(ABT_CONFIG_DISABLE_ERROR_CHECK, 1,
        [Define to disable error check])
     ABT_NULL="1"],
    [ABT_NULL="0"])
AC_SUBST([ABT_NULL])

AS_IF([test "x$enable_pool_producer_check" = "xno"],
    [AC_DEFINE(ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK, 1,
        [Define to disable pool producer check])])

AS_IF([test "x$enable_pool_consumer_check" = "xno"],
    [AC_DEFINE(ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK, 1,
        [Define to disable pool consumer check])])

AM_CONDITIONAL([ABT_CONFIG_DISABLE_POOL_ACCESS_CHECK],
    [test "x$enable_pool_producer_check" = "xno" -o "x$enable_pool_consumer_check" = "xno"])


# --disable-fcontext
# if fcontext supports this platform and the user does not specify
# --disable-fcontext, we use fcontext instead of ucontext.
AM_CONDITIONAL([ABT_USE_FCONTEXT],
               [test "x$fctx_arch_bin" != "x" -a "x$enable_fcontext" != "xno"])
AS_IF([test "x$fctx_arch_bin" != "x" -a "x$enable_fcontext" != "xno"],
      [AC_DEFINE(ABT_CONFIG_USE_FCONTEXT, 1, [Define to use fcontext])])

# --disable-preserve-fpu
AS_IF([test "x$enable_preserve_fpu" != "xno"],
      [AC_DEFINE(ABTD_FCONTEXT_PRESERVE_FPU, 1,
                 [Define to 1 if we preserve fpu registers])],
      [AC_DEFINE(ABTD_FCONTEXT_PRESERVE_FPU, 0)])


# --disable-mem-pool: Memory pool is enabled by default.
AS_IF([test "x$enable_mem_pool" != "xno"],
      [AC_DEFINE(ABT_CONFIG_USE_MEM_POOL, 1,
          [Define to use memory pools for ULT and tasklet creation])])


# --disable-aligned-alloc: Aligned memory allocation is enabled by default.
AS_IF([test "x$enable_aligned_alloc" != "xno"],
    [AC_DEFINE(ABT_CONFIG_USE_ALIGNED_ALLOC, 1,
        [Define to allocate objects aligned to the cache line size])])


# --enable-feature
save_IFS="$IFS"
IFS=","
for option in $enable_feature ; do
    case "$option" in
        all|yes)
            enable_thread_cancel=yes
            enable_task_cancel=yes
            enable_migration=yes
            enable_stackable_sched=yes
            enable_ext_thread=yes
        ;;
        no-thread-cancel)
            enable_thread_cancel=no
        ;;
        no-task-cancel)
            enable_task_cancel=no
        ;;
        no-migration)
            enable_migration=no
        ;;
        no-stackable-sched)
            enable_stackable_sched=no
        ;;
        no-ext-thread)
            enable_ext_thread=no
        ;;
        none|no)
            enable_thread_cancel=no
            enable_task_cancel=no
            enable_migration=no
            enable_stackable_sched=no
            enable_ext_thread=no
        ;;
        *)
            IFS="$save_IFS"
            AC_MSG_WARN([Unknown value $option for --enable-feature])
            IFS=","
        ;;
    esac
done
IFS="$save_IFS"

AS_IF([test "x$enable_thread_cancel" = "xno"],
    [AC_DEFINE(ABT_CONFIG_DISABLE_THREAD_CANCEL, 1,
        [Define to disable ULT cancellation])])

AS_IF([test "x$enable_task_cancel" = "xno"],
    [AC_DEFINE(ABT_CONFIG_DISABLE_TASK_CANCEL, 1,
        [Define to disable tasklet cancellation])])

AS_IF([test "x$enable_migration" = "xno"],
    [AC_DEFINE(ABT_CONFIG_DISABLE_MIGRATION, 1,
        [Define to disable ULT migration])])

AS_IF([test "x$enable_stackable_sched" = "xno"],
    [AC_DEFINE(ABT_CONFIG_DISABLE_STACKABLE_SCHED, 1,
        [Define to disable stackable scheduler])])

AS_IF([test "x$enable_ext_thread" = "xno"],
    [AC_DEFINE(ABT_CONFIG_DISABLE_EXT_THREAD, 1,
        [Define to disable supporting external threads])])
AM_CONDITIONAL([ABT_CONFIG_DISABLE_EXT_THREAD],
    [test "x$enable_ext_thread" = "xno"])


# --enable-sched-sleep
AS_IF([test "x$enable_sched_sleep" = "xyes"],
      [AC_DEFINE(ABT_CONFIG_USE_SCHED_SLEEP, 1,
                 [Define to make the scheduler sleep when its pools are empty])])


# check if __atomic_exchange_n() is supported
AC_TRY_COMPILE(
[#include <stdint.h>],
[uint32_t val = 0;
 uint32_t old = __atomic_exchange_n(&val, 2, __ATOMIC_SEQ_CST);],
[have_atomic_exchange=yes],
[have_atomic_exchange=no]
)
if test "x$have_atomic_exchange" = "xyes" ; then
    AC_DEFINE(ABT_CONFIG_HAVE_ATOMIC_EXCHANGE, 1,
              [Define if __atomic_exchange_n is supported])
else
    # If __atomic_exchange_n is not supported, we have to use the simple mutex
    # implementation.
    enable_simple_mutex=yes
fi

# --enable-simple-mutex
AS_IF([test "x$enable_simple_mutex" = "xyes"],
      [AC_DEFINE(ABT_CONFIG_USE_SIMPLE_MUTEX, 1,
                 [Define to use a simple mutex implementation])])


# --with-lts
if test "x$with_lts" != "x"; then
    PAC_PREPEND_FLAG([-I${with_lts}/include], [CFLAGS])
    PAC_PREPEND_FLAG([-Wl,-rpath,${with_lts}/lib -L${with_lts}/lib -lthsync], [LDFLAGS])
    AC_CHECK_HEADERS(lh_lock.h clh.h)
fi


# --with-hugetlbfs
if test "x$with_hugetlbfs" != "x"; then
    PAC_PREPEND_FLAG([-I${with_hugetlbfs}/include], [CFLAGS])
    PAC_PREPEND_FLAG([-L${with_hugetlbfs}/lib64 -lhugetlbfs], [LDFLAGS])
    AC_CHECK_LIB(hugetlbfs, get_huge_pages)
fi


# --enable-power-event: power management events from outside
AS_IF([test "x$enable_power_event" = "xyes"],
      [AC_DEFINE(ABT_CONFIG_HANDLE_POWER_EVENT, 1,
                 [Define to handle power management events])])


# --enable-publish-info: publish execution information
AS_IF([test "x$enable_publish_info" = "xyes"],
      [AC_DEFINE(ABT_CONFIG_PUBLISH_INFO, 1,
                 [Define to publish execution information])])


# --with-beacon: BEACON path
if test "x$with_beacon" != "x"; then
    CFLAGS="-I$with_beacon/include $CFLAGS"
    LDFLAGS="-L$with_beacon/lib -lbeacon $LDFLAGS"
fi
AC_CHECK_HEADERS(beacon.h)
default_pub_filename="stdout"
AC_CHECK_LIB(beacon, BEACON_Publish, [default_pub_filename="beacon"])
AC_DEFINE_UNQUOTED(ABT_CONFIG_DEFAULT_PUB_FILENAME, ["$default_pub_filename"],
                   [Default filename for publishing the performance data])


# --with-intercoolr: INTERCOOLR path
if test "x$with_intercoolr" != "x"; then
    CFLAGS="-I$with_intercoolr/include $CFLAGS"
    LDFLAGS="-L$with_intercoolr/lib -lintercoolr $LDFLAGS"
fi
AC_CHECK_HEADERS(raplreader.h)
AC_CHECK_LIB(intercoolr, raplreader_init)
dnl ----------------------------------------------------------------------------


dnl ----------------------------------------------------------------------------
dnl check the function availability
dnl ----------------------------------------------------------------------------
# check pthread
AC_CHECK_HEADERS(pthread.h)
AC_CHECK_LIB(pthread, pthread_join)

# check pthread_barrier
AC_CHECK_FUNCS(pthread_barrier_init)

# check timer functions
# for clock_gettime and clock_getres, we need to search them from librt or
# libposix4 because they may not be included in the standard library.
AC_SEARCH_LIBS([clock_gettime], [rt posix4])
AC_SEARCH_LIBS([clock_getres], [rt posix4])
if test "$ac_cv_search_clock_gettime" != "no" -a \
        "$ac_cv_search_clock_getres" != "no" ; then
    AC_CHECK_FUNCS(clock_gettime clock_getres)
fi
AC_CHECK_FUNCS(mach_absolute_time gettimeofday)
if test "x$ac_cv_func_clock_gettime" = "xyes" -a \
        "x$ac_cv_func_clock_getres" = "xyes" ; then
    timer_type=clock_gettime
elif test "$ac_cv_func_mach_absolute_time" = "yes" ; then
    timer_type=mach_absolute_time
elif test "$ac_cv_func_gettimeofday" = "yes" ; then
    timer_type=gettimeofday
fi
if test -z "$timer_type" ; then
    AC_MSG_ERROR([No timer function found])
fi

# check math library
AC_CHECK_LIB([m], [log10])

# check if MAP_ANONYMOUS or MAP_ANON is defined
AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/mman.h>
#define FLAGS   (MAP_PRIVATE | MAP_ANONYMOUS)
],[void *ptr = mmap(NULL, 4096, PROT_READ, FLAGS, 0, 0);],
[have_map_anonymous=yes],[have_map_anonymous=no])
if test "$have_map_anonymous" = "yes" ; then
AC_DEFINE(HAVE_MAP_ANONYMOUS, 1, [Define if MAP_ANONYMOUS is defined])
else
AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/mman.h>
#define FLAGS   (MAP_PRIVATE | MAP_ANON)
],[void *ptr = mmap(NULL, 4096, PROT_READ, FLAGS, 0, 0);],
[have_map_anon=yes],[have_map_anon=no])
AS_IF([test "$have_map_anon" = "yes"],
    AC_DEFINE(HAVE_MAP_ANON, 1, [Define if MAP_ANON is defined])])
fi

if test "$have_map_anonymous" = "yes" -o "$have_map_anon" = "yes" ; then
# check if MAP_HUGETLB is supported in mmap
AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS   MAP_ANON
#endif
#define PROTS       (PROT_READ | PROT_WRITE)
#define FLAGS_HP    (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)
#define PAGESIZE    (2 * 1024 * 1024)
],[void *ptr = mmap(NULL, PAGESIZE, PROTS, FLAGS_HP, 0, 0);],
[AC_DEFINE(HAVE_MAP_HUGETLB, 1, [Define if MAP_HUGETLB is supported])],,)
fi
dnl ----------------------------------------------------------------------------

AM_INIT_AUTOMAKE([-Wall -Wno-portability-recursive -Werror foreign 1.12.3 subdir-objects])
AM_SILENT_RULES([yes])
AM_PROG_AS
AM_PROG_AR

LT_INIT

AC_PROG_INSTALL


dnl ----------------------------------------------------------------------------
dnl config headers
dnl ----------------------------------------------------------------------------
AC_CONFIG_HEADERS([src/include/abt_config.h])
AH_TOP([
/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABT_CONFIG_H_INCLUDED
#define ABT_CONFIG_H_INCLUDED
])
AH_BOTTOM([
#endif /* ABT_CONFIG_H_INCLUDED */
])
dnl ----------------------------------------------------------------------------


AC_CONFIG_FILES([Makefile
                 Doxyfile
                 maint/argobots.pc
                 src/include/abt.h
                 src/Makefile
                 test/Makefile
                 test/basic/Makefile
                 test/util/Makefile
                 examples/Makefile
                 examples/dynamic-es/Makefile])
AC_OUTPUT
