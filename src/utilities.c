/**
 * Copyright (c) 2013-2014 Genome Research Ltd. All rights reserved.
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
 * @file utilities.c
 * @author Keith James <kdj@sanger.ac.uk>
 */

// For strptime
#include <config.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "utilities.h"

char *copy_str(const char *str, size_t max_len) {
    size_t term_len = strnlen(str, max_len) + 1;
    char *copy = NULL;

    if (term_len > MAX_STR_LEN) {
        logmsg(ERROR, "Failed to allocate a string of length %d: "
               "it exceeded the maximum length of %d characters",
               term_len, MAX_STR_LEN);
        goto error;
    }

    copy = calloc(term_len, sizeof (char));
    if (!copy) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
    }

    snprintf(copy, term_len, "%s", str);

    return copy;

error:
    logmsg(ERROR, "Failed to allocate memory: error %d %s",
           errno, strerror(errno));

    return NULL;
}

int str_starts_with(const char *str, const char *prefix, size_t max_len) {
    if (!str || !prefix) return 0;

    size_t len  = strnlen(str,    max_len);
    size_t plen = strnlen(prefix, max_len);

    // A string always starts with the empty string
    if (plen == 0)  return 1;
    if (plen > len) return 0;

    return strncmp(str, prefix, plen) == 0;
}

int str_equals(const char *str1, const char *str2, size_t max_len) {
    size_t len1 = strnlen(str1, max_len);
    size_t len2 = strnlen(str2, max_len);
    return len1 == len2 && (strncmp(str1, str2, len2) == 0);
}

int str_equals_ignore_case(const char *str1, const char *str2, size_t max_len) {
    size_t len1 = strnlen(str1, max_len);
    size_t len2 = strnlen(str2, max_len);
    return len1 == len2 && (strncasecmp(str1, str2, max_len) == 0);
}

int str_ends_with(const char *str, const char *suffix, size_t max_len) {
    if (!str || !suffix) return 0;

    size_t len  = strnlen(str,    max_len);
    size_t slen = strnlen(suffix, max_len);

    // A string always ends with the empty string
    if (slen == 0)  return 1;
    if (slen > len) return 0;

    return strncmp(str + (len - slen), suffix, len) == 0;
}

const char *parse_base_name(const char *path) {
    const char delim = '/';

    const char *base_name = strrchr(path, delim);

    // Base name should not include the '/'
    if (base_name) {
        base_name++;
    }
    else {
        base_name = path;
    }

    return base_name;
}

FILE *maybe_stdin(const char *path) {
    FILE *stream;

    if (path) {
        stream = fopen(path, "r");
        if (!stream) goto error;
    }
    else {
        stream = stdin;
    }

    return stream;

error:
    logmsg(ERROR, "Failed to open '%s': error %d %s",
           path, errno, strerror(errno));

    return NULL;
}

char *format_timestamp(const char *raw_timestamp, const char *format) {
    size_t output_len = 32;
    char *output = NULL;
    int base = 10;

    struct tm tm;
    time_t time;
    int status;

    output = calloc(output_len, sizeof (char));
    if (!output) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    errno = 0;
    time = strtoul(raw_timestamp, NULL, base);
    if (errno != 0) {
        logmsg(ERROR, "Failed to convert timestamp '%s' to a number: "
               "error %d %s", raw_timestamp, errno, strerror(errno));
        goto error;
    }

    gmtime_r(&time, &tm);

    status = strftime(output, output_len, format, &tm);
    if (status == 0) {
        logmsg(ERROR, "Failed to format timestamp '%s' as an ISO date time: "
               "error %d %s", raw_timestamp, errno, strerror(errno));
        goto error;
    }

    logmsg(DEBUG,"Converted timestamp '%s' to '%s'", raw_timestamp, output);

    return output;

error:
    if (output) free(output);

    return NULL;
}

char *parse_timestamp(const char *timestamp, const char *format) {
    size_t output_len = 32;
    char *output = NULL;
    char *rest;

    struct tm tm;
    time_t time;

    output = calloc(output_len, sizeof (char));
    if (!output) {
        logmsg(ERROR, "Failed to allocate memory: error %d %s",
               errno, strerror(errno));
        goto error;
    }

    rest = strptime(timestamp, format, &tm);
    if (!rest) {
        logmsg(ERROR, "Failed to parse ISO date time '%s'", timestamp);
        goto error;
    }

    time = timegm(&tm);
    snprintf(output, output_len, "%ld", time);

    logmsg(DEBUG, "Parsed timestamp '%s' to '%ld'", timestamp, time);

    return output;

error:
    if (output) free(output);

    return NULL;
}

