#
# SYNOPSIS
#
#   AX_WITH_IRODS()
#
# DESCRIPTION
#
#   This macro searches for the header files and libraries of an iRODS
#   (https://irods.org) installation. To use, add to configure.ac
#
#     AX_WITH_IRODS
#
#   If your iRODS header files and libraries are in a non-standard
#   location, you will need to set CPPFLAGS and LDFLAGS appropriately.
#
#   The iRODS versions are supported are >= 4.1.8 and >= 4.2.x
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
# Copyright (C) 2016, 2019 Genome Research Ltd.
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
   IRODS_CPPFLAGS=
   IRODS_LDFLAGS=
   IRODS_LIBS=

   irods_4_1_found=
   irods_4_2_found=

   saved_CPPFLAGS="$CPPFLAGS"
   saved_LDFLAGS="$LDFLAGS"
   saved_LIBS="$LIBS"

   AC_CHECK_LIB([rt], [timer_create])
   AC_CHECK_LIB([m], [log10])
   AC_CHECK_LIB([dl], [dlopen])
   AC_CHECK_LIB([pthread], [pthread_kill])
   AC_CHECK_LIB([gssapi_krb5], [gss_acquire_cred])

   AC_CHECK_LIB([crypto], [EVP_EncryptUpdate], [],
                [AC_MSG_ERROR([unable to find libcrypto])])
   AC_CHECK_LIB([ssl], [SSL_get_error], [],
                [AC_MSG_ERROR([unable to find libssl])])

   AC_MSG_CHECKING([for iRODS >=3.3.x])
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
      ],
      [AC_MSG_ERROR([iRODS 3.x series is no longer supported])],
      AC_MSG_RESULT([no]))

   CPPFLAGS="$saved_CPPFLAGS"
   LDFLAGS="$saved_LDFLAGS"
   LIBS="$saved_LIBS"

   AC_MSG_CHECKING([for iRODS >=4.1.8, <4.2.x])

   # This is a non-standard location used by the RENCI iRODS 4.1 package
   LDFLAGS="-L/usr/lib/irods/externals $LDFLAGS"
   LIBS="-lstdc++ -ljansson \
         -lboost_program_options -lboost_filesystem \
         -lboost_regex -lboost_chrono -lboost_thread -lboost_system \
         $LIBS"

   AC_RUN_IFELSE([AC_LANG_PROGRAM([
          #include <rodsVersion.h>
        ], [
          #if   IRODS_VERSION_INTEGER >= 4002000
          exit(-1)
          #elif IRODS_VERSION_INTEGER >= 4001008
          return 0;
          #else
          exit(-1);
          #endif
        ])
      ], [
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_IRODS], [1], [iRODS >=4.1.8, <4.2.x])

        AC_CHECK_LIB([RodsAPIs], [getRodsEnv], [],
                     [AC_MSG_ERROR([unable to find libRodsAPIs])],
                     [-lirods_client_plugins])

        AC_CHECK_LIB([irods_client_plugins],
                     [operation_rule_execution_manager_factory], [],
                     [AC_MSG_ERROR([unable to find libirods_client_plugins])],
                     [-lRodsAPIs])

        IRODS_CPPFLAGS="$CPPFLAGS"
        IRODS_LDFLAGS="$LDFLAGS"
        IRODS_LIBS="$LIBS"
        irods_4_1_found="yes"
      ],
      AC_MSG_RESULT([no]))

   CPPFLAGS="$saved_CPPFLAGS"
   LDFLAGS="$saved_LDFLAGS"
   LIBS="$saved_LIBS"

   AC_MSG_CHECKING([for iRODS >=4.2.x])

   LIBS="-lstdc++ $LIBS"

   AC_RUN_IFELSE([AC_LANG_PROGRAM([
           #include <rodsVersion.h>
         ], [
           #if IRODS_VERSION_INTEGER >= 4002000
           return 0
           #else
           exit(-1);
           #endif
         ])
       ], [
         AC_MSG_RESULT([yes])
         AC_DEFINE([HAVE_IRODS], [1], [iRODS >=4.2.x])

         AC_CHECK_LIB([irods_client], [getRodsEnv], [],
                     [AC_MSG_ERROR([unable to find libirods_client])],
                     [-lirods_common])

         IRODS_CPPFLAGS="$CPPFLAGS"
         IRODS_LDFLAGS="$LDFLAGS"
         IRODS_LIBS="$LIBS"
         irods_4_2_found="yes"
       ],
       AC_MSG_RESULT([no]))

   CPPFLAGS="$saved_CPPFLAGS"
   LDFLAGS="$saved_LDFLAGS"
   LIBS="$saved_LIBS"

   unset saved_CPPFLAGS
   unset saved_LDFLAGS
   unset saved_LIBS

   # Exit if iRODS is not found
   if test "$irods_4_1_found" != "yes" && test "$irods_4_2_found" != "yes"; then
     AC_MSG_ERROR([unable to find iRODS 4.1 or 4.2])
   fi

   # Use iRODS 4.2 if somehow both are present
   if test "$irods_4_1_found" = "yes" && test "$irods_4_2_found" = "yes"; then
     AC_MSG_WARN([Found both iRODS 4.1 and 4.2; using 4.2])
   fi

   AC_SUBST([IRODS_CPPFLAGS])
   AC_SUBST([IRODS_LDFLAGS])
   AC_SUBST([IRODS_LIBS])
])
