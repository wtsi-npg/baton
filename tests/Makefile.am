
AM_CPPFLAGS = -DTEST_ROOT='"$(top_srcdir)/tests"' -DTEST_RESOURCE='"$(TEST_RESOURCE)"' $(IRODS_CPPFLAGS)
AM_CFLAGS = -Wall -Werror -Wextra -pedantic

TESTS = check_baton

check_PROGRAMS = check_baton

check_baton_SOURCES = check_baton.c $(top_builddir)/src/baton.h
check_baton_CFLAGS = @CHECK_CFLAGS@
check_baton_LDADD = $(top_builddir)/src/libbaton.la @CHECK_LIBS@ $(IRODS_LIBS)

EXTRA_DIST = data metadata scripts sql
