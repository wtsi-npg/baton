
AM_CPPFLAGS = -DTEST_ROOT='"$(top_srcdir)/tests"'
AM_CFLAGS = -Wall -Werror -Wextra -pedantic

if HAVE_IRODS4
AM_CPPFLAGS += $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_FILESYSTEM_LDFLAGS) $(BOOST_REGEX_LDFLAGS) $(BOOST_THREAD_LDFLAGS)
LIBS += $(BOOST_SYSTEM_LIBS) $(BOOST_FILESYSTEM_LIBS) $(BOOST_REGEX_LIBS) $(BOOST_THREAD_LIBS)
endif

TESTS = check_baton

check_PROGRAMS = check_baton

check_baton_SOURCES = check_baton.c $(top_builddir)/src/baton.h
check_baton_CFLAGS = @CHECK_CFLAGS@
check_baton_LDADD = $(top_builddir)/src/libbaton.la @CHECK_LIBS@

EXTRA_DIST = data metadata scripts
