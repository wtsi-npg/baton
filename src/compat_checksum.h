/**
 * Copyright (C) 2015, 2016, 2023 Genome Research Ltd. All rights reserved.
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

#ifndef _BATON_COMPAT_CHECKSUM_H
#define _BATON_COMPAT_CHECKSUM_H

#include <rodsVersion.h>
#include <openssl/opensslv.h>

#include "config.h"
#include "error.h"

// OpenSSL 1.0
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define MD5_NEW EVP_MD_CTX_create
#define MD5_FREE EVP_MD_CTX_destroy
#else
#define MD5_NEW EVP_MD_CTX_new
#define MD5_FREE EVP_MD_CTX_free
#endif

#include <openssl/evp.h>

EVP_MD_CTX *compat_MD5Init(baton_error_t *error);

void compat_MD5Update(EVP_MD_CTX *context, unsigned char *input, unsigned int len,
                      baton_error_t *error);

void compat_MD5Final(unsigned char digest[16], EVP_MD_CTX *context,
                     baton_error_t *error);

#endif // _BATON_COMPAT_CHECKSUM_H
