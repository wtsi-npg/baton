AC_INIT([baton], m4_esyscmd_s([git describe --dirty --always --tags]), [kdj+baton@sanger.ac.uk])

AC_PREREQ(2.68)
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([foreign -Wall -Werror]) # Must precede AC_PROG_CC

AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_INSTALL

AM_PROG_AR
AM_PROG_CC_C_O

AC_LANG([C])

LT_INIT

AC_CONFIG_MACRO_DIR([m4])

LT_LIB_CURRENT=0
LT_LIB_REVISION=1
LT_LIB_AGE=0
AC_SUBST(LT_LIB_CURRENT)
AC_SUBST(LT_LIB_REVISION)
AC_SUBST(LT_LIB_AGE)

dnl Begin check unit test
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4], [check_present=yes],
                  [check_present=no])
AM_CONDITIONAL(HAVE_CHECK, test "x$check_present" = "xyes")
dnl End check

dnl Begin Sphinx documentation build (HTML manual and manpage)
AC_ARG_VAR([SPHINXBUILD], [Sphinx documentation builder])
AC_PATH_PROG([SPHINXBUILD], sphinx-build, no)

AS_IF(
  [test "x${SPHINXBUILD}" = "xno"],
    [AC_MSG_WARN([unable to find sphinx-build on this system;
                  the HTML manual and manpage will not be created])],
  [])

AM_CONDITIONAL(HAVE_SPHINX, [test "x${SPHINXBUILD}" != "xno"])
dnl End Sphinx

dnl Begin GitHub release upload
AC_ARG_VAR([GITHUB_API_TOKEN], [GitHub API token file])
AC_ARG_VAR([GITHUB_USER], [GitHub release user name (default: $USER)])
AC_ARG_VAR([GITHUB_REPO], [GitHub release repository (default: baton)])

if test -z "$GITHUB_USER"; then
   GITHUB_USER=${USER}
fi

if test -z "$GITHUB_REPO"; then
   GITHUB_REPO=baton
fi

AC_ARG_ENABLE([github-release],
  [AS_HELP_STRING([--enable-github-release],
    [Enable GitHub release (default is no)])],
  [release_enabled=${enableval}], [release_enabled=no])

AS_IF(
  [test "x${release_enabled}" = "xyes"],
    AS_IF([test "x${GITHUB_API_TOKEN}" = "x"],
            [AC_MSG_ERROR(No GitHub API token file set. GitHub releases will not be made)],
          [AC_MSG_NOTICE([GitHub release enabled for repo $GITHUB_REPO])
           AC_MSG_NOTICE([GitHub release user $GITHUB_USER])
           AC_MSG_NOTICE([GitHub release API token $GITHUB_API_TOKEN])]))
dnl End GitHub

dnl Begin Lcov code coverage analysis

dnl Be aware on OS X that gcc may not really be gcc, but a symlink to
dnl llvm-gcc. In this case, the coverage files will not be generated
dnl unless CC is set to the real gcc when running configure.

AC_ARG_ENABLE([coverage],
  [AS_HELP_STRING([--enable-coverage],
    [Enable code coverage analysis (default is no)])],
  [coverage_enabled=${enableval}], [coverage_enabled=no])

AC_ARG_VAR([LCOV], [lcov code coverage analysis tool])
AC_ARG_VAR([GENHTML], [lcov HTML reporting tool])

AS_IF(
  [test "x${coverage_enabled}" = "xyes"],
    [AC_CHECK_LIB(gcov, __gcov_init, [], [])
     AC_PATH_PROG([LCOV], lcov, no)
     AS_IF([test "x${LCOV}" = "xno"],
             [AC_MSG_ERROR(failed to find lcov for coverage analysis)],
            [])
     AC_PATH_PROG([GENHTML], genhtml, no)
     AS_IF([test "x${GENHTML}" = "xno"],
             [AC_MSG_ERROR(failed to find genhtml for coverage reporting)],
           [])],
  [])

AM_CONDITIONAL(COVERAGE_ENABLED, [test "x${coverage_enabled}" = "xyes"])
dnl End Lcov

dnl Begin address-sanitizer
AC_ARG_VAR([ASAN], [Address sanitizer])

AC_ARG_ENABLE([address-sanitizer],
  [AS_HELP_STRING([--enable-address-sanitizer],
    [Enable address-sanitizer (default is no)])],
  [sanitizer_enabled=${enableval}], [sanitizer_enabled=no])

AM_CONDITIONAL(SANITIZER_ENABLED, [test "x${sanitizer_enabled}" = "xyes"])  

dnl End address-sanitizer

dnl Begin test resource
TEST_RESOURCE=testResc
AC_SUBST([TEST_RESOURCE])

AC_ARG_WITH([test-resource],
  [AS_HELP_STRING([--with-test-resource],
    [Specify an iRODS resource name for making replicates during testing (default: testResc)])],
   [AC_MSG_NOTICE([using iRODS test resource $with_test_resource])
    TEST_RESOURCE="$with_test_resource"],
   [AC_MSG_NOTICE([using the default iRODS test resource])
    TEST_RESOURCE="testResc"])
dnl End test resource

dnl Begin test iRODS password
TEST_IRODS_PASSWORD=irods
AC_SUBST([TEST_IRODS_PASSWORD])

AC_ARG_WITH([test-irods-password],
  [AS_HELP_STRING([--with-test-irods-password],
    [Specify an iRODS password for testing (default: irods)])],
   [AC_MSG_NOTICE([using iRODS test password $with_test_irods_password])
    TEST_IRODS_PASSWORD="$with_test_irods_password"],
   [AC_MSG_NOTICE([using the default iRODS test password])
    TEST_IRODS_PASSWORD="irods"])
dnl End test iRODS password

AX_PTHREAD(, [AC_MSG_ERROR([unable to find libpthread])])

AC_CHECK_LIB([jansson], [json_unpack], [],
             [AC_MSG_ERROR([unable to find libjansson])])

AX_WITH_IRODS

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/baton.h])
AC_CONFIG_FILES([baton.pc Makefile src/Makefile tests/Makefile doc/Makefile])
AC_OUTPUT
