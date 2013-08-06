AC_INIT([baton], [m4_esyscmd([git describe --dirty | tr -d '\012'])], [kdj@sanger.ac.uk])

AM_DISABLE_SHARED

AC_PROG_CC_C99
AM_PROG_CC_C_O

AC_PROG_LIBTOOL
AC_PROG_INSTALL


# LT_INIT

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AC_CHECK_LIB([zlog], [zlog_init])
AC_CHECK_LIB([jansson], [json_unpack])
AC_CHECK_LIB([pthread], [pthread_kill])
AC_CHECK_LIB([gssapi_krb5], [gss_acquire_cred])

AC_OUTPUT
