
lib_LTLIBRARIES = libbaton.la

libbaton_la_SOURCES = baton.c
libbaton_includedir = $(includedir)/baton
libbaton_include_HEADERS = baton.h

bin_PROGRAMS = metamod metaquery

metamod_SOURCES = metamod.c
metamod_LDADD = libbaton.la

metaquery_SOURCES = metaquery.c
metaquery_LDADD = libbaton.la

AM_CPPFLAGS = -DZLOG_CONF='"$(sysconfdir)/metamod_zlog.conf"'
