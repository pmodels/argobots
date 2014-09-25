# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS = $(DEPS_CPPFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/src/include -I$(top_srcdir)/test/util
AM_LDFLAGS = $(DEPS_LDFLAGS)

libabt = $(top_srcdir)/src/libabt.la
libutil = $(top_srcdir)/test/util/libutil.la

$(libabt):
	$(MAKE) -C $(top_srcdir)/src

$(libutil):
	$(MAKE) -C $(top_srcdir)/test/util

LDADD = $(libabt) $(libutil)

