
AM_CPPFLAGS = -DTEST_ROOT='"$(top_srcdir)/tests"' -DTEST_RESOURCE='"$(TEST_RESOURCE)"' $(IRODS_CPPFLAGS)
AM_CFLAGS   = -Wall -Werror -Wextra -pedantic
AM_LDFLAGS  = $(IRODS_LDFLAGS)

if SANITIZER_ENABLED
AM_CFLAGS += -fsanitize=address
AM_LDFLAGS += -fsanitize=address
endif

AM_TESTS_ENVIRONMENT = ienv && echo $(TEST_IRODS_PASSWORD) | iinit && ils -l;

TESTS = check_baton

check_PROGRAMS = check_baton

check_baton_SOURCES = check_baton.c $(top_builddir)/src/baton.h
check_baton_CFLAGS = @CHECK_CFLAGS@
check_baton_LDADD = $(top_builddir)/src/libbaton.la @CHECK_LIBS@ $(IRODS_LIBS)

EXTRA_DIST = data metadata scripts sql
