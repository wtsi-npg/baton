\AC_INIT([baton], m4_esyscmd([git describe --dirty | tr -d '\012']), [kdj@sanger.ac.uk])

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

dnl Packaged (.deb) versus source tree (run-in-place) installation.
IS_PACKAGED_INSTALL=no

IRODS_HOME=
AC_ARG_WITH([irods],
 [AS_HELP_STRING([--with-irods],
   [Specify the location of a run-in-place iRODS installation (default: /usr/local/lib/irods)])],
 [AS_IF([test "x$with_irods" = "xno"],
        [AC_MSG_FAILURE([iRODS is required to build baton])],
        [test "x$with_irods" = "xyes"],
        [AC_MSG_NOTICE(Using default iRODS location)
         IRODS_HOME="/usr/local/lib/irods"
         IS_PACKAGED_INSTALL="no"],
        [IRODS_HOME="$with_irods"
         IS_PACKAGED_INSTALL="no"])],
 [IS_PACKAGED_INSTALL="yes"])

AC_SUBST(IRODS_HOME)

dnl Notes for OSX with homebrew:
dnl Ensure that check.m4 and pkg.m4 are in a system aclocal path or
dnl a path added by ACLOCAL_AMFLAGS in Makefile.am

dnl Unit tests using the check library
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4], [check_present=yes],
                  [check_present=no])
AM_CONDITIONAL(HAVE_CHECK, test "x$check_present" = "xyes")

dnl Sphinx documentation build (HTML manual and manpage)
AC_ARG_VAR([SPHINXBUILD], [Sphinx documentation builder])
AC_ARG_VAR([LCOV], [lcov code coverage analysis tool])
AC_ARG_VAR([GENHTML], [lcov HTML reporting tool])

AC_PATH_PROG([SPHINXBUILD], sphinx-build, no)
 if test "x${SPHINXBUILD}" = "xno"; then
   AC_MSG_WARN([
-----------------------------------------------
Unable to find sphinx-build on this system. The
HTML manual and manpage will not be created.
-----------------------------------------------])
fi

AM_CONDITIONAL(HAVE_SPHINX, [test "x${SPHINXBUILD}" != "xno"])

dnl Lcov code coverage analysis
AC_ARG_ENABLE([coverage],
  [AS_HELP_STRING([--enable-coverage],
    [Enable code coverage analysis (default is no)])],
[coverage_enabled=${enableval}], [coverage_enabled=no])

if test "x${coverage_enabled}" = "xyes"; then
  AC_CHECK_LIB(gcov, __gcov_init, [], [])

  AC_PATH_PROG([LCOV], lcov, no)
  if test "x${LCOV}" = "xno"; then
    AC_MSG_ERROR([Could not find lcov, necessary for coverage analysis.])
  fi

  AC_PATH_PROG([GENHTML], genhtml, no)
  if test "x${GENHTML}" = "xno"; then
    AC_MSG_ERROR([Could not find genhtml, necessary for coverage reporting.])
  fi
fi

AM_CONDITIONAL(COVERAGE_ENABLED, [test "x${coverage_enabled}" = "xyes"])

AC_CHECK_LIB([m], [log10])
AC_CHECK_LIB([pthread], [pthread_kill])
dnl librt exists on Linux, but not OSX
AC_CHECK_LIB([rt], [timer_create])

dnl Save the original values to restore between tests
CPPFLAGS_ORIG=${CPPFLAGS}
LDFLAGS_ORIG=${LDFLAGS}
LIBS_ORIG=${LIBS}

dnl Begin test for iRODS 3.3.x (independent of test for iRODS 4.x)

dnl iRODS 3.3.x supports only run-in-place (RIP) installation.  Its
dnl headers and shared libraries are in non-standard places, which may
dnl be determined relative to IRODS_HOME.
IRODS3_CPPFLAGS=\
"-I${IRODS_HOME}/lib/api/include \
-I${IRODS_HOME}/lib/core/include \
-I${IRODS_HOME}/lib/md5/include \
-I${IRODS_HOME}/lib/sha1/include \
-I${IRODS_HOME}/server/core/include \
-I${IRODS_HOME}/server/drivers/include \
-I${IRODS_HOME}/server/icat/include \
-I${IRODS_HOME}/server/re/include"
IRODS3_LDFLAGS="-L${IRODS_HOME}/lib/core/obj"

