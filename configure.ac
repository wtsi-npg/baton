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

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4], [HAVE_CHECK=1], [HAVE_CHECK=0])
AM_CONDITIONAL(HAVE_CHECK, test x"$have_check" = "xyes")

AC_CHECK_PROGS([SPHINXBUILD], [sphinx-build], [no])
AS_IF([test "x$SPHINXBUILD" = xno],
      AC_MSG_WARN(sphinx-build is required to build documentation))

AC_ARG_ENABLE(coverage,
  AS_HELP_STRING([--enable-coverage], [Enable gcov/lcov support]),
    [coverage_enabled=$enableval], [coverage_enabled=no])

if test "$coverage_enabled" = yes; then
  AC_CHECK_LIB(gcov, __gcov_init, [], [])

  AC_PATH_PROG(LCOV, lcov, no)
   if test "x$LCOV" = "xno"; then
     AC_MSG_ERROR([Could not find lcov; install lcov package for coverage.])
   fi

  AC_PATH_PROG(GENHTML, genhtml, no)
   if test "x$GENHTML" = "xno"; then
     AC_MSG_ERROR([Could not find genhtml; install lcov package for coverage.])
   fi
fi
AM_CONDITIONAL(COVERAGE_ENABLED, [test "$coverage_enabled"=yes])

AC_CHECK_LIB([zlog], [zlog_init])
AC_CHECK_LIB([jansson], [json_unpack])
AC_CHECK_LIB([pthread], [pthread_kill])
AC_CHECK_LIB([gssapi_krb5], [gss_acquire_cred])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/baton.h])
AC_CONFIG_FILES([baton.pc Makefile src/Makefile tests/Makefile doc/Makefile])
AC_OUTPUT
