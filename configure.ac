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
AC_PROG_CC
AC_HEADER_STDC

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
              [darwin*], [fctx_arch_bin="i386_sysv_macho_gas"])],
    [x86_64], [AS_CASE([$host_os],
              [linux*],  [fctx_arch_bin="x86_64_sysv_elf_gas"],
              [darwin*], [fctx_arch_bin="x86_64_sysv_macho_gas"])],
    [p*pc32], [AS_CASE([$host_os],
              [linux*],  [fctx_arch_bin="ppc32_sysv_elf_gas"],
              [darwin*], [fctx_arch_bin="ppc32_sysv_macho_gas"])],
    [p*pc64], [AS_CASE([$host_os],
              [linux*],  [fctx_arch_bin="ppc64_sysv_elf_gas"],
              [darwin*], [fctx_arch_bin="ppc64_sysv_macho_gas"])])
AC_SUBST([fctx_arch_bin])
AM_SUBST_NOTMAKE([fctx_arch_bin])

# check pthread
AC_CHECK_HEADERS(pthread.h)
AC_CHECK_LIB(pthread, pthread_join)

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
dnl ----------------------------------------------------------------------------

dnl ----------------------------------------------------------------------------
dnl setup top-level argument handling
dnl ----------------------------------------------------------------------------
# debug options
AC_ARG_ENABLE([debug],
[  --enable-debug@<:@=OPTS@:>@   control the level of debugging. "OPTS" is a list of
                          comma separated names below. Default is "yes".
                           none - no debugging
                           yes  - add compiler flags, -g -O0 -Wall
                           log  - enable debug event logging
                           all  - all of the above choices],,
[enable_debug=none])

# strip off multiple options, separated by commas
save_IFS="$IFS"
IFS=","
for option in $enable_debug ; do
    case "$option" in
        no|none)
        ;;
        yes)
            debug_flags=yes
        ;;
        log)
            debug_log=yes
        ;;
        all)
            debug_flags=yes
            debug_log=yes
        ;;
        *)
            IFS=$save_IFS
            AC_MSG_WARN([Unknown value $option for enable-debug])
            IFS=","
        ;;
    esac
done
IFS="$save_IFS"

AS_IF([test "x$debug_flags" = "xyes"], [CFLAGS="$CFLAGS -g -O0 -Wall"])
AS_IF([test "x$debug_log" = "xyes"],
    [AC_DEFINE(ABT_CONFIG_USE_DEBUG_LOG, 1, [Define to enable debug logging])])

AC_ARG_ENABLE([affinity],
    AS_HELP_STRING([--enable-affinity],
        [enable ES affinity]),,
        [enable_affinity=no])
AS_IF([test "x$enable_affinity" != "xno"], [
       AC_CHECK_LIB(pthread, pthread_setaffinity_np,
                    [AC_DEFINE(HAVE_PTHREAD_SETAFFINITY_NP, 1,
                     [Define if pthread_setaffinity_np is available])])
       ])

# set CFLAGS for the performance optimization
AC_ARG_ENABLE([perf-opt],
    AS_HELP_STRING([--enable-perf-opt], [enable performance optimization]))
if test "x$enable_perf_opt" = "xyes"; then
    if test "x$GCC" = "xyes"; then
        CFLAGS="-O3 -ftls-model=initial-exec"
    fi
fi

# enable valgrind support if requested
AC_ARG_ENABLE([valgrind],
    AS_HELP_STRING([--enable-valgrind], [enable valgrind support]))
AS_IF([test "x$enable_valgrind" = "xyes"], [
       AC_DEFINE(HAVE_VALGRIND_SUPPORT, 1, [Define valgrind support])
       ])

# Disable checks if requested
AC_ARG_ENABLE([checks],
    AS_HELP_STRING([--disable-checks], [disable checks (unsafe mode)]))
AS_IF([test "x$enable_checks" = "xno"],
      [AC_DEFINE(UNSAFE_MODE, 1, [Define unsafe mode]) ABT_NULL="1"],
      [ABT_NULL="0"])
AC_SUBST([ABT_NULL])

AC_ARG_ENABLE([fcontext],
    AS_HELP_STRING([--disable-fcontext],
        [do not use fcontext even though it is supported. If you disable
         fcontext, ucontext in libc is used.]),,
        [enable_fcontext=yes])
# if fcontext supports this platform and the user does not specify
# --disable-fcontext, we use fcontext instead of ucontext.
AM_CONDITIONAL([ABT_USE_FCONTEXT],
               [test "x$fctx_arch_bin" != "x" -a "x$enable_fcontext" != "xno"])
AS_IF([test "x$fctx_arch_bin" != "x" -a "x$enable_fcontext" != "xno"],
      [AC_DEFINE(ABT_CONFIG_USE_FCONTEXT, 1, [Define to use fcontext])])

AC_ARG_ENABLE([preserve-fpu],
    AS_HELP_STRING([--disable-preserve-fpu],
        [do not preserve fpu registers when fcontext is used]),,
        [enable_preserve_fpu=yes])
AS_IF([test "x$enable_preserve_fpu" != "xno"],
      [AC_DEFINE(ABTD_FCONTEXT_PRESERVE_FPU, 1,
                 [Define to 1 if we preserve fpu registers])],
      [AC_DEFINE(ABTD_FCONTEXT_PRESERVE_FPU, 0)])
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
                 src/include/abt.h
                 src/Makefile
                 test/Makefile
                 test/basic/Makefile
                 test/util/Makefile
                 examples/Makefile
                 examples/dynamic-es/Makefile])
AC_OUTPUT
