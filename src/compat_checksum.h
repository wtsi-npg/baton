/**
 * Copyright (C) 2015, 2016 Genome Research Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @file compat_checksum.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include <rodsVersion.h>

#include "config.h"

#if IRODS_VERSION_INTEGER && IRODS_VERSION_INTEGER >= 4001008
#include <openssl/md5.h>
#else
#include "md5Checksum.h"
#endif

// iRODS 3.x shares the same checksum API as iRODS 4.0.x, while 4.1.x
// uses openssl directly. Backwards compatibility was broken for the
// iRODS checksum API in 4.1.0. Since baton needs to continue support
// for both 3.3.1 and 4.1.x this compatability shim was introduced and
// baton's support for iRODS 4.0.x discontinued. It's a long-winded way
// of doing things, but it's very clear what's going on.

// MD5_CTX is a symbol that clashes between iRODS 3.x/4.0.x MD5 and
// OpenSSL MD5. We just use whichever is present, which the ifdefs
// ensure will be the correct one.

void compat_MD5Init(MD5_CTX *context);

void compat_MD5Update(MD5_CTX *context, unsigned char *input, unsigned int len);

void compat_MD5Final(unsigned char digest[16], MD5_CTX *context);
