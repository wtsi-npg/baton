
MOSTLYCLEANFILES = *.gcda *.gcno

if COVERAGE_ENABLED
AM_CFLAGS = -fprofile-arcs -ftest-coverage
AM_LDFLAGS = --coverage
else
AM_CFLAGS = -Wall -Werror -Wextra -pedantic
endif

lib_LTLIBRARIES = libbaton.la

libbaton_includedir = $(includedir)/baton
libbaton_include_HEADERS = baton.h error.h log.h json.h json_query.h query.h read.h utilities.h
libbaton_la_SOURCES = baton.c error.c log.c json.c json_query.c query.c read.c utilities.c

bin_PROGRAMS = baton-chmod baton-get baton-list baton-metamod baton-metaquery baton-metasuper

baton_chmod_SOURCES = baton-chmod.c
baton_chmod_LDADD = libbaton.la

baton_get_SOURCES = baton-get.c
baton_get_LDADD = libbaton.la

baton_list_SOURCES = baton-list.c
baton_list_LDADD = libbaton.la

baton_metamod_SOURCES = baton-metamod.c
baton_metamod_LDADD = libbaton.la

baton_metaquery_SOURCES = baton-metaquery.c
baton_metaquery_LDADD = libbaton.la

baton_metasuper_SOURCES = baton-metasuper.c
baton_metasuper_LDADD = libbaton.la
