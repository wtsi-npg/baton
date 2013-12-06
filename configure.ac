AC_INIT([baton], m4_esyscmd([git describe --dirty | tr -d '\012']), [kdj@sanger.ac.uk])

AC_PREREQ(2.68)

AM_INIT_AUTOMAKE([foreign -Wall -Werror]) # Must precede AC_PROG_CC

AC_PROG_CC_C99
AM_PROG_CC_C_O
AM_PROG_AR

LT_INIT
AC_PROG_INSTALL

AC_CONFIG_MACRO_DIR([m4])

# Notes for OSX with homebrew:
# Ensure that check.m4 and pkg.m4 are in a system aclocal path or
# a path added by ACLOCAL_AMFLAGS in Makefile.am

# AM_PATH_CHECK is deprecated and replaced by PKG_CHECK_MODULES
# AM_PATH_CHECK(,[have_check="yes"],
#         AC_MSG_WARN([Check not found; cannot run unit tests!])
#         [have_check="no"])
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4], [HAVE_CHECK=1], [HAVE_CHECK=0])
AM_CONDITIONAL(HAVE_CHECK, test x"$have_check" = "xyes")

AC_CHECK_LIB([zlog], [zlog_init])
AC_CHECK_LIB([jansson], [json_unpack])
AC_CHECK_LIB([pthread], [pthread_kill])
AC_CHECK_LIB([gssapi_krb5], [gss_acquire_cred])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/baton.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
