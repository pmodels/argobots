# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src
DIST_SUBDIRS = $(SUBDIRS) test examples

EXTRA_DIST = autogen.sh

.PHONY: build-all clean-all doxygen

build-all:
	$(MAKE)
	$(MAKE) -C test
	$(MAKE) -C examples

clean-all:
	$(MAKE) clean
	$(MAKE) -C test clean
	$(MAKE) -C examples clean

doxygen:
	mkdir -p doc
	doxygen Doxyfile
