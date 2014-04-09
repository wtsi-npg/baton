
MOSTLYCLEANFILES = *.gcda *.gcno

if COVERAGE_ENABLED
AM_CFLAGS = -fprofile-arcs -ftest-coverage
AM_LDFLAGS = --coverage
else
AM_CFLAGS = -Wall -Werror -Wextra -pedantic
endif

AM_CPPFLAGS = -DZLOG_CONF='"$(sysconfdir)/baton_zlog.conf"'

lib_LTLIBRARIES = libbaton.la

libbaton_la_SOURCES = baton.c error.c log.c json.c json_query.c query.c utilities.c
libbaton_includedir = $(includedir)/baton
libbaton_include_HEADERS = baton.h error.h log.h json.h json_query.h query.h utilities.h

bin_PROGRAMS = metalist metamod metaquery json-chmod json-list json-metalist json-metamod json-metaquery json-metasuper

metalist_SOURCES = metalist.c
metalist_LDADD = libbaton.la

metamod_SOURCES = metamod.c
metamod_LDADD = libbaton.la

metaquery_SOURCES = metaquery.c
metaquery_LDADD = libbaton.la

json_chmod_SOURCES = json-chmod.c
json_chmod_LDADD = libbaton.la

json_list_SOURCES = json-list.c
json_list_LDADD = libbaton.la

json_metalist_SOURCES = json-metalist.c
json_metalist_LDADD = libbaton.la

json_metamod_SOURCES = json-metamod.c
json_metamod_LDADD = libbaton.la

json_metaquery_SOURCES = json-metaquery.c
json_metaquery_LDADD = libbaton.la

json_metasuper_SOURCES = json-metasuper.c
json_metasuper_LDADD = libbaton.la
