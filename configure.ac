AC_INIT([baton], m4_esyscmd([git describe --dirty | tr -d '\012']), [kdj@sanger.ac.uk])

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

dnl Notes for OSX with homebrew:
dnl Ensure that check.m4 and pkg.m4 are in a system aclocal path or
dnl a path added by ACLOCAL_AMFLAGS in Makefile.am

dnl Packaged (.deb) versus source tree (run-in-place) installation.
IS_PACKAGED_INSTALL=no

IRODS_HOME=
AC_ARG_WITH([irods],
  [AS_HELP_STRING([--with-irods],
    [Specify the location of a run-in-place iRODS installation (default: /usr/local/lib/irods)])],
  [AS_IF(
    [test "x$with_irods" = "xno"],
      [AC_MSG_FAILURE([iRODS is required to build baton])],
    [test "x$with_irods" = "xyes"],
      [AC_MSG_NOTICE(Using default iRODS location)
        IRODS_HOME="/usr/local/lib/irods"
        IS_PACKAGED_INSTALL="no"],
     [IRODS_HOME="$with_irods"
      IS_PACKAGED_INSTALL="no"])],
  [IS_PACKAGED_INSTALL="yes"])

AC_SUBST(IRODS_HOME)

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
    [AC_MSG_WARN(unable to find sphinx-build on this system; the HTML manual and manpage will not be created)],
  [])

AM_CONDITIONAL(HAVE_SPHINX, [test "x${SPHINXBUILD}" != "xno"])
dnl End Sphinx

dnl Begin Lcov code coverage analysis
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

dnl librt exists on Linux, but not OSX
AC_CHECK_LIB([rt], [timer_create])
AC_CHECK_LIB([m], [log10])
AC_CHECK_LIB([pthread], [pthread_kill])
AC_CHECK_LIB([gssapi_krb5], [gss_acquire_cred])
AC_CHECK_LIB([jansson], [json_unpack], [],
  [AC_MSG_ERROR([failed to detect the required library libjannson])])

dnl Save the original values to restore between tests
CPPFLAGS_ORIG=${CPPFLAGS}
LDFLAGS_ORIG=${LDFLAGS}
LIBS_ORIG=${LIBS}

dnl Begin iRODS 3.3.x

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
                    [])],
      [AC_MSG_NOTICE([skipping test for iRODS 3.3x for packaged install])])

AM_CONDITIONAL(HAVE_IRODS3, test "x$HAVE_IRODS3" = "xyes")

IRODS3_LIBS="${LIBS}"

CPPFLAGS="${CPPFLAGS_ORIG}"
LDFLAGS="${LDFLAGS_ORIG}"
LIBS="${LIBS_ORIG}"
dnl End iRODS 3.3.x

dnl Begin iRODS 4.x
HAVE_IRODS4=
AS_IF(
  [test "x$HAVE_IRODS3" = "xyes"],
    [AC_MSG_NOTICE(iRODS 3.3.x detected)],
  [test "x$HAVE_IRODS3" = "no"],
    [AC_MSG_NOTICE(iRODS 3.3.x not detected, checking for 4.x)],
  [HAVE_IRODS4=no])

dnl iRODS 4.x supports both packaged (PKG) and run-in-place (RIP)
dnl installation.  Its headers and shared libraries may be in in /usr
dnl (PKG) or in non-standard places which may be determined relative
dnl to IRODS_HOME (RIP).  A problem with the RIP version is that
dnl apparently the client shared libraries are not built.
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
IRODS4_RIP_LIBS="-lRodsAPIs"

RIP_BOOST_VERSION=boost_1_57_0
IRODS4_RIP_BOOST_CPPFLAGS="-I${IRODS_HOME}/../external/${RIP_BOOST_VERSION}/boost"
IRODS4_RIP_BOOST_LDFLAGS="-L${IRODS_HOME}/../external/${RIP_BOOST_VERSION}/stage/lib"
IRODS4_RIP_BOOST_LIBS="-lboost_system -lboost_filesystem -lboost_regex -lboost_thread"

IRODS4_PKG_CPPFLAGS="-I/usr/include/irods"
IRODS4_PKG_LDFLAGS="-L/usr/lib/irods"
IRODS4_PKG_LIBS="-lrods_client_api -lrods_client"
IRODS4_PKG_BOOST_LIBS="-lboost_system -lboost_filesystem -lboost_regex -lboost_thread"

IRODS4_CPPFLAGS=
IRODS4_LDFLAGS=
IRODS4_BOOST_LIBS=

