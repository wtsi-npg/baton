
AM_CPPFLAGS = @IRODS_CPPFLAGS@
AM_LDFLAGS  = @IRODS_LDFLAGS@
AM_LDLIBS   = @IRODS_LDLIBS@

AM_CPPFLAGS += -DTEST_ROOT='"$(top_srcdir)/tests"'

TESTS = check_baton

check_PROGRAMS = check_baton

check_baton_SOURCES = check_baton.c $(top_builddir)/src/baton.h
check_baton_CFLAGS = @CHECK_CFLAGS@
check_baton_LDADD = $(top_builddir)/src/libbaton.la @CHECK_LIBS@

EXTRA_DIST = data metadata scripts
