#
# SYNOPSIS
#
#   AX_WITH_IRODS()
#
# DESCRIPTION
#
#   This macro searches for the header files and libraries of an iRODS
#   (https://irods.org) installation.  If --with-irods is specified
#   without argument, or as --with-irods=yes, a packaged (.deb or
#   .rpm) installation is assumed and the default system paths will be
#   searched.  If --with-irods=DIR is specified, a run-in-place iRODS
#   installation will be searched for in DIR.  If --without-irods is
#   specified, any iRODS installations present will be ignored.
#
#   The system header and library paths will be used for run-in-place
#   iRODS installation dependencies, in preference to the those
#   dependencies provided by iRODS in its 'externals' directory
#   because the latter cannot be determined reliably.
#
#   The iRODS versions are supported are 3.3.x and >= 4.1.8.
#
#   The macro defines the symbol HAVE_IRODS if the library is found
#   and the following output variables are set with AC_SUBST:
#
#     IRODS_CPPFLAGS
#     IRODS_LDFLAGS
#     IRODS_LIBS
#
#   You can use them like this in Makefile.am:
#
#      AM_CPPFLAGS       = $(IRODS_CPPFLAGS)
#      AM_LDFLAGS        = $(IRODS_LDFLAGS)
#      library_la_LIBADD = $(IRODS_LIBS)
#      program_LDADD     = $(IRODS_LIBS)
#
# LICENSE
#
# Copyright (C) 2016, Genome Research Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