HAVE_IRODS3=
AS_IF([test "x$IS_PACKAGED_INSTALL" = "xno"],
      [CPPFLAGS="$CPPFLAGS_ORIG ${IRODS3_CPPFLAGS}"
       LDFLAGS="$LDFLAGS_ORIG ${IRODS3_LDFLAGS}"
       LIBS="${LIBS_ORIG}"
       AC_CHECK_LIB([RodsAPIs], [getRodsEnvFileName],
                    [AC_DEFINE([HAVE_IRODS3], [], [iRODS 3.3.x])
                    [HAVE_IRODS3=yes]
                    [LIBS="-lRodsAPIs $LIBS"]],
                    [HAVE_IRODS3=no],
                    [-lpthread -lgssapi_krb5])],
      [AC_MSG_NOTICE([Skipping test for iRODS 3.3x for packaged install])])

AM_CONDITIONAL(HAVE_IRODS3, test "x$HAVE_IRODS3" = "xyes")

IRODS3_LIBS="${LIBS}"

CPPFLAGS="${CPPFLAGS_ORIG}"
LDFLAGS="${LDFLAGS_ORIG}"
LIBS="${LIBS_ORIG}"
dnl End test for iRODS 3.3.x

dnl Begin test for iRODS 4.x (independent of test for iRODS 3.3.x)

dnl Check for Boost (required by iRODS 4.x). These macros do not have
dnl a non-fatal Boost detection mode, so they cause a requirement for
dnl Boost in configuring baton for iRODS 3.3.x too. However, no Boost
dnl components are used in the build for 3.3.x. A workaround to avoid
dnl configure looking for Boost in these circumstances is to comment
dnl out the following 4 lines.
BOOST_REQUIRE([1.46.0])
BOOST_THREAD
BOOST_FILESYSTEM
BOOST_REGEX

dnl iRODS 4.x supports both packaged (PKG) and run-in-place (RIP)
dnl installation.  Its headers and shared libraries may be in in /usr
dnl (PKG) or in non-standard places which may be determined relative
dnl to IRODS_HOME (RIP).
IRODS4_RIP_CPPFLAGS=\
"-I${IRODS_HOME}/lib/api/include \
-I${IRODS_HOME}/lib/core/include \
-I${IRODS_HOME}/lib/md5/include \
-I${IRODS_HOME}/lib/sha1/include \
-I${IRODS_HOME}/server/core/include \
-I${IRODS_HOME}/server/drivers/include \
-I${IRODS_HOME}/server/icat/include \
-I${IRODS_HOME}/server/re/include"
IRODS4_RIP_LDFLAGS="-L${IRODS_HOME}/lib/core/obj"
IRODS4_RIP_CHK_LIBS="-rdynamic -lpthread -ldl -lgssapi_krb5 -lm ${BOOST_THREAD_LIBS} ${BOOST_FILESYSTEM_LIBS} ${BOOST_REGEX_LIBS} -lssl -lcrypto -lstdc++"

IRODS4_PKG_CPPFLAGS="-I/usr/include/irods"
IRODS4_PKG_LDFLAGS="-L/usr/lib/irods"
IRODS4_PKG_CHK_LIBS="-rdynamic -lpthread -ldl -lgssapi_krb5 -lm -lboost_system -lboost_filesystem -lboost_regex -lboost_thread -lssl -lcrypto -lstdc++"

