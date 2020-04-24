# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

AM_CPPFLAGS = $(DEPS_CPPFLAGS)
AM_CPPFLAGS += -I$(top_builddir)/src/include -std=gnu99
AM_LDFLAGS = $(DEPS_LDFLAGS)

libabt = $(top_builddir)/src/libabt.la

$(libabt):
	$(MAKE) -C $(top_builddir)/src

LDADD = $(libabt)
