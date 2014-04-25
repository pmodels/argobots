m4_include([maint/version.m4])
AC_INIT([argobots],ARGOBOTS_VERSION_m4)

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

AM_INIT_AUTOMAKE([-Wall -Wno-portability-recursive -Werror foreign 1.12.3 subdir-objects])
AM_SILENT_RULES([yes])
AM_PROG_AR
AC_CONFIG_HEADER(config.h)

LT_INIT

AC_PROG_INSTALL

AC_OUTPUT(Makefile
	src/abt.h)
