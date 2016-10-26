
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
libbaton_include_HEADERS = baton.h error.h log.h json.h json_query.h query.h read.h compat_checksum.h utilities.h
libbaton_la_SOURCES = baton.c error.c log.c json.c json_query.c query.c read.c compat_checksum.c utilities.c
libbaton_la_LDFLAGS = -version-info $(LT_VERSION_INFO) $(IRODS_LDFLAGS)
libbaton_la_LIBADD = $(IRODS_LIBS)

bin_PROGRAMS = baton-chmod baton-get baton-list baton-metamod baton-metaquery \
               baton-metasuper baton-specificquery

baton_chmod_SOURCES = baton-chmod.c
baton_chmod_LDADD = libbaton.la $(IRODS_LIBS)

baton_get_SOURCES = baton-get.c
baton_get_LDADD = libbaton.la $(IRODS_LIBS)

baton_list_SOURCES = baton-list.c
baton_list_LDADD = libbaton.la $(IRODS_LIBS)

baton_metamod_SOURCES = baton-metamod.c
baton_metamod_LDADD = libbaton.la $(IRODS_LIBS)

baton_metaquery_SOURCES = baton-metaquery.c
baton_metaquery_LDADD = libbaton.la $(IRODS_LIBS)

baton_metasuper_SOURCES = baton-metasuper.c
baton_metasuper_LDADD = libbaton.la $(IRODS_LIBS)

baton_specificquery_SOURCES = baton-specificquery.c
baton_specificquery_LDADD = libbaton.la $(IRODS_LIBS)

bin_SCRIPTS = baton
CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST = baton.in

do_subst = sed -e 's,[@]PACKAGE_VERSION[@],$(PACKAGE_VERSION),g'

baton: baton.in Makefile
	$(do_subst) < $(srcdir)/baton.in > baton
	@chmod +x baton