size_t to_utf8(const char *input, char *output, size_t max_len) {
    size_t len = strnlen(input, max_len);

    const unsigned char *bytes = (const unsigned char *) input;
    char *op = output;

    // In Latin-1, the numeric values of the encoding are equal to the
    // first 256 Unicode codepoints

    // http://www.ietf.org/rfc/rfc3629.txt, Section 3. for encoding.

    for (size_t i = 0; i < len; i++) {
        if (bytes[i] < 0x80) {
            *op++ = bytes[i];
        }
        else {
            *op++ = 0xc0 | (bytes[i] & 0xc0) >> 6;
            *op++ = 0x80 | (bytes[i] & 0x3f);
        }
    }

    return len;
}

int maybe_utf8 (const char *str, size_t max_len) {
    // http://www.rfc-editor.org/rfc/rfc3629.txt, Section 4. for the syntax
    // of UTF-8 byte sequences.
    //
    // UTF8-octets = *( UTF8-char )
    // UTF8-char   = UTF8-1 / UTF8-2 / UTF8-3 / UTF8-4
    // UTF8-tail   = %x80-BF

    size_t len = strnlen(str, max_len);
    size_t i   = 0;

    const unsigned char *bytes = (const unsigned char *) str;

    while (i < len) {
        // UTF8-1 = %x00-7F
        // Includes 0x7f DEL and other control characters
        if (bytes[i + 0] <= 0x7F) {
            i += 1;
            continue;
        }

        // UTF8-2 = %xC2-DF UTF8-tail
        if ((bytes[i + 0] >= 0xc2) && (bytes[i + 0] <= 0xdf) &&
            (bytes[i + 1] >= 0x80) && (bytes[i + 1] <= 0xbf)) {
            i += 2;
            continue;
        }

        // UTF8-3 = %xE0 %xA0-BF UTF8-tail / %xE1-EC 2( UTF8-tail ) /
        //          %xED %x80-9F UTF8-tail / %xEE-EF 2( UTF8-tail )
        if (// %xE0 %xA0-BF UTF8-tail
            ((bytes[i + 0] == 0xe0)                             &&
             ((bytes[i + 1] >= 0xa0) && (bytes[i + 1] <= 0xbf)) &&
             ((bytes[i + 2] >= 0x80) && (bytes[i + 2] <= 0xbf)))
            ||
            // %xE1-EC 2( UTF8-tail )
            (((bytes[i + 0] >= 0xe1) && (bytes[i + 0] <= 0xec)) &&
             ((bytes[i + 1] >= 0x80) && (bytes[i + 1] <= 0xbf)) &&
             ((bytes[i + 2] >= 0x80) && (bytes[i + 2] <= 0xbf)))
            ||
            // %xED %x80-9F UTF8-tail
            ((bytes[i + 0] == 0xed)                             &&
             ((bytes[i + 1] >= 0x80) && (bytes[i + 1] <= 0x9f)) &&
             ((bytes[i + 2] >= 0x80) && (bytes[i + 2] <= 0xbf)))
            ||
            // %xEE-EF 2( UTF8-tail )
            (((bytes[i + 0] >= 0xee) && (bytes[i + 0] <= 0xef)) &&
             ((bytes[i + 1] >= 0x80) && (bytes[i + 1] <= 0xbf)) &&
             ((bytes[i + 2] >= 0x80) && (bytes[i + 2] <= 0xbf)))) {
            i += 3;
            continue;
         }

        // UTF8-4 = %xF0 %x90-BF 2( UTF8-tail ) / %xF1-F3 3( UTF8-tail ) /
        //          %xF4 %x80-8F 2( UTF8-tail )
        if (// %xF0 %x90-BF 2( UTF8-tail )
            ((bytes[i + 0] == 0xf0)                             &&
             ((bytes[i + 1] >= 0x90) && (bytes[i + 1] <= 0xbf)) &&
             ((bytes[i + 2] >= 0x80) && (bytes[i + 2] <= 0xbf)) &&
             ((bytes[i + 3] >= 0x80) && (bytes[i + 3] <= 0xbf)))
            ||
            // %xF1-F3 3( UTF8-tail )
            (((bytes[i + 0] >= 0xf1) && (bytes[i + 0] <= 0xf3)) &&
             ((bytes[i + 1] >= 0x80) && (bytes[i + 1] <= 0xbf)) &&
             ((bytes[i + 2] >= 0x80) && (bytes[i + 2] <= 0xbf)) &&
             ((bytes[i + 3] >= 0x80) && (bytes[i + 3] <= 0xbf)))
            ||
            // %xF4 %x80-8F 2( UTF8-tail )
            ((bytes[i + 0] == 0xf4)                             &&
             ((bytes[i + 1] >= 0x80) && (bytes[i + 1] <= 0x8f)) &&
             ((bytes[i + 2] >= 0x80) && (bytes[i + 2] <= 0xbf)) &&
             ((bytes[i + 3] >= 0x80) && (bytes[i + 3] <= 0xbf)))) {
            i += 4;
            continue;
        }

        return 0;
    }

    return 1;
}
