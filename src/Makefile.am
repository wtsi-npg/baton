
lib_LTLIBRARIES = libbaton.la

libbaton_la_SOURCES = baton.c
libbaton_includedir = $(includedir)/baton
libbaton_include_HEADERS = baton.h

bin_PROGRAMS = bmeta

bmeta_SOURCES = bmeta.c
bmeta_LDADD = libbaton.la

AM_CPPFLAGS = -DZLOG_CONF='"$(sysconfdir)/bmeta_zlog.conf"'