IRODS4_CPPFLAGS=
IRODS4_LDFLAGS=
AS_IF([test "x$IS_PACKAGED_INSTALL" = "xyes"],
      [AC_MSG_NOTICE(Checking the packaged iRODS 4.x install)
       IRODS4_LDFLAGS="${IRODS4_PKG_LDFLAGS}"
       IRODS4_CPPFLAGS="${IRODS4_PKG_CPPFLAGS}"
       IRODS4_LIBS="${IRODS4_PKG_CHK_LIBS}"]
      [test "x$IS_PACKAGED_INSTALL" = "xno"],
      [AC_MSG_NOTICE(Checking the run-in-place iRODS 4.x install)
       IRODS4_CPPFLAGS="${IRODS4_RIP_CPPFLAGS}"
       IRODS4_LDFLAGS="${IRODS4_RIP_LDFLAGS}"
       IRODS4_LIBS="${IRODS4_RIP_CHK_LIBS}"])

CPPFLAGS="$CPPFLAGS_ORIG ${IRODS4_CPPFLAGS}"
LDFLAGS="$LDFLAGS_ORIG ${IRODS4_LDFLAGS}"
LIBS="${LIBS_ORIG}"

HAVE_IRODS4=
AC_CHECK_LIB([RodsAPIs], [init_client_api_table],
             [AC_DEFINE([HAVE_IRODS4], [], [iRODS 4.x.x])
                       [HAVE_IRODS4=yes]
                       [LIBS="-lRodsAPIs $LIBS"]],
             [HAVE_IRODS4=no],
             ["$IRODS4_LIBS"])

AM_CONDITIONAL(HAVE_IRODS4, test "x$HAVE_IRODS4" = "xyes")

IRODS4_LIBS="${LIBS}"

CPPFLAGS="${CPPFLAGS_ORIG}"
LDFLAGS="${LDFLAGS_ORIG}"
LIBS="${LIBS_ORIG}"
dnl End test for iRODS 4.x

dnl Examine the test results
AS_IF([test "x$IS_PACKAGED_INSTALL" = "xyes" && test "x$HAVE_IRODS4" = "xno"],
      [AC_MSG_ERROR([failed to detect the packaged iRODS 4.x install])],
      [])

AS_IF([test "x$HAVE_IRODS3" = "xno" && test "x$HAVE_IRODS4" = "xno"],
      [AC_MSG_ERROR([failed to detect iRODS 3.3.x or 4.x.x in ${IRODS_HOME}])],
      [])

AS_IF([test "x$HAVE_IRODS3" = "xyes" && test "x$HAVE_IRODS4" = "xyes"],
      [AC_MSG_ERROR([detected both iRODS 3.3.x and 4.x.x in ${IRODS_HOME}])],
      [])

AS_IF([test "x$HAVE_IRODS3" = "xyes"],
      [AC_MSG_NOTICE([detected an iRODS 3.3.x installation at ${IRODS_HOME}])
       CPPFLAGS="${IRODS3_CPPFLAGS}"
       LDFLAGS="${IRODS3_LDFLAGS}"
       LDLIBS="${IRODS3_LIBS}"
       LIBS="${IRODS3_LIBS}"],
      [])

AS_IF([test "x$HAVE_IRODS4" = "xyes"],
      [AC_MSG_NOTICE([detected an iRODS 4.x installation at ${IRODS_HOME}])
       CPPFLAGS="${IRODS4_CPPFLAGS}"
       LDFLAGS="${IRODS4_LDFLAGS}"
       LDLIBS="${IRODS4_LIBS}"
       LIBS="${IRODS4_LIBS}"
       AC_CHECK_LIB([crypto], [EVP_EncryptUpdate], [],
        [AC_MSG_ERROR([unable to find the required library libcrypto])])
       AC_CHECK_LIB([ssl], [SSL_get_error], [],
         [AC_MSG_ERROR([unable to find the required library libssl])])],
      [])

AC_CHECK_LIB([gssapi_krb5], [gss_acquire_cred])
AC_CHECK_LIB([jansson], [json_unpack], [],
  [AC_MSG_ERROR([unable to find the required library libjannson])])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/baton.h])
AC_CONFIG_FILES([baton.pc Makefile src/Makefile tests/Makefile doc/Makefile])
AC_OUTPUT
