# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

if ABT_USE_FCONTEXT
abt_sources += \
	arch/fcontext/jump_@fctx_arch_bin@.S \
	arch/fcontext/make_@fctx_arch_bin@.S
endif
