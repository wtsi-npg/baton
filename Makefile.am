ACLOCAL_AMFLAGS = -I m4  -I /usr/local/share/aclocal

SUBDIRS = src tests

if HAVE_CHECK
	SUBDIRS += tests
endif

etcdir = $(sysconfdir)
etc_DATA = bmeta_zlog.conf
