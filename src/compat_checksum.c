/**
 * Copyright (C) 2015, 2023 Genome Research Ltd. All rights reserved.
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
 * @file compat_checksum.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

#include "compat_checksum.h"

EVP_MD_CTX* compat_MD5Init(baton_error_t *error) {
    const EVP_MD *md = EVP_md5();

    EVP_MD_CTX *context = MD5_NEW();
    if (context == NULL) {
         set_baton_error(error, -1, "Failed to create an MD5 context");
         return NULL;
    }

    if (!EVP_DigestInit_ex(context, md, NULL)) {
        MD5_FREE(context);
        set_baton_error(error, -1, "Failed to initialize an MD5 context");
        return NULL;
    }

    return context;
}

void compat_MD5Update(EVP_MD_CTX *context, unsigned char *input,
                      unsigned int len, baton_error_t *error) {
    if (!EVP_DigestUpdate(context, input, len)) {
        MD5_FREE(context);
        set_baton_error(error, -1, "Failed to update an MD5 context");
    }
}

void compat_MD5Final(unsigned char digest[16], EVP_MD_CTX *context,
                    baton_error_t *error) {
    uint len = 16;
    if (!EVP_DigestFinal_ex(context, digest, &len)) {
        MD5_FREE(context);
        set_baton_error(error, -1, "Failed to finalise an MD5 context");
    }
}
