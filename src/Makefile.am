
lib_LTLIBRARIES = libbaton.la

libbaton_la_SOURCES = baton.c json.c utilities.c
libbaton_includedir = $(includedir)/baton
libbaton_include_HEADERS = baton.h json.h utilities.h

bin_PROGRAMS = metalist metamod metaquery json-metamod

metalist_SOURCES = metalist.c
metalist_LDADD = libbaton.la

metamod_SOURCES = metamod.c
metamod_LDADD = libbaton.la

metaquery_SOURCES = metaquery.c
metaquery_LDADD = libbaton.la

json_metamod_SOURCES = json-metamod.c
json_metamod_LDADD = libbaton.la

AM_CPPFLAGS = -DZLOG_CONF='"$(sysconfdir)/baton_zlog.conf"'
