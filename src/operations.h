/**
 * Copyright (C) 2013, 2014, 2015, 2016, 2017 Genome Research Ltd. All
 * rights reserved.
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
 * @file operations.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_OPERATIONS_H
#define _BATON_OPERATIONS_H

/**
 *  @enum metadata_op
 *  @brief AVU metadata operations.
 */
typedef enum {
    /** Add an AVU. */
    META_ADD,
    /** Remove an AVU. */
    META_REM,
    /** Query AVUs */
    META_QUERY
} metadata_op;

typedef enum {
    /** Non-recursive operation */
    NO_RECURSE,
    /** Recursive operation */
    RECURSE
} recursive_op;

typedef enum {
    /** Print AVUs on collections and data objects */
    PRINT_AVU          = 1 << 0,
    /** Print ACLs on collections and data objects */
    PRINT_ACL          = 1 << 1,
    /** Print the contents of collections */
    PRINT_CONTENTS     = 1 << 2,
    /** Print timestamps on collections and data objects */
    PRINT_TIMESTAMP    = 1 << 3,
    /** Print file sizes for data objects */
    PRINT_SIZE         = 1 << 4,
    /** Pretty-print JSON */
    PRINT_PRETTY       = 1 << 5,
    /** Print raw output */
    PRINT_RAW          = 1 << 6,
    /** Search collection AVUs */
    SEARCH_COLLECTIONS = 1 << 7,
    /** Search data object AVUs */
    SEARCH_OBJECTS     = 1 << 8,
    /** Unsafely resolve relative paths */
    UNSAFE_RESOLVE     = 1 << 9,
    /** Print replicate details for data objects */
    PRINT_REPLICATE    = 1 << 10,
    /** Print checksums for data objects */
    PRINT_CHECKSUM     = 1 << 11
} option_flags;

#endif // _BATON_OPERATIONS_H
