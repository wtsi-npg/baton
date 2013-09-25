
SWIG_SOURCES = baton.i

lib_LTLIBRARIES = libpbaton.la

libpbaton_la_SOURCES = $(srcdir)/baton_wrap.c $(SWIG_SOURCES)
libpbaton_la_CPPFLAGS =  -I$(top_srcdir)/src $(PERL_EXT_CPPFLAGS) -I$(PERL_EXT_INC)
libpbaton_la_LIBADD = ../src/libbaton.la

$(srcdir)/baton_wrap.c : $(SWIG_SOURCES)
	$(SWIG) -perl -I$(top_srcdir)/src $(CPPFLAGS) $(AM_CPPFLAGS) -I$(includedir) -o $@ $<

AM_CPPFLAGS = -I/usr/local/include/ -I/usr/include/
