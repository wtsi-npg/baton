AC_INIT([baton], [m4_esyscmd([git describe --dirty | tr -d '\012'])],
[kdj@sanger.ac.uk])

AC_DISABLE_SHARED([])

AC_PROG_CC_C99
AM_PROG_CC_C_O

AC_PROG_LIBTOOL # LT_INIT([disable-shared])
AC_PROG_INSTALL

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])
AM_CONDITIONAL([HAVE_CHECK], [test x"$have_check" = "xyes"])

AC_CHECK_LIB([zlog], [zlog_init])
AC_CHECK_LIB([jansson], [json_unpack])
AC_CHECK_LIB([pthread], [pthread_kill])
AC_CHECK_LIB([gssapi_krb5], [gss_acquire_cred])

AC_OUTPUT
