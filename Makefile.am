# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

ACLOCAL_AMFLAGS = -I m4

abt_sources =

include src/Makefile.mk

lib_LTLIBRARIES = libabt.la
libabt_la_SOURCES = $(abt_sources)
libabt_la_CPPFLAGS = -I$(top_builddir)/src -pthread
libabt_la_LDFLAGS = -version-info @libabt_so_version@ -pthread

EXTRA_DIST = autogen.sh
