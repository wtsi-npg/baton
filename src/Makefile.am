
MOSTLYCLEANFILES = *.gcda *.gcno

AM_CPPFLAGS = $(IRODS_CPPFLAGS)
AM_LDFLAGS  = $(IRODS_LDFLAGS)

if COVERAGE_ENABLED
AM_CFLAGS   = -fprofile-arcs -ftest-coverage
AM_LDFLAGS += --coverage
else
AM_CFLAGS = -Wall -Werror -Wextra -pedantic
endif

LT_VERSION_INFO=$(LT_LIB_CURRENT):$(LT_LIB_REVISION):$(LT_LIB_AGE)

lib_LTLIBRARIES = libbaton.la

libbaton_includedir = $(includedir)/baton

libbaton_include_HEADERS = baton.h \
                           compat_checksum.h \
                           error.h \
                           json.h \
                           json_query.h \
                           list.h \
                           log.h \
                           operations.h \
                           query.h \
                           read.h \
                           signal_handler.h \
                           utilities.h \
                           write.h

libbaton_la_SOURCES = baton.c \
                      compat_checksum.c \
                      error.c \
                      json.c \
                      json_query.c \
                      list.c \
                      log.c \
                      operations.c \
                      query.c \
                      read.c \
                      signal_handler.c \
                      utilities.c \
                      write.c

libbaton_la_LDFLAGS = -version-info $(LT_VERSION_INFO) $(IRODS_LDFLAGS)
libbaton_la_LIBADD = $(IRODS_LIBS)

bin_PROGRAMS = baton-chmod \
               baton-do \
               baton-get \
               baton-list \
               baton-metamod \
               baton-metaquery \
               baton-put \
               baton-specificquery

baton_chmod_SOURCES = baton-chmod.c
baton_chmod_LDADD = libbaton.la $(IRODS_LIBS)

baton_do_SOURCES = baton-do.c
baton_do_LDADD = libbaton.la $(IRODS_LIBS)

baton_get_SOURCES = baton-get.c
baton_get_LDADD = libbaton.la $(IRODS_LIBS)

baton_list_SOURCES = baton-list.c
baton_list_LDADD = libbaton.la $(IRODS_LIBS)

baton_metamod_SOURCES = baton-metamod.c
baton_metamod_LDADD = libbaton.la $(IRODS_LIBS)

baton_metaquery_SOURCES = baton-metaquery.c
baton_metaquery_LDADD = libbaton.la $(IRODS_LIBS)

baton_put_SOURCES = baton-put.c
baton_put_LDADD = libbaton.la $(IRODS_LIBS)

baton_specificquery_SOURCES = baton-specificquery.c
baton_specificquery_LDADD = libbaton.la $(IRODS_LIBS)

bin_SCRIPTS = baton
CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST = baton.in

do_subst = sed -e 's,[@]PACKAGE_VERSION[@],$(PACKAGE_VERSION),g'

baton: baton.in Makefile
	$(do_subst) < $(srcdir)/baton.in > baton
	@chmod +x baton