AC_DEFUN([AX_WITH_IRODS], [
   IRODS_HOME=
   Irods_CPPFLAGS=
   IRODS_LDFLAGS=
   IRODS_LIBS=

   saved_CPPFLAGS="$CPPFLAGS"
   saved_LDFLAGS="$LDFLAGS"
   saved_LIBS="$LIBS"

   AC_ARG_WITH([irods],
       [AS_HELP_STRING([--with-irods[[=DIR]]], [Enable iRODS support])], [
           AS_IF([test "x$with_irods" != "xno"], [

               AC_CHECK_LIB([rt], [timer_create])
               AC_CHECK_LIB([m], [log10])
               AC_CHECK_LIB([dl], [dlopen])
               AC_CHECK_LIB([pthread], [pthread_kill])
               AC_CHECK_LIB([gssapi_krb5], [gss_acquire_cred])
               AC_CHECK_LIB([jansson], [json_unpack], [],
                  [AC_MSG_ERROR([unable to find libjannson])])

               IRODS4_LIBS="-lboost_program_options -lboost_filesystem \
-lboost_regex -lboost_chrono -lboost_thread -lboost_system"

               AS_IF([test "x$with_irods" = "xyes"], [
                   AC_MSG_CHECKING([for packaged iRODS])

                   CPPFLAGS="-I/usr/include/irods $CPPFLAGS"
                   LDFLAGS="-L/usr/lib/irods/externals $LDFLAGS"

                   AC_RUN_IFELSE([AC_LANG_PROGRAM([
                        #include <rodsVersion.h>
                      ], [
                        #if IRODS_VERSION_INTEGER >= 4001008
                        return 0;
                        #else
                        exit(-1);
                        #endif
                      ])
                   ], [
                       AC_MSG_RESULT([yes])
                       AC_DEFINE([HAVE_IRODS], [1], [iRODS >= 4.1.8])

                       LDFLAGS="$IRODS4_RIP_LDFLAGS $LDFLAGS"
                       LIBS="$IRODS4_LIBS $LIBS -lstdc++"

                       AC_CHECK_LIB([crypto], [EVP_EncryptUpdate], [],
                          [AC_MSG_ERROR([unable to find libcrypto])])

                       AC_CHECK_LIB([ssl], [SSL_get_error], [],
                          [AC_MSG_ERROR([unable to find libssl])])

                       AC_CHECK_LIB([RodsAPIs], [getRodsEnv], [],
                          [AC_MSG_ERROR([unable to find libRodsAPIs])],
                          [-lirods_client_plugins])

                       AC_CHECK_LIB([irods_client_plugins],
                          [operation_rule_execution_manager_factory], [],
                          [AC_MSG_ERROR([unable to find libirods_client_plugins])],
                          [-lRodsAPIs])
                   ], [
                       AC_MSG_RESULT([no])
                   ])
               ], [
                   IRODS_HOME="$with_irods"

                   IRODS_RIP_CPPFLAGS="-I$IRODS_HOME/lib/api/include \
-I$IRODS_HOME/lib/core/include -I$IRODS_HOME/lib/md5/include \
-I$IRODS_HOME/lib/sha1/include -I$IRODS_HOME/server/core/include \
-I$IRODS_HOME/server/drivers/include -I$IRODS_HOME/server/icat/include \
-I$IRODS_HOME/server/re/include"

                   IRODS3_RIP_LDFLAGS="-L$IRODS_HOME/lib/core/obj"
                   IRODS4_RIP_LDFLAGS="-L$IRODS_HOME/lib/core/obj \
-L$IRODS_HOME/lib/development_libraries"

                   CPPFLAGS="$IRODS_RIP_CPPFLAGS $CPPFLAGS"

                   AC_MSG_CHECKING([for iRODS 3.3.x])
                   AC_RUN_IFELSE([AC_LANG_PROGRAM([
                        #include <string.h>
                        #include <rodsVersion.h>
                      ], [
                        #if defined (RODS_REL_VERSION)
                        return strncmp("rods3.3", RODS_REL_VERSION, 7);
                        #else
                        exit(-1);
                        #endif
                      ])
                   ], [
                       AC_MSG_RESULT([yes])
                       AC_DEFINE([HAVE_IRODS], [1], [iRODS 3.3.x])

                       LDFLAGS="$IRODS3_RIP_LDFLAGS $LDFLAGS"

                       AC_CHECK_LIB([RodsAPIs], [getRodsEnv], [],
                          [AC_MSG_ERROR([unable to find libRodsAPIs])])
                   ], [
                       AC_MSG_RESULT([no])
                   ])

                   AC_MSG_CHECKING([for iRODS >= 4.1.8])
                   AC_RUN_IFELSE([AC_LANG_PROGRAM([
                        #include <rodsVersion.h>
                      ], [
                        #if IRODS_VERSION_INTEGER >= 4001008
                        return 0;
                        #else
                        exit(-1);
                        #endif
                      ])
                   ], [
                       AC_MSG_RESULT([yes])
                       AC_DEFINE([HAVE_IRODS], [1], [iRODS >= 4.1.8])

                       LDFLAGS="$IRODS4_RIP_LDFLAGS $LDFLAGS"
                       LIBS="$IRODS4_LIBS $LIBS -lstdc++"

                       AC_CHECK_LIB([crypto], [EVP_EncryptUpdate], [],
                          [AC_MSG_ERROR([unable to find libcrypto])])

                       AC_CHECK_LIB([ssl], [SSL_get_error], [],
                          [AC_MSG_ERROR([unable to find libssl])])

                       AC_CHECK_LIB([RodsAPIs], [getRodsEnv], [],
                          [AC_MSG_ERROR([unable to find libRodsAPIs])],
                          [-lirods_client_plugins])

                       AC_CHECK_LIB([irods_client_plugins],
                          [operation_rule_execution_manager_factory], [],
                          [AC_MSG_ERROR([unable to find libirods_client_plugins])],
                          [-lRodsAPIs])
                   ], [
                       AC_MSG_RESULT([no])
                   ])
               ])

               IRODS_CPPFLAGS="$CPPFLAGS"
               IRODS_LDFLAGS="$LDFLAGS"
               IRODS_LIBS="$LIBS"

               CPPFLAGS="$saved_CPPFLAGS"
               LDFALGS="$saved_LDFLAGS"
               LIBS="$saved_LIBS"

               unset saved_CPPFLAGS
               unset saved_LDFLAGS
               unset saved_LIBS

               AC_SUBST([IRODS_HOME])
               AC_SUBST([IRODS_CPPFLAGS])
               AC_SUBST([IRODS_LDFLAGS])
               AC_SUBST([IRODS_LIBS])
           ])
   ])
])
