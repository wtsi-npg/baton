AC_INIT([baton], [0.1.0], [bug-report@address])

# AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_LIBTOOL
AC_PROG_INSTALL

LT_INIT

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AM_DISABLE_SHARED

AC_CHECK_LIB([zlog], [zlog_init])
AC_CHECK_LIB([pthread], [pthread_kill])

AC_OUTPUT
