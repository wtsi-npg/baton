SUBDIRS = src .
ACLOCAL_AMFLAGS= -I m4

if HAVE_SWIG
SUBDIRS += perl
endif

if HAVE_CHECK
SUBDIRS += tests
endif

etcdir = $(sysconfdir)
etc_DATA = baton_zlog.conf