HAVE_IRODS4=
AS_IF(
  [test "x$IS_PACKAGED_INSTALL" = "xyes"],
    dnl Packed iRODS 4.x has irods_client_api and irods_client
    [AC_MSG_NOTICE(checking the packaged iRODS 4.x install)
     IRODS4_CPPFLAGS=${IRODS4_PKG_CPPFLAGS}
     IRODS4_LDFLAGS=${IRODS4_PKG_LDFLAGS}

     CPPFLAGS="${CPPFLAGS_ORIG} ${IRODS4_PKG_CPPFLAGS}"
     LDFLAGS="${LDFLAGS_ORIG} ${IRODS4_PKG_LDFLAGS}"

     AC_CHECK_LIB([crypto], [EVP_EncryptUpdate], [],
      [AC_MSG_ERROR([unable to find the required library libcrypto])])
     AC_CHECK_LIB([ssl], [SSL_get_error], [],
      [AC_MSG_ERROR([unable to find the required library libssl])])

     LIBS="${IRODS4_PKG_BOOST_LIBS} -lstdc++ $LIBS"

     AC_CHECK_LIB([irods_client_api], [api_pack_table_init],
       [],
       [],
       [-rdynamic -lirods_client -ldl])

     AC_CHECK_LIB([irods_client], [getRodsEnvFileName],
       [AC_DEFINE([HAVE_IRODS4], [], [iRODS 4.x.x])
        [HAVE_IRODS4=yes]
        [LIBS="-lirods_client $LIBS"]],
       [HAVE_IRODS4=no],
       [-rdynamic -ldl])]

  [test "x$IS_PACKAGED_INSTALL" = "xno"],
    dnl Run-in-place iRODS 4.x has only RodsAPIs
    [AC_MSG_NOTICE(checking for a run-in-place iRODS 4.x install)
     IRODS4_CPPFLAGS="${IRODS4_RIP_CPPFLAGS} ${IRODS4_RIP_BOOST_CPPFLAGS}"
     IRODS4_LDFLAGS="${IRODS4_RIP_LDFLAGS} ${IRODS4_RIP_BOOST_LDFLAGS}"

     CPPFLAGS="${CPPFLAGS_ORIG} ${IRODS4_RIP_CPPFLAGS}"
     LDFLAGS="${LDFLAGS_ORIG} ${IRODS4_RIP_LDFLAGS}"

     AC_CHECK_LIB([crypto], [EVP_EncryptUpdate], [],
      [AC_MSG_ERROR([unable to find the required library libcrypto])])
     AC_CHECK_LIB([ssl], [SSL_get_error], [],
      [AC_MSG_ERROR([unable to find the required library libssl])])

     CPPFLAGS="${IRODS4_RIP_BOOST_CPPFLAGS} $CPPFLAGS"
     LDFLAGS="${IRODS4_RIP_BOOST_LDFLAGS} $LDFLAGS"
     LIBS="${IRODS4_RIP_BOOST_LIBS} -lstdc++ $LIBS"

     AC_CHECK_LIB([RodsAPIs], [getRodsEnv],
       [AC_DEFINE([HAVE_IRODS4], [], [iRODS 4.x.x])
	[HAVE_IRODS4=yes]
        [LIBS="-lRodsAPIs $LIBS"]],
       [HAVE_IRODS4=no],
       [-ldl])])

AS_IF(
  [test "x$HAVE_IRODS4" = "xyes"],
    [AC_CHECK_LIB([crypto], [EVP_EncryptUpdate], [],
      [AC_MSG_ERROR([unable to find the required library libcrypto])])
     AC_CHECK_LIB([ssl], [SSL_get_error], [],
      [AC_MSG_ERROR([unable to find the required library libssl])])],
  [])

AM_CONDITIONAL(HAVE_IRODS4, test "x$HAVE_IRODS4" = "xyes")

IRODS4_LIBS="${LIBS}"

CPPFLAGS="${CPPFLAGS_ORIG}"
LDFLAGS="${LDFLAGS_ORIG}"
LIBS="${LIBS_ORIG}"
dnl End test for iRODS 4.x

dnl Examine the test results
AS_IF(
  [test "x$IS_PACKAGED_INSTALL" = "xyes" && test "x$HAVE_IRODS4" = "xno"],
    [AC_MSG_ERROR([failed to detect the packaged iRODS 4.x install])],
  [])

AS_IF(
  [test "x$HAVE_IRODS3" = "xno" && test "x$HAVE_IRODS4" = "xno"],
    [AC_MSG_ERROR([failed to detect iRODS 3.3.x or 4.x.x in ${IRODS_HOME}])],
  [])

AS_IF(
  [test "x$HAVE_IRODS3" = "xyes" && test "x$HAVE_IRODS4" = "xyes"],
    [AC_MSG_ERROR([detected both iRODS 3.3.x and 4.x.x in ${IRODS_HOME}])],
  [])

AS_IF(
  [test "x$HAVE_IRODS3" = "xyes"],
    [AC_MSG_NOTICE([detected an iRODS 3.3.x installation in ${IRODS_HOME}])
     CPPFLAGS="${IRODS3_CPPFLAGS}"
     LDFLAGS="${IRODS3_LDFLAGS}"
     LDLIBS="${IRODS3_LIBS}"
     LIBS="${IRODS3_LIBS}"],
  [])

AS_IF(
  [test "x$HAVE_IRODS4" = "xyes"],
    [AC_MSG_NOTICE([detected an iRODS 4.x installation])
     CPPFLAGS="${IRODS4_CPPFLAGS}"
     LDFLAGS="${IRODS4_LDFLAGS}"
     LDLIBS="${IRODS4_LIBS}"
     LIBS="${IRODS4_LIBS}"],
  [])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/baton.h])
AC_CONFIG_FILES([baton.pc Makefile src/Makefile tests/Makefile doc/Makefile])
AC_OUTPUT
