# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	dyn_server \
	dyn_app \
	dyn_event

noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

dyn_server_SOURCES = dyn_server.c common.h
dyn_app_SOURCES = dyn_app.c common.h
dyn_event_SOURCES = dyn_event.c dyn_event.h dyn_event_rt1.c


