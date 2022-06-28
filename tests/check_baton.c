/**
 * Copyright (C) 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021
 * Genome Research Ltd. All rights reserved.
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
 * @file check_baton.c
 */

#include <assert.h>
#include <limits.h>
#include <unistd.h>

#include <jansson.h>
#include <check.h>

#include "../src/baton.h"
#include "../src/json.h"
#include "../src/log.h"
#include "../src/read.h"
#include "../src/compat_checksum.h"
#include "../src/signal_handler.h"

int exit_flag;

static int MAX_COMMAND_LEN = 1024;
static int MAX_PATH_LEN    = 4096;

static char *TEST_COLL           = "baton-basic-test";
static char *TEST_DATA_PATH      = "data";
static char *TEST_METADATA_PATH  = "metadata/meta1.imeta";
static char *SQL_PATH            = "sql/specific_queries.sql";

static char *SETUP_SCRIPT        = "scripts/setup_irods.sh";
static char *SQL_SETUP_SCRIPT    = "scripts/setup_sql.sh";

static char *TEARDOWN_SCRIPT     = "scripts/teardown_irods.sh";
static char *SQL_TEARDOWN_SCRIPT = "scripts/teardown_sql.sh";

static char *BAD_REPLICA_SCRIPT  = "scripts/make_bad_replica.sh";

static void set_current_rods_root(char *in, char *out) {
    rodsEnv rodsEnv;

    int status = getRodsEnv(&rodsEnv);
    if (status != 0) raise(SIGTERM);

    // Create an absolute iRODS path.  Using rodsCwd is generally
    // unsafe. However, we assume that the test environment is
    // single-threaded and free of stale .irodsEnv files.
    snprintf(out, MAX_PATH_LEN, "%s/%s.%d", rodsEnv.rodsCwd, in, getpid());
}

static void setup() {
    set_log_threshold(ERROR);

    char command[MAX_COMMAND_LEN];
    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    snprintf(command, MAX_COMMAND_LEN, "%s/%s %s/%s",
             TEST_ROOT, SQL_SETUP_SCRIPT,
             TEST_ROOT, SQL_PATH);

    printf("Setup: %s\n", command);
    int ret = system(command);

    if (ret != 0) raise(SIGTERM);
}

static void teardown() {
    char command[MAX_COMMAND_LEN];
    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    snprintf(command, MAX_COMMAND_LEN, "%s/%s %s/%s",
             TEST_ROOT, SQL_TEARDOWN_SCRIPT,
             TEST_ROOT, SQL_PATH);

    printf("Teardown: %s\n", command);
    int ret = system(command);

    if (ret != 0) raise(SIGINT);
}

static void basic_setup() {
    char command[MAX_COMMAND_LEN];
    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    snprintf(command, MAX_COMMAND_LEN, "%s/%s %s/%s %s %s %s/%s",
             TEST_ROOT, SETUP_SCRIPT,
             TEST_ROOT, TEST_DATA_PATH,
             rods_root,
             TEST_RESOURCE,
             TEST_ROOT, TEST_METADATA_PATH);

    printf("Data setup: %s\n", command);
    int ret = system(command);

    if (ret != 0) raise(SIGTERM);
}

static void basic_teardown() {
    char command[MAX_COMMAND_LEN];
    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsEnv env;
    int ret = getRodsEnv(&env);
    if (ret != 0) raise(SIGTERM);

    snprintf(command, MAX_COMMAND_LEN, "%s/%s %s %s",
             TEST_ROOT, TEARDOWN_SCRIPT,
             env.rodsUserName, rods_root);

    printf("Data teardown: %s\n", command);
    ret = system(command);

    if (ret != 0) raise(SIGINT);
}

static int have_rodsadmin() {
    char *command = "iuserinfo | grep 'type: rodsadmin'";

    return !system(command);
}

static void confirm_checksum(FILE *in, const char *expected_md5) {
    char buffer[1024];
    unsigned char digest[16];

    MD5_CTX context;
    compat_MD5Init(&context);

    size_t nr;
    while ((nr = fread(buffer, 1, 1024, in)) > 0) {
        compat_MD5Update(&context, (unsigned char *) buffer, nr);
    }

    compat_MD5Final(digest, &context);

    char *md5 = calloc(33, sizeof (char));
    for (int i = 0; i < 16; i++) {
        snprintf(md5 + i * 2, 3, "%02x", digest[i]);
    }

    ck_assert_str_eq(md5, expected_md5);
    free(md5);

    return;
}

START_TEST(test_str_starts_with) {
    size_t len = MAX_STR_LEN;
    ck_assert_msg(str_starts_with("",   "",  len),    "'' starts with ''");
    ck_assert_msg(str_starts_with("a",  "",  len),   "'a' starts with ''");
    ck_assert_msg(str_starts_with("a", "a",  len),  "'a' starts with 'a'");
    ck_assert_msg(str_starts_with("ab", "a", len), "'ab' starts with 'a'");

    ck_assert_msg(!str_starts_with("",   "a", len),   "'' !starts with 'a'");
    ck_assert_msg(!str_starts_with("b",  "a", len),  "'b' !starts with 'a'");
    ck_assert_msg(!str_starts_with("ba", "a", len), "'ba' !starts with 'a'");
}
END_TEST

START_TEST(test_str_ends_with) {
    size_t len = MAX_STR_LEN;
    ck_assert_msg(str_ends_with("",   "", len),    "'' ends with ''");
    ck_assert_msg(str_ends_with("a",  "", len),   "'a' ends with ''");
    ck_assert_msg(str_ends_with("a",  "a", len),  "'a' ends with 'a'");
    ck_assert_msg(str_ends_with("ba", "a", len), "'ba' ends with 'a'");

    ck_assert_msg(!str_ends_with("",   "a", len),   "'' !ends with 'a'");
    ck_assert_msg(!str_ends_with("b",  "a", len),  "'b' !ends with 'a'");
    ck_assert_msg(!str_ends_with("ab", "a", len), "'ab' !ends with 'a'");
}
END_TEST

START_TEST(test_str_equals) {
    size_t len = MAX_STR_LEN;
    ck_assert_msg(str_equals("",     "", len),    "'' equals ''");
    ck_assert_msg(str_equals(" ",   " ", len),  "' ' equals ' '");
    ck_assert_msg(str_equals("a",   "a", len),  "'a' equals 'a'");
    ck_assert_msg(!str_equals("a",  "A", len), "'a' !equals 'A'");
    ck_assert_msg(!str_equals("aa", "a", len), "'aa' !equals 'a'");
    ck_assert_msg(!str_equals("a", "aa", len), "'a' !equals 'aa'");
}
END_TEST

START_TEST(test_str_equals_ignore_case) {
    size_t len = MAX_STR_LEN;
    ck_assert_msg(str_equals_ignore_case("",     "", len),   "'' equals ''");
    ck_assert_msg(str_equals_ignore_case(" ",   " ", len), "' ' equals ' '");
    ck_assert_msg(str_equals_ignore_case("a",   "a", len), "'a' equals 'a'");
    ck_assert_msg(str_equals_ignore_case("a",   "A", len), "'a' equals 'A'");
    ck_assert_msg(!str_equals_ignore_case("aa", "A", len), "'aa' equals 'A'");
    ck_assert_msg(!str_equals_ignore_case("a", "AA", len), "'a' equals 'AA'");
}
END_TEST

START_TEST(test_parse_base_name) {
    ck_assert_str_eq("a", parse_base_name("a"));
    ck_assert_str_eq("a", parse_base_name("/a"));
    ck_assert_str_eq("b", parse_base_name("/a/b"));
}
END_TEST

START_TEST(test_maybe_stdin) {
    ck_assert_ptr_eq(stdin, maybe_stdin(NULL));

    char file_path[MAX_PATH_LEN];
    snprintf(file_path, MAX_PATH_LEN, "%s/%s/f1.txt",
             TEST_ROOT, TEST_DATA_PATH);

    FILE *f = maybe_stdin(file_path);
    ck_assert_ptr_ne(f, NULL);
    ck_assert_int_eq(fclose(f), 0);

    ck_assert_ptr_eq(maybe_stdin("no_such_path"), NULL);
}
END_TEST

START_TEST(test_format_timestamp) {
    char *formatted = format_timestamp("01375107252", ISO8601_FORMAT);
    ck_assert_str_eq(formatted, "2013-07-29T14:14:12");

    free(formatted);
}
END_TEST

START_TEST(test_parse_timestamp) {
    char *parsed = parse_timestamp("2013-07-29T14:14:12", ISO8601_FORMAT);
    ck_assert_str_eq(parsed, "1375107252");

    free(parsed);
}
END_TEST

// Can we parse file size strings?
START_TEST(test_parse_size) {
    ck_assert_int_eq(0, parse_size("0"));

    char max[1024];
    snprintf(max, sizeof max, "%lu", ULONG_MAX);
    ck_assert_int_eq(ULONG_MAX, parse_size(max));
}
END_TEST

// Can we coerce ISO-8859-1 to UTF-8?
START_TEST(test_to_utf8) {
    char in[2]  = { 0, 0 };
    char out[3] = { 0, 0, 0 };

    for (unsigned int codepoint = 0; codepoint < 256; codepoint++) {
        in[0] = codepoint;

        memset(out, 0, sizeof out);
        to_utf8(in, out, sizeof out);

        ck_assert(maybe_utf8(out, sizeof out));
    }
}
END_TEST

// Can we log in?
START_TEST(test_rods_login) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    ck_assert_ptr_ne(conn, NULL);
    ck_assert(conn->loggedIn);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we test that iRODS is accepting connections?
START_TEST(test_is_irods_available) {
    int avail = is_irods_available();
    ck_assert(avail == 0 || avail == 1);
}
END_TEST

// Can we prepare a new path designator?
START_TEST(test_init_rods_path) {
    rodsPath_t rodspath;
    char *inpath = "a path";

    ck_assert_int_eq(init_rods_path(&rodspath, inpath), 0);
    ck_assert_str_eq(rodspath.inPath, inpath);
}
END_TEST

// Can we get client and server version strings?
START_TEST(test_get_version) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char expected[MAX_VERSION_STR_LEN];
    snprintf(expected, MAX_VERSION_STR_LEN, "%d.%d.%d",
	     IRODS_VERSION_MAJOR, IRODS_VERSION_MINOR, IRODS_VERSION_PATCHLEVEL);

    char *client_version = get_client_version();
    ck_assert_str_eq(client_version, expected);
    if (client_version) free(client_version);

    baton_error_t version_error;
    char *server_version = get_server_version(conn, &version_error);
    ck_assert_int_eq(version_error.code, 0);
    ck_assert_ptr_ne(server_version, NULL);
    if (server_version) free(server_version);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we resolve a real path?
START_TEST(test_resolve_rods_path) {
    option_flags flags = 0;
    rodsEnv env;
    rodsPath_t rods_path;

    rcComm_t *conn = rods_login(&env);
    char *path = "/";

    ck_assert_msg(is_irods_available(), "iRODS is not available");
    ck_assert(conn->loggedIn);

    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, path,
                                       flags, &resolve_error), EXIST_ST);
    ck_assert_int_eq(resolve_error.code, 0);
    ck_assert_str_eq(rods_path.inPath, path);
    ck_assert_str_eq(rods_path.outPath, path);
    ck_assert_int_eq(rods_path.objType, COLL_OBJ_T);

    char *invalid_path = "";
    rodsPath_t inv_rods_path;
    baton_error_t invalid_path_error;
    ck_assert(resolve_rods_path(conn, &env, &inv_rods_path, invalid_path,
                                flags, &invalid_path_error) < 0);


    char *no_path = "no such path";
    rodsPath_t no_rods_path;
    baton_error_t no_path_error;
    ck_assert_int_ne(resolve_rods_path(conn, &env, &no_rods_path, no_path,
                                       flags, &no_path_error), EXIST_ST);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Do we fail to list a non-existent path?
START_TEST(test_list_missing_path) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char *path = "no such path";
    rodsPath_t rods_path;
    baton_error_t resolve_error;
    resolve_rods_path(conn, &env, &rods_path, path, flags, &resolve_error);
    ck_assert_int_ne(resolve_error.code, 0);

    baton_error_t error;
    ck_assert_ptr_eq(list_path(conn, &rods_path, PRINT_ACL, &error), NULL);
    ck_assert_int_ne(error.code, 0);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we list a data object?
START_TEST(test_list_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_obj_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                                       flags, &resolve_obj_error), EXIST_ST);

    json_t *perm = json_pack("{s:s, s:s, s:s}",
                             JSON_OWNER_KEY, env.rodsUserName,
                             JSON_ZONE_KEY,  env.rodsZone,
                             JSON_LEVEL_KEY, ACCESS_LEVEL_OWN);
    json_t *avu = json_pack("{s:s, s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1",
                            JSON_UNITS_KEY,     "units1");

    flags = SEARCH_COLLECTIONS | SEARCH_OBJECTS;

    // Default representation
    baton_error_t error1;
    json_t *results1 = list_path(conn, &rods_obj_path, flags, &error1);
    json_t *expected1 = json_pack("{s:s, s:s}",
                                  JSON_COLLECTION_KEY,  rods_path.outPath,
                                  JSON_DATA_OBJECT_KEY, "f1.txt");

    ck_assert_int_eq(json_equal(results1, expected1), 1);
    ck_assert_int_eq(error1.code, 0);

    // With file size
    baton_error_t error2;
    json_t *results2 = list_path(conn, &rods_obj_path, flags | PRINT_SIZE,
                                 &error2);
    json_t *expected2 = json_pack("{s:s, s:s, s:i}",
                                  JSON_COLLECTION_KEY,  rods_path.outPath,
                                  JSON_DATA_OBJECT_KEY, "f1.txt",
                                  JSON_SIZE_KEY,        0);

    ck_assert_int_eq(json_equal(results2, expected2), 1);
    ck_assert_int_eq(error2.code, 0);

    // With ACL
    baton_error_t error3;
    json_t *results3 = list_path(conn, &rods_obj_path,
                                 flags | PRINT_SIZE | PRINT_ACL, &error3);
    json_t *expected3 = json_pack("{s:s, s:s, s:i, s:[O]}",
                                  JSON_COLLECTION_KEY,  rods_path.outPath,
                                  JSON_DATA_OBJECT_KEY, "f1.txt",
                                  JSON_SIZE_KEY,        0,
                                  JSON_ACCESS_KEY,      perm);

    ck_assert_int_eq(json_equal(results3, expected3), 1);
    ck_assert_int_eq(error3.code, 0);

    // With AVUs
    baton_error_t error4;
    json_t *results4 = list_path(conn, &rods_obj_path,
                                 flags | PRINT_SIZE | PRINT_ACL | PRINT_AVU,
                                 &error4);
    json_t *expected4 = json_pack("{s:s, s:s, s:i, s:[O], s:[O]}",
                                  JSON_COLLECTION_KEY,  rods_path.outPath,
                                  JSON_DATA_OBJECT_KEY, "f1.txt",
                                  JSON_SIZE_KEY,        0,
                                  JSON_ACCESS_KEY,      perm,
                                  JSON_AVUS_KEY,        avu);

    ck_assert_int_eq(json_equal(results4, expected4), 1);
    ck_assert_int_eq(error4.code, 0);

    // With timestamps
    baton_error_t error5;
    json_t *results5 = list_path(conn, &rods_obj_path,
                                 flags | PRINT_SIZE | PRINT_ACL | PRINT_AVU |
                                 PRINT_TIMESTAMP, &error5);
    ck_assert_int_eq(error5.code, 0);

    json_t *timestamps = json_object_get(results5, JSON_TIMESTAMPS_KEY);
    ck_assert(json_is_array(timestamps));

    int num_timestamps = 2 * 2; // For 2 replicates;
    ck_assert_int_eq(json_array_size(timestamps), num_timestamps);

    // These are processed timestamps where there is a separate JSON
    // object for created and modified stamps and the times are
    // ISO8601 format.
    size_t index;
    json_t *timestamp;
    json_array_foreach(timestamps, index, timestamp) {
        ck_assert_int_eq(json_object_size(timestamp), 2);
        ck_assert(json_object_get(timestamp, JSON_CREATED_KEY) ||
                  json_object_get(timestamp, JSON_MODIFIED_KEY));
    }

    // With checksum
    baton_error_t error6;
    json_t *results6 = list_path(conn, &rods_obj_path,
                                 flags | PRINT_SIZE | PRINT_ACL | PRINT_AVU |
                                 PRINT_TIMESTAMP | PRINT_CHECKSUM , &error6);
    ck_assert_int_eq(error6.code, 0);
    json_t *checksum = json_object_get(results6, JSON_CHECKSUM_KEY);
    ck_assert(json_is_string(checksum));
    ck_assert_str_eq(json_string_value(checksum),
                     "d41d8cd98f00b204e9800998ecf8427e");

    json_decref(results1);
    json_decref(expected1);

    json_decref(results2);
    json_decref(expected2);

    json_decref(results3);
    json_decref(expected3);

    json_decref(results4);
    json_decref(expected4);

    json_decref(results5);
    json_decref(results6);

    json_decref(perm);
    json_decref(avu);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we list a collection?
START_TEST(test_list_coll) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_path(conn, &rods_path, PRINT_ACL, &error);

    json_t *perm = json_pack("{s:s, s:s, s:s}",
                             JSON_OWNER_KEY, env.rodsUserName,
                             JSON_ZONE_KEY,  env.rodsZone,
                             JSON_LEVEL_KEY, ACCESS_OWN);
    json_t *expected = json_pack("{s:s, s:[o]}",
                                 JSON_COLLECTION_KEY, rods_path.outPath,
                                 JSON_ACCESS_KEY,     perm);

    ck_assert_ptr_ne(NULL, results);
    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we list a collection's contents?
START_TEST(test_list_coll_contents) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_path(conn, &rods_path,
                                PRINT_SIZE | PRINT_ACL | PRINT_CONTENTS |
                                PRINT_CHECKSUM, &error);

    char a[MAX_PATH_LEN];
    char b[MAX_PATH_LEN];
    char c[MAX_PATH_LEN];
    snprintf(a, MAX_PATH_LEN, "%s/a", rods_path.outPath);
    snprintf(b, MAX_PATH_LEN, "%s/b", rods_path.outPath);
    snprintf(c, MAX_PATH_LEN, "%s/c", rods_path.outPath);

    json_t *perm = json_pack("{s:s, s:s, s:s}",
                             JSON_OWNER_KEY, env.rodsUserName,
                             JSON_ZONE_KEY,  env.rodsZone,
                             JSON_LEVEL_KEY, ACCESS_OWN);

    json_t *expected =
        json_pack("{s:s, s:[o],"
                  " s:[{s:s, s:s, s:i, s:s, s:[o]},"  // f1.txt
                  "    {s:s, s:s, s:i, s:s, s:[o]},"  // f2.txt
                  "    {s:s, s:s, s:i, s:s, s:[o]},"  // f3.txt
                  "    {s:s, s:s, s:i, s:s, s:[o]},"  // lorem_10k.txt
                  "    {s:s, s:s, s:i, s:s, s:[o]},"  // lorem_1b.txt
                  "    {s:s, s:s, s:i, s:s, s:[o]},"  // lorem_1k.txt
                  "    {s:s, s:s, s:i, s:s, s:[o]},"  // r1.txt
                  "    {s:s, s:[o]},"            // a
                  "    {s:s, s:[o]},"            // b
                  "    {s:s, s:[o]}]}",          // c
                  JSON_COLLECTION_KEY, rods_path.outPath,
                  JSON_ACCESS_KEY,     perm,

                  JSON_CONTENTS_KEY,
                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "f1.txt",
                  JSON_SIZE_KEY,        0,
                  JSON_CHECKSUM_KEY,    "d41d8cd98f00b204e9800998ecf8427e",
                  JSON_ACCESS_KEY,      perm,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "f2.txt",
                  JSON_SIZE_KEY,        0,
                  JSON_CHECKSUM_KEY,    "d41d8cd98f00b204e9800998ecf8427e",
                  JSON_ACCESS_KEY,      perm,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "f3.txt",
                  JSON_SIZE_KEY,        0,
                  JSON_CHECKSUM_KEY,    "d41d8cd98f00b204e9800998ecf8427e",
                  JSON_ACCESS_KEY,      perm,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "lorem_10k.txt",
                  JSON_SIZE_KEY,        10240,
                  JSON_CHECKSUM_KEY,    "4efe0c1befd6f6ac4621cbdb13241246",
                  JSON_ACCESS_KEY,      perm,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "lorem_1b.txt",
                  JSON_SIZE_KEY,        1,
                  JSON_CHECKSUM_KEY,    "d20caec3b48a1eef164cb4ca81ba2587",
                  JSON_ACCESS_KEY,      perm,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "lorem_1k.txt",
                  JSON_SIZE_KEY,        1024,
                  JSON_CHECKSUM_KEY,    "1f40c34d28e56efcf9da6732cdc93b8b",
                  JSON_ACCESS_KEY,      perm,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "r1.txt",
                  JSON_SIZE_KEY,        0,
                  JSON_CHECKSUM_KEY,    "d41d8cd98f00b204e9800998ecf8427e",
                  JSON_ACCESS_KEY,      perm,

                  JSON_COLLECTION_KEY,  a,
                  JSON_ACCESS_KEY,      perm,

                  JSON_COLLECTION_KEY,  b,
                  JSON_ACCESS_KEY,      perm,

                  JSON_COLLECTION_KEY,  c,
                  JSON_ACCESS_KEY,      perm);

    ck_assert_ptr_ne(NULL, results);
    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we build a general query input?
START_TEST(test_make_query_input) {
    int max_rows = 10;
    int num_columns = 1;
    int columns[] = { COL_COLL_NAME };
    genQueryInp_t* query_input =
        make_query_input(max_rows, num_columns, columns);

    ck_assert_ptr_ne(query_input, NULL);
    ck_assert_ptr_ne(query_input->selectInp.inx, NULL);
    ck_assert_ptr_ne(query_input->selectInp.value, NULL);
    ck_assert_int_eq(query_input->selectInp.len, num_columns);

    ck_assert_int_eq(query_input->maxRows, max_rows);
    ck_assert_int_eq(query_input->continueInx, 0);
    ck_assert_int_eq(query_input->condInput.len, 0);

    ck_assert_ptr_ne(query_input->sqlCondInp.inx, NULL);
    ck_assert_ptr_ne(query_input->sqlCondInp.value, NULL);
    ck_assert_int_eq(query_input->sqlCondInp.len, 0);

    free_query_input(query_input);
}
END_TEST

// Do we fail to list the ACL of a non-existent path?
START_TEST(test_list_permissions_missing_path) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char *path = "no such path";
    rodsPath_t rods_path;
    baton_error_t resolve_error;
    resolve_rods_path(conn, &env, &rods_path, path, flags, &resolve_error);

    baton_error_t error;
    ck_assert_ptr_eq(list_permissions(conn, &rods_path, &error), NULL);
    ck_assert_int_ne(error.code, 0);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we list the ACL of an object?
START_TEST(test_list_permissions_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_permissions(conn, &rods_path, &error);
    json_t *expected = json_pack("[{s:s, s:s, s:s}]",
                                 JSON_OWNER_KEY, env.rodsUserName,
                                 JSON_ZONE_KEY,  env.rodsZone,
                                 JSON_LEVEL_KEY, ACCESS_OWN);

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we list the ACL of a collection?
START_TEST(test_list_permissions_coll) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_permissions(conn, &rods_path, &error);
    json_t *expected = json_pack("[{s:s, s:s, s:s}]",
                                 JSON_OWNER_KEY, env.rodsUserName,
                                 JSON_ZONE_KEY,  env.rodsZone,
                                 JSON_LEVEL_KEY, ACCESS_OWN);

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we list metadata on a data object?
START_TEST(test_list_metadata_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char *no_path = "no such path";
    rodsPath_t no_rods_path;
    baton_error_t resolve_no_path_error;
    resolve_rods_path(conn, &env, &no_rods_path, no_path,
                      flags, &resolve_no_path_error);

    baton_error_t no_path_error;
    ck_assert_ptr_eq(list_metadata(conn, &no_rods_path, NULL, &no_path_error),
                     NULL);
    ck_assert_int_ne(no_path_error.code, 0);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_metadata(conn, &rods_path, NULL, &error);
    json_t *expected = json_pack("[{s:s, s:s, s:s}]",
                                 JSON_ATTRIBUTE_KEY, "attr1",
                                 JSON_VALUE_KEY,     "value1",
                                 JSON_UNITS_KEY,     "units1");

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we list metadata on a collection?
START_TEST(test_list_metadata_coll) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char coll_path[MAX_PATH_LEN];
    snprintf(coll_path, MAX_PATH_LEN, "%s/a", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, coll_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_metadata(conn, &rods_path, NULL, &error);
    json_t *expected = json_pack("[{s:s, s:s, s:s}]",
                                 JSON_ATTRIBUTE_KEY, "attr2",
                                 JSON_VALUE_KEY,     "value2",
                                 JSON_UNITS_KEY,     "units2");

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_list_replicates_obj) {
    if (!TEST_RESOURCE) {
        logmsg(WARN, "!!! Skipping test_list_replicates_obj because "
               "no test resource is defined; TEST_RESOURCE=%s !!!",
               TEST_RESOURCE);
        return;
    }

    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    char *checksum = "d41d8cd98f00b204e9800998ecf8427e";
    json_t *expected =
      json_pack("[{s:s, s:i, s:b}, {s:s, s:i, s:b}]",
                // Assume original copy is replicate 0 on default resource
                JSON_CHECKSUM_KEY,         checksum,
                JSON_REPLICATE_NUMBER_KEY, 0,
                JSON_REPLICATE_STATUS_KEY, 1,
                // Assume replicated copy is replicate 1 on test resource
                JSON_CHECKSUM_KEY,         checksum,
                JSON_REPLICATE_NUMBER_KEY, 1,
                JSON_REPLICATE_STATUS_KEY, 1);

    baton_error_t error;
    json_t *results = list_replicates(conn, &rods_path, &error);
    ck_assert_int_eq(error.code, 0);
    ck_assert(json_is_array(results));

    size_t index;
    json_t *result;
    json_array_foreach(results, index, result) {
        ck_assert(json_is_object(result));
        // We don't know what the location value will be for a test
        // run because it's an iRODS resource server hostname.
        ck_assert(json_object_get(result, JSON_LOCATION_KEY));
        // Remove it once tested so that we can easily check the other
        // values together.
        if (json_object_get(result, JSON_LOCATION_KEY)) {
            json_object_del(result, JSON_LOCATION_KEY);
        }

        ck_assert(json_object_get(result, JSON_RESOURCE_KEY));
        const char *resc =
            json_string_value(json_object_get(result, JSON_RESOURCE_KEY));

        if (index == 0) {
            ck_assert_msg(!str_equals(resc, TEST_RESOURCE, MAX_STR_LEN),
                          "resource other than TEST_RESOURCE");
        }
        else {
            // For a compound resource, the resource is a leaf named
            // unixfs* to which the named TEST_RESOURCE has delegated. For
            // a non-compound resource, the resource is the single named
            // TEST_RESOURCE.
            ck_assert_msg(str_starts_with(resc, "unixfs", 7)
                          ||
                          str_equals(resc, TEST_RESOURCE, MAX_STR_LEN),
                          "resource is either root or leaf");
        }

        if (json_object_get(result, JSON_RESOURCE_KEY)) {
            json_object_del(result, JSON_RESOURCE_KEY);
        }
    }

    ck_assert_int_eq(json_equal(results, expected), 1);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_list_timestamps_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *timestamps = list_timestamps(conn, &rods_path, &error);
    ck_assert_int_eq(error.code, 0);
    ck_assert(json_is_array(timestamps));

    int num_timestamps = 1 * 2; // For 2 replicates
    ck_assert_int_eq(json_array_size(timestamps), num_timestamps);

    // These are raw timestamps where both created and modified times
    // are present iRODS query results.
    size_t index;
    json_t *timestamp;
    json_array_foreach(timestamps, index, timestamp) {
        ck_assert(json_is_object(timestamp));
        ck_assert_int_eq(json_object_size(timestamp), 3);
        ck_assert(json_object_get(timestamp, JSON_CREATED_KEY));
        ck_assert(json_object_get(timestamp, JSON_MODIFIED_KEY));
        ck_assert(json_object_get(timestamp, JSON_REPLICATE_KEY));
    }

    json_decref(timestamps);

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_list_timestamps_coll) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *timestamps = list_timestamps(conn, &rods_path, &error);

    ck_assert_int_eq(error.code, 0);
    ck_assert(json_is_array(timestamps));
    ck_assert_int_eq(json_array_size(timestamps), 1);

    // These are raw timestamps where both created and modified times
    // are present iRODS query results. Unlike data object timestamps,
    // there is no replicate number.
    size_t index;
    json_t *timestamp;
    json_array_foreach(timestamps, index, timestamp) {
        ck_assert(json_is_object(timestamp));
        ck_assert_int_eq(json_object_size(timestamp), 2);
        ck_assert(json_object_get(timestamp, JSON_CREATED_KEY));
        ck_assert(json_object_get(timestamp, JSON_MODIFIED_KEY));
    }

    json_decref(timestamps);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we search for data objects by their metadata?
START_TEST(test_search_metadata_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    json_t *avu = json_pack("{s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1");
    json_t *query = json_pack("{s:s, s:[o]}",
                              JSON_COLLECTION_KEY, rods_path.outPath,
                              JSON_AVUS_KEY,       avu);
    flags = SEARCH_COLLECTIONS | SEARCH_OBJECTS;

    baton_error_t expected_error1;
    search_metadata(conn, NULL, NULL, flags | PRINT_AVU, &expected_error1);
    ck_assert_int_ne(expected_error1.code, 0);

    baton_error_t expected_error2;
    search_metadata(conn, json_pack("[]"), NULL, flags | PRINT_AVU,
                    &expected_error2);
    ck_assert_int_ne(expected_error2.code, 0);

    baton_error_t error;
    json_t *results = search_metadata(conn, query, NULL, flags | PRINT_AVU,
                                      &error);
    ck_assert_int_eq(json_array_size(results), 12);

    for (size_t i = 0; i < 12; i++) {
        json_t *obj = json_array_get(results, i);
        ck_assert_ptr_ne(json_object_get(obj, JSON_COLLECTION_KEY), NULL);
        ck_assert_ptr_ne(json_object_get(obj, JSON_DATA_OBJECT_KEY), NULL);
    }
    ck_assert_int_eq(error.code, 0);

    json_decref(query);
    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we search for data objects by their metadata, limiting scope by
// path?
START_TEST(test_search_metadata_path_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    char search_root[MAX_PATH_LEN];
    snprintf(search_root, MAX_PATH_LEN, "%s/a/x/m", rods_path.outPath);

    json_t *avu = json_pack("{s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1");
    json_t *query1 = json_pack("{s:s, s:[O]}",
                               JSON_COLLECTION_KEY, search_root,
                               JSON_AVUS_KEY,       avu);
    flags = SEARCH_COLLECTIONS | SEARCH_OBJECTS;

    baton_error_t error1;
    json_t *results1 = search_metadata(conn, query1, NULL, flags | PRINT_AVU,
                                       &error1);

    ck_assert_int_eq(error1.code, 0);
    ck_assert_int_eq(json_array_size(results1), 3);

    for (size_t i = 0; i < 3; i++) {
        json_t *obj = json_array_get(results1, i);
        ck_assert_ptr_ne(json_object_get(obj, JSON_COLLECTION_KEY), NULL);
        ck_assert_ptr_ne(json_object_get(obj, JSON_DATA_OBJECT_KEY), NULL);
    }

    // Short form JSON collection property
    json_t *query2 = json_pack("{s:s, s:[O]}",
                                JSON_COLLECTION_SHORT_KEY, search_root,
                                JSON_AVUS_KEY,       avu);
    baton_error_t error2;
    json_t *results2 = search_metadata(conn, query2, NULL, flags | PRINT_AVU,
                                        &error2);

    ck_assert_int_eq(error2.code, 0);
    ck_assert_int_eq(json_array_size(results2), 3);

    for (size_t i = 0; i < 3; i++) {
        json_t *obj = json_array_get(results2, i);
        ck_assert_ptr_ne(json_object_get(obj, JSON_COLLECTION_KEY), NULL);
        ck_assert_ptr_ne(json_object_get(obj, JSON_DATA_OBJECT_KEY), NULL);
    }

    json_decref(query1);
    json_decref(results1);

    json_decref(query2);
    json_decref(results2);

    json_decref(avu);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we search for data objects by their metadata, limited by
// permission?
START_TEST(test_search_metadata_perm_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t mod_error;
    int rv = modify_permissions(conn, &rods_path, NO_RECURSE, "public",
                                ACCESS_LEVEL_READ, &mod_error);
    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(mod_error.code, 0);

    json_t *avu = json_pack("{s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1");
    json_t *perm = json_pack("{s:s:, s:s}",
                             JSON_OWNER_KEY, "public",
                             JSON_LEVEL_KEY,  ACCESS_LEVEL_READ);

    json_t *query = json_pack("{s:[o], s:[o]}",
                              JSON_AVUS_KEY,   avu,
                              JSON_ACCESS_KEY, perm);
    flags = SEARCH_COLLECTIONS | SEARCH_OBJECTS;

    baton_error_t error;
    json_t *results = search_metadata(conn, query, NULL, flags| PRINT_AVU,
                                      &error);
    ck_assert_int_eq(error.code, 0);
    ck_assert_int_eq(json_array_size(results), 1);

    json_t *obj = json_array_get(results, 0);
    ck_assert_ptr_ne(json_object_get(obj, JSON_COLLECTION_KEY), NULL);
    ck_assert_ptr_ne(json_object_get(obj, JSON_DATA_OBJECT_KEY), NULL);

    json_decref(query);
    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we search for data objects, limited by creation timestamp?
START_TEST(test_search_metadata_tps_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t ts_error;
    json_t *current_tps = list_timestamps(conn, &rods_path, &ts_error);
    ck_assert_int_eq(ts_error.code, 0);
    json_t *first_tps = json_array_get(current_tps, 0);

    const char *raw_created = get_created_timestamp(first_tps, &ts_error);
    ck_assert_int_eq(ts_error.code, 0);

    char *iso_created = format_timestamp(raw_created, RFC3339_FORMAT);

    // Should not find any results for data objects created before the
    // root collection of the test data was created
    json_t *created_lt = json_pack("{s:s, s:s}",
                                   JSON_CREATED_KEY,  iso_created,
                                   JSON_OPERATOR_KEY, SEARCH_OP_NUM_LT);
    json_t *query_lt = json_pack("{s:[], s:s, s:[o]}",
                                 JSON_AVUS_KEY,
                                 JSON_COLLECTION_KEY, rods_path.outPath,
                                 JSON_TIMESTAMPS_KEY, created_lt);
    flags = SEARCH_COLLECTIONS | SEARCH_OBJECTS;

    baton_error_t error_lt;
    json_t *results_lt = search_metadata(conn, query_lt, NULL, flags,
                                         &error_lt);
    ck_assert_int_eq(error_lt.code, 0);
    ck_assert_int_eq(json_array_size(results_lt), 0);

    // Should find all files because they must be created at, or after
    // the time their containing collection was created
    json_t *created_ge = json_pack("{s:s, s:s}",
                                   JSON_CREATED_KEY,  iso_created,
                                   JSON_OPERATOR_KEY, SEARCH_OP_NUM_GE);
    json_t *query_ge = json_pack("{s:[], s:s, s:[o]}",
                                 JSON_AVUS_KEY,
                                 JSON_COLLECTION_KEY, rods_root,
                                 JSON_TIMESTAMPS_KEY, created_ge);

    baton_error_t error_ge;
    json_t *results_ge = search_metadata(conn, query_ge, NULL, flags,
                                         &error_ge);

    ck_assert_int_eq(error_ge.code, 0);
    ck_assert_int_eq(json_array_size(results_ge), 32);

    int num_colls = 0;
    int num_objs  = 0;
    int i;
    json_t *elt;

    json_array_foreach(results_ge, i, elt) {
        if (represents_collection(elt))  num_colls++;
        if (represents_data_object(elt)) num_objs++;
    }

    ck_assert_int_eq(num_colls, 10);
    ck_assert_int_eq(num_objs,  22); // Includes some .gitignore files

    free(iso_created);

    json_decref(current_tps);
    json_decref(query_ge);
    json_decref(results_ge);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we search for collections by their metadata?
START_TEST(test_search_metadata_coll) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    json_t *avu = json_pack("{s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr2",
                            JSON_VALUE_KEY,     "value2");
    json_t *query = json_pack("{s:s, s:[o]}",
                              JSON_COLLECTION_KEY, rods_path.outPath,
                              JSON_AVUS_KEY,       avu);
    flags = SEARCH_COLLECTIONS | SEARCH_OBJECTS;

    baton_error_t error;
    json_t *results = search_metadata(conn, query, NULL, flags | PRINT_AVU,
                                      &error);
    ck_assert_int_eq(json_array_size(results), 3);
    for (size_t i = 0; i < 3; i++) {
        json_t *coll = json_array_get(results, i);
        ck_assert_ptr_ne(json_object_get(coll, JSON_COLLECTION_KEY), NULL);
        ck_assert_ptr_eq(json_object_get(coll, JSON_DATA_OBJECT_KEY), NULL);
    }
    ck_assert_int_eq(error.code, 0);

    json_decref(query);
    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we add an AVU to a data object?
START_TEST(test_add_metadata_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    // Bad call with no attr
    baton_error_t expected_error1;
    int fail_rv1 = modify_metadata(conn, &rods_path, META_ADD, NULL,
                                   "test_value", "test_units",
                                   &expected_error1);
    ck_assert_int_ne(fail_rv1, 0);
    ck_assert_int_ne(expected_error1.code, 0);

    // Bad call with empty attr
    baton_error_t expected_error2;
    int fail_rv2 = modify_metadata(conn, &rods_path, META_ADD, "",
                                   "test_value", "test_units",
                                   &expected_error2);
    ck_assert_int_ne(fail_rv2, 0);
    ck_assert_int_ne(expected_error2.code, 0);

    // Bad call with no value
    baton_error_t expected_error3;
    int fail_rv3 = modify_metadata(conn, &rods_path, META_ADD, "test_attr",
                                   NULL, "test_units",
                                   &expected_error3);
    ck_assert_int_ne(fail_rv3, 0);
    ck_assert_int_ne(expected_error3.code, 0);

    // Bad call with empty value
    baton_error_t expected_error4;
    int fail_rv4 = modify_metadata(conn, &rods_path, META_ADD, "test_attr",
                                   "", "test_units",
                                   &expected_error4);
    ck_assert_int_ne(fail_rv4, 0);
    ck_assert_int_ne(expected_error4.code, 0);

    baton_error_t error;
    int rv = modify_metadata(conn, &rods_path, META_ADD, "test_attr",
                             "test_value", "test_units",
                             &error);
    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(error.code, 0);

    baton_error_t list_error;
    json_t *results = list_metadata(conn, &rods_path, "test_attr", &list_error);
    json_t *expected = json_pack("[{s:s, s:s, s:s}]",
                                 JSON_ATTRIBUTE_KEY, "test_attr",
                                 JSON_VALUE_KEY,     "test_value",
                                 JSON_UNITS_KEY,     "test_units");

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(list_error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST


// Do we fail to add metadata to a non-existent path?
START_TEST(test_add_metadata_missing_path) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char *path = "no such path";
    rodsPath_t rods_path;
    baton_error_t resolve_error;
    resolve_rods_path(conn, &env, &rods_path, path, flags, &resolve_error);

    baton_error_t expected_error;
    int rv = modify_metadata(conn, &rods_path, META_ADD, "test_attr",
                             "test_value", "test_units", &expected_error);
    ck_assert_int_ne(rv, 0);
    ck_assert_int_ne(expected_error.code, 0);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we remove an AVU from a data object?
START_TEST(test_remove_metadata_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    int rv = modify_metadata(conn, &rods_path, META_REM, "attr1", "value1",
                             "units1", &error);
    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(error.code, 0);

    baton_error_t list_error;
    json_t *results = list_metadata(conn, &rods_path, NULL, &list_error);
    json_t *expected = json_array(); // Empty

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(list_error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we add a JSON AVU to a data object?
START_TEST(test_add_json_metadata_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    // Bad AVU; not a JSON object
    json_t *bad_avu1 = json_pack("[]");
    baton_error_t expected_error1;
    int fail_rv1 = modify_json_metadata(conn, &rods_path, META_ADD, bad_avu1,
                                        &expected_error1);
    ck_assert_int_ne(fail_rv1, 0);
    ck_assert_int_ne(expected_error1.code, 0);

    // Bad AVU with no attribute
    json_t *bad_avu2 = json_pack("{s:s}", JSON_VALUE_KEY, "test_value");
    baton_error_t expected_error2;
    int fail_rv2 = modify_json_metadata(conn, &rods_path, META_ADD, bad_avu2,
                                        &expected_error2);
    ck_assert_int_ne(fail_rv2, 0);
    ck_assert_int_ne(expected_error2.code, 0);

    // Bad AVU with no value
    json_t *bad_avu3 = json_pack("{s:s}", JSON_ATTRIBUTE_KEY, "test_attr");
    baton_error_t expected_error3;
    int fail_rv3 = modify_json_metadata(conn, &rods_path, META_ADD, bad_avu3,
                                        &expected_error3);
    ck_assert_int_ne(fail_rv3, 0);
    ck_assert_int_ne(expected_error3.code, 0);

    json_t *avu = json_pack("{s:s, s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "test_attr",
                            JSON_VALUE_KEY,     "test_value",
                            JSON_UNITS_KEY,     "test_units");
    baton_error_t error;
    modify_json_metadata(conn, &rods_path, META_ADD, avu, &error);
    ck_assert_int_eq(error.code, 0);

    baton_error_t list_error;
    json_t *results = list_metadata(conn, &rods_path, "test_attr", &list_error);
    json_t *expected = json_array();
    json_array_append_new(expected, avu);

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(list_error.code, 0);

    json_decref(bad_avu1);
    json_decref(bad_avu2);
    json_decref(bad_avu3);
    json_decref(avu);
    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we remove a JSON AVU from a data object?
START_TEST(test_remove_json_metadata_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    json_t *avu = json_pack("{s:s, s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1",
                            JSON_UNITS_KEY,     "units1");
    baton_error_t error;
    modify_json_metadata(conn, &rods_path, META_REM, avu, &error);
    ck_assert_int_eq(error.code, 0);

    baton_error_t list_error;
    json_t *results = list_metadata(conn, &rods_path, NULL, &list_error);
    json_t *expected = json_array(); // Empty

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(list_error.code, 0);

    json_decref(avu);
    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we change permissions on a data object?
START_TEST(test_modify_permissions_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t mod_error;
    int rv = modify_permissions(conn, &rods_path, NO_RECURSE, "public",
                                ACCESS_LEVEL_READ, &mod_error);
    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(mod_error.code, 0);

    json_t *expected = json_pack("{s:s, s:s, s:s}",
                                 JSON_OWNER_KEY, "public",
                                 JSON_ZONE_KEY,  env.rodsZone,
                                 JSON_LEVEL_KEY, ACCESS_LEVEL_READ);

    baton_error_t list_error;
    json_t *acl = list_permissions(conn, &rods_path, &list_error);
    ck_assert_int_eq(list_error.code, 0);

    int num_elts = json_array_size(acl);
    ck_assert_int_ne(num_elts, 0);

    int found = 0;
    for (int i = 0; i < num_elts; i++) {
        json_t *elt = json_array_get(acl, i);

        ck_assert(json_is_object(elt));
        if (json_equal(expected, elt)) found = 1;
    }

    ck_assert_int_eq(found, 1);

    json_decref(expected);
    json_decref(acl);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we change JSON permissions on a data object?
START_TEST(test_modify_json_permissions_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    json_t *bad_perm1 = json_pack("{s:s}", JSON_OWNER_KEY, "public");
    baton_error_t expected_error1;
    int fail_rv1 = modify_json_permissions(conn, &rods_path, NO_RECURSE,
                                           bad_perm1, &expected_error1);
    ck_assert_int_ne(fail_rv1, 0);
    ck_assert_int_ne(expected_error1.code, 0);

    json_t *bad_perm2 = json_pack("{s:s}", JSON_LEVEL_KEY, ACCESS_LEVEL_READ);
    baton_error_t expected_error2;
    int fail_rv2 = modify_json_permissions(conn, &rods_path, NO_RECURSE,
                                           bad_perm2, &expected_error2);

    ck_assert_int_ne(expected_error2.code, 0);
    ck_assert_int_ne(fail_rv2, 0);

    // Explicit zone
    json_t *perm_with_zone = json_pack("{s:s, s:s, s:s}",
                                       JSON_OWNER_KEY, "public",
                                       JSON_ZONE_KEY,  env.rodsZone,
                                       JSON_LEVEL_KEY, ACCESS_LEVEL_READ);

    baton_error_t error_with_zone;
    int rv_with_zone =
        modify_json_permissions(conn, &rods_path, NO_RECURSE,
                                perm_with_zone, &error_with_zone);
    ck_assert_int_eq(rv_with_zone, 0);
    ck_assert_int_eq(error_with_zone.code, 0);

    baton_error_t list_error_with_zone;
    json_t *acl_with_zone =
        list_permissions(conn, &rods_path, &list_error_with_zone);
    ck_assert_int_eq(list_error_with_zone.code, 0);

    int num_elts = json_array_size(acl_with_zone);
    ck_assert_int_ne(num_elts, 0);

    int found = 0;
    for (int i = 0; i < num_elts; i++) {
        json_t *elt = json_array_get(acl_with_zone, i);

        // Return value should equal the input
        ck_assert(json_is_object(elt));
        if (json_equal(perm_with_zone, elt)) found = 1;
    }

    ck_assert_int_eq(found, 1);

    json_t *perm_without_zone = json_pack("{s:s, s:s}",
                                          JSON_OWNER_KEY, "public",
                                          JSON_LEVEL_KEY, ACCESS_LEVEL_READ);

    // Implicit zone should not raise an error
    baton_error_t error_without_zone;
    int rv_without_zone =
        modify_json_permissions(conn, &rods_path, NO_RECURSE,
                                perm_without_zone, &error_without_zone);
    ck_assert_int_eq(rv_without_zone, 0);
    ck_assert_int_eq(error_without_zone.code, 0);

    baton_error_t list_error_without_zone;
    json_t *acl_without_zone =
        list_permissions(conn, &rods_path, &list_error_without_zone);
    ck_assert_int_eq(list_error_without_zone.code, 0);
    // Zone should be populated in returned ACL
    ck_assert_int_eq(json_equal(acl_without_zone, acl_with_zone), 1);

    json_decref(bad_perm1);
    json_decref(bad_perm2);
    json_decref(perm_with_zone);
    json_decref(perm_without_zone);
    json_decref(acl_with_zone);
    json_decref(acl_without_zone);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we convert JSON representation to a useful path string?
START_TEST(test_json_to_path) {
    const char *coll_path = "/a/b/c";
    json_t *coll1 = json_pack("{s:s}", JSON_COLLECTION_KEY, coll_path);
    baton_error_t error_coll1;
    ck_assert_str_eq(json_to_path(coll1, &error_coll1), coll_path);
    ck_assert_int_eq(error_coll1.code, 0);
    json_decref(coll1);

    // Collection not a string
    json_t *coll2 = json_pack("{s: []}", JSON_COLLECTION_KEY);
    baton_error_t error_coll2;
    ck_assert_ptr_eq(json_to_path(coll2, &error_coll2), NULL);
    ck_assert_int_ne(error_coll2.code, 0);
    json_decref(coll2);

    const char *obj_path = "/a/b/c.txt";
    json_t *obj1 = json_pack("{s:s, s:s}",
                             JSON_COLLECTION_KEY,  "/a/b",
                             JSON_DATA_OBJECT_KEY, "c.txt");
    baton_error_t error_obj1;
    ck_assert_str_eq(json_to_path(obj1, &error_obj1), obj_path);
    ck_assert_int_eq(error_obj1.code, 0);
    json_decref(obj1);

    // Slash-terminated collection
    json_t *obj2 = json_pack("{s:s, s:s}",
                             JSON_COLLECTION_KEY,  "/a/b/",
                             JSON_DATA_OBJECT_KEY, "c.txt");
    baton_error_t error_obj2;
    ck_assert_str_eq(json_to_path(obj2, &error_obj2), obj_path);
    ck_assert_int_eq(error_obj2.code, 0);
    json_decref(obj2);

    // No collection key:value
    json_t *malformed_obj = json_pack("{s:s}", JSON_DATA_OBJECT_KEY, "c.txt");

    baton_error_t error_obj3;
    ck_assert_ptr_eq(json_to_path(malformed_obj, &error_obj3), NULL);
    ck_assert_int_ne(error_obj3.code, 0);
    json_decref(malformed_obj);
}
END_TEST

// Can we convert JSON representation to a useful local path string?
START_TEST(test_json_to_local_path) {
    const char *file_name = "file1.txt";
    const char *file_path = "/file1/path";

    const char *obj_name = "obj1.txt";
    const char *coll_path = "/obj/path";

    json_t *path1 = json_pack("{s:s s:s s:s s:s}",
                              JSON_DIRECTORY_KEY,   file_path,
                              JSON_FILE_KEY,        file_name,
                              JSON_COLLECTION_KEY,  coll_path,
                              JSON_DATA_OBJECT_KEY, obj_name);

    baton_error_t error_path1;
    ck_assert_str_eq(json_to_local_path(path1, &error_path1),
                     "/file1/path/file1.txt");
    ck_assert_int_eq(error_path1.code, 0);
    json_decref(path1);

    json_t *path2 = json_pack("{s:s s:s s:s}",
                              JSON_FILE_KEY,        file_name,
                              JSON_COLLECTION_KEY,  coll_path,
                              JSON_DATA_OBJECT_KEY, obj_name);

    baton_error_t error_path2;
    ck_assert_str_eq(json_to_local_path(path1, &error_path2),
                     "./file1.txt");
    ck_assert_int_eq(error_path2.code, 0);
    json_decref(path2);

    json_t *path3 = json_pack("{s:s s:s}",
                              JSON_COLLECTION_KEY,  coll_path,
                              JSON_DATA_OBJECT_KEY, obj_name);

    baton_error_t error_path3;
    ck_assert_str_eq(json_to_local_path(path1, &error_path3),
                     "./obj1.txt");
    ck_assert_int_eq(error_path3.code, 0);
    json_decref(path3);
}
END_TEST

// Can we test JSON for the presence of an AVU?
START_TEST(test_contains_avu) {
    json_t *avu1 = json_pack("{s:s, s:s}",
                             JSON_ATTRIBUTE_KEY, "foo",
                             JSON_VALUE_KEY,     "bar");
    json_t *avu2 = json_pack("{s:s, s:s}",
                             JSON_ATTRIBUTE_KEY, "baz",
                             JSON_VALUE_KEY,     "qux");
    json_t *avu3 = json_pack("{s:s, s:s}",
                             JSON_ATTRIBUTE_KEY, "baz",
                             JSON_VALUE_KEY,     "zab");

    json_t *avus = json_pack("[o, o]", avu1, avu2);

    ck_assert(contains_avu(avus, avu1));
    ck_assert(contains_avu(avus, avu2));
    ck_assert(!contains_avu(avus, avu3));

    json_decref(avus);
}
END_TEST

// Can we test for JSON representation of a collection?
START_TEST(test_represents_coll) {
    json_t *col = json_pack("{s:s}", JSON_COLLECTION_KEY, "foo");
    json_t *obj = json_pack("{s:s, s:s}",
                            JSON_COLLECTION_KEY,  "foo",
                            JSON_DATA_OBJECT_KEY, "bar");

    ck_assert(represents_collection(col));
    ck_assert(!represents_collection(obj));

    json_decref(col);
    json_decref(obj);
}
END_TEST

// Can we test for JSON representation of a data object?
START_TEST(test_represents_data_obj) {
    json_t *col = json_pack("{s:s}", JSON_COLLECTION_KEY, "foo");
    json_t *obj = json_pack("{s:s, s:s}",
                            JSON_COLLECTION_KEY,  "foo",
                            JSON_DATA_OBJECT_KEY, "bar");

    ck_assert(!represents_data_object(col));
    ck_assert(represents_data_object(obj));

    json_decref(col);
    json_decref(obj);
}
END_TEST

// Can we test for JSON representation of a directory?
START_TEST(test_represents_dir) {
    json_t *dir  = json_pack("{s:s}", JSON_DIRECTORY_KEY, "foo");
    json_t *file = json_pack("{s:s, s:s}",
                             JSON_DIRECTORY_KEY, "foo",
                             JSON_FILE_KEY,      "bar");

    ck_assert(represents_directory(dir));
    ck_assert(!represents_directory(file));

    json_decref(dir);
    json_decref(file);
}
END_TEST

// Can we test for JSON representation of a file?
START_TEST(test_represents_file) {
    json_t *dir  = json_pack("{s:s}", JSON_DIRECTORY_KEY, "foo");
    json_t *file = json_pack("{s:s, s:s}",
                             JSON_DIRECTORY_KEY, "foo",
                             JSON_FILE_KEY,      "bar");

    ck_assert(!represents_file(dir));
    ck_assert(represents_file(file));

    json_decref(dir);
    json_decref(file);
}
END_TEST

// Can we read a data object and write to a stream?
START_TEST(test_get_data_obj_stream) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/lorem_10k.txt", rods_root);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    FILE *tmp = tmpfile();
    ck_assert_ptr_ne(NULL, tmp);

    size_t buffer_size = 1024;
    baton_error_t error;
    int num_written =
        get_data_obj_stream(conn, &rods_obj_path, tmp, buffer_size, &error);
    ck_assert_int_eq(num_written, 10240);
    ck_assert_int_eq(error.code, 0);

    rewind(tmp);
    confirm_checksum(tmp, "4efe0c1befd6f6ac4621cbdb13241246");
    fclose(tmp);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we read a data object into a UTF-8 string?
START_TEST(test_slurp_data_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/lorem_10k.txt", rods_root);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    // Test a range of buffer sizes, both smaller and larger than the
    // data
    size_t buffer_sizes[10] = {    1,  128,  256,   512,  1024,
                                2048, 4096, 8192, 16384, 37268 };
    for (int i = 0; i < 10; i++) {
        baton_error_t open_error;
        data_obj_file_t *obj = open_data_obj(conn, &rods_obj_path,
                                             O_RDONLY, flags, &open_error);
        ck_assert_int_eq(open_error.code, 0);

        baton_error_t slurp_error;
        char *data = slurp_data_obj(conn, obj, buffer_sizes[i],
                                    &slurp_error);
        ck_assert_int_eq(slurp_error.code, 0);
        ck_assert_int_eq(strnlen(data, 10240), 10240);
        ck_assert_str_eq(obj->md5_last_read,
                         "4efe0c1befd6f6ac4621cbdb13241246");

        ck_assert_int_eq(close_data_obj(conn, obj), 0);

        if (data) free(data);
    }

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we ingest a data object as JSON?
START_TEST(test_ingest_data_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/lorem_10k.txt", rods_root);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    // Test a range of buffer sizes, both smaller and larger than the
    // data
    size_t buffer_sizes[10] = {    1,  128,  256,   512,  1024,
                                2048, 4096, 8192, 16384, 37268 };
    for (int i = 0; i < 10; i++) {
        baton_error_t error;
        json_t *obj = ingest_data_obj(conn, &rods_obj_path, flags,
                                      buffer_sizes[i], &error);
        ck_assert_int_eq(error.code, 0);

        json_decref(obj);
    }

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_get_data_obj_file) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/lorem_10k.txt", rods_root);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    // Write data object to temp file
    char template[] = "baton_test_get_data_obj_file.XXXXXX";
    int fd = mkstemp(template);

    size_t buffer_size = 1024;
    baton_error_t error;
    int status = get_data_obj_file(conn, &rods_obj_path, template,
                                   buffer_size, &error);
    ck_assert_int_eq(error.code, 0);
    close(fd);

    // Check the MD5 of the tempfile
    FILE *tmp = fopen(template, "r");
    confirm_checksum(tmp, "4efe0c1befd6f6ac4621cbdb13241246");
    fclose(tmp);
    unlink(template);

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_write_data_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char file_path[MAX_PATH_LEN];
    snprintf(file_path, MAX_PATH_LEN, "%s/%s/lorem_10k.txt",
             TEST_ROOT, TEST_DATA_PATH);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/test_write_data_obj.txt", rods_root);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_error;
    resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                      flags, &resolve_error);

    // Test a range of buffer sizes, both smaller and larger than the
    // data
    size_t buffer_sizes[10] = {    1,  128,  256,   512,  1024,
                                2048, 4096, 8192, 16384, 37268 };

    for (int i = 0; i < 10; i++) {
        baton_error_t write_error;
        FILE *in = fopen(file_path, "r");
        size_t num_written = write_data_obj(conn, in, &rods_obj_path,
                                            buffer_sizes[i], flags,
                                            &write_error);
        ck_assert_int_eq(write_error.code, 0);
        ck_assert_int_eq(num_written, 10240);
        ck_assert_int_eq(fclose(in), 0);

        rodsPath_t result_obj_path;
        baton_error_t result_error;
        resolve_rods_path(conn, &env, &result_obj_path, obj_path,
                          flags, &result_error);
        ck_assert_int_eq(result_error.code, 0);

        baton_error_t list_error;
        json_t *result = list_path(conn, &result_obj_path, PRINT_CHECKSUM,
                                   &list_error);
        ck_assert_int_eq(list_error.code, 0);
        json_t *checksum = json_object_get(result, JSON_CHECKSUM_KEY);
        ck_assert(json_is_string(checksum));
        ck_assert_str_eq(json_string_value(checksum),
                         "4efe0c1befd6f6ac4621cbdb13241246");
        json_decref(result);

        // Get the data object to temp file
        char template[] = "baton_test_write_data_obj.XXXXXX";
        int fd = mkstemp(template);

        size_t buffer_size = 1024;
        baton_error_t get_error;
        int get_status = get_data_obj_file(conn, &result_obj_path, template,
                                           buffer_size, &get_error);
        ck_assert_int_eq(get_error.code, 0);
        ck_assert_int_eq(get_status, 0);
        close(fd);

        // Check the MD5 of the tempfile
        FILE *tmp = fopen(template, "r");
        confirm_checksum(tmp, "4efe0c1befd6f6ac4621cbdb13241246");
        fclose(tmp);
        unlink(template);
    }

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_put_data_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    char *md5 = "4efe0c1befd6f6ac4621cbdb13241246";

    char file_path[MAX_PATH_LEN];
    snprintf(file_path, MAX_PATH_LEN, "%s/%s/lorem_10k.txt",
             TEST_ROOT, TEST_DATA_PATH);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/test_put_data_obj.txt", rods_root);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_error;
    resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                      flags, &resolve_error);

    baton_error_t put_error;
    int put_status =
        put_data_obj(conn, file_path, &rods_obj_path, TEST_RESOURCE, md5,
                     flags | VERIFY_CHECKSUM,
                     &put_error);
    ck_assert_int_eq(put_error.code, 0);
    ck_assert_int_eq(put_status, 0);

    rodsPath_t result_obj_path;
    baton_error_t result_error;
    resolve_rods_path(conn, &env, &result_obj_path, obj_path,
                      flags, &result_error);
    ck_assert_int_eq(result_error.code, 0);

    baton_error_t list_error;
    json_t *result = list_path(conn, &result_obj_path, PRINT_CHECKSUM,
                               &list_error);
    ck_assert_int_eq(list_error.code, 0);
    json_t *checksum = json_object_get(result, JSON_CHECKSUM_KEY);
    ck_assert(json_is_string(checksum));
    ck_assert_str_eq(json_string_value(checksum), md5);
    json_decref(result);

    // Get the data object to temp file
    char template[] = "baton_test_put_data_obj.XXXXXX";
    int fd = mkstemp(template);

    size_t buffer_size = 1024;
    baton_error_t get_error;
    int get_status = get_data_obj_file(conn, &result_obj_path, template,
                                       buffer_size, &get_error);
    ck_assert_int_eq(get_error.code, 0);
    ck_assert_int_eq(get_status, 0);
    close(fd);

    // Check the MD5 of the tempfile
    FILE *tmp = fopen(template, "r");
    confirm_checksum(tmp, "4efe0c1befd6f6ac4621cbdb13241246");
    fclose(tmp);
    unlink(template);

    baton_error_t bad_checksum_error;
    int bad_checksum_status =
        put_data_obj(conn, file_path, &rods_obj_path, TEST_RESOURCE,
                     "dummy_bad_checksum",
                     flags | VERIFY_CHECKSUM,
                     &bad_checksum_error);

    // Work around iRODS bug
    // https://github.com/irods/irods/issues/5400 in versions up to
    // (and including) 4.2.8. For this test to pass, the server must
    // be >= 4.2.9
    baton_error_t version_error;
    char* version = get_server_version(conn, &version_error);
    if (str_equals(version, "4.2.7", MAX_VERSION_STR_LEN) ||
	str_equals(version, "4.2.8", MAX_VERSION_STR_LEN)) {
        fprintf(stderr, "Skipping put_data_obj bad checksum test on iRODS %d.%d.%d",
                IRODS_VERSION_MAJOR, IRODS_VERSION_MINOR, IRODS_VERSION_PATCHLEVEL);
    } else {
        ck_assert_int_eq(bad_checksum_error.code, USER_CHKSUM_MISMATCH);
        ck_assert_int_eq(bad_checksum_status, USER_CHKSUM_MISMATCH);
    }
    free(version);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we checksum a data object?
START_TEST(test_checksum_data_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char file_path[MAX_PATH_LEN];
    snprintf(file_path, MAX_PATH_LEN, "%s/%s/lorem_10k.txt",
             TEST_ROOT, TEST_DATA_PATH);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/test_checksum_data_obj.txt",
             rods_root);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_error;
    resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                      flags, &resolve_error);
    ck_assert_int_eq(resolve_error.code, 0);

    baton_error_t put_error;
    int put_status = put_data_obj(conn, file_path, &rods_obj_path,
                                  TEST_RESOURCE, NULL, flags, &put_error);
    ck_assert_int_eq(put_error.code, 0);
    ck_assert_int_eq(put_status, 0);

    rodsPath_t result_obj_path;
    baton_error_t result_error;
    resolve_rods_path(conn, &env, &result_obj_path, obj_path,
                      flags, &result_error);
    ck_assert_int_eq(result_error.code, 0);

    baton_error_t list_error;
    json_t *result = list_path(conn, &result_obj_path, PRINT_CHECKSUM,
                               &list_error);
    ck_assert_int_eq(list_error.code, 0);
    json_t *checksum = json_object_get(result, JSON_CHECKSUM_KEY);

    ck_assert_ptr_eq(checksum, NULL);
    json_decref(result);

    baton_error_t flag_conflict_error;
    checksum_data_obj(conn, &result_obj_path,
                      flags | CALCULATE_CHECKSUM | VERIFY_CHECKSUM,
                      &flag_conflict_error);
    ck_assert_int_ne(flag_conflict_error.code, 0);

    baton_error_t checksum_error;
    checksum_data_obj(conn, &result_obj_path, flags | CALCULATE_CHECKSUM,
                      &checksum_error);
    ck_assert_int_eq(checksum_error.code, 0);

    result = list_path(conn, &result_obj_path, PRINT_CHECKSUM, &list_error);
    ck_assert_int_eq(list_error.code, 0);
    checksum = json_object_get(result, JSON_CHECKSUM_KEY);

    ck_assert(json_is_string(checksum));
    ck_assert_str_eq(json_string_value(checksum),
                     "4efe0c1befd6f6ac4621cbdb13241246");
    json_decref(result);
}
END_TEST

// Can we checksum an object, ignoring stale replicas
START_TEST(test_checksum_ignore_stale) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_name[38] = "lorem_10k.txt";
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/%s",
             rods_root, obj_name);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_error;
    resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                      flags, &resolve_error);
    ck_assert_int_eq(resolve_error.code, 0);

    char command[MAX_COMMAND_LEN];
    // change checksum of replica 1 without marking stale
    snprintf(command, MAX_COMMAND_LEN, "%s/%s -c %s -d %s -a", TEST_ROOT,
             BAD_REPLICA_SCRIPT, rods_root, obj_name);

    int ret = system(command);
    if (ret != 0) raise(SIGTERM);

    baton_error_t wrong_checksum_error;
    json_t *result = list_path(conn, &rods_obj_path, PRINT_CHECKSUM,
                               &wrong_checksum_error);
    ck_assert_int_ne(wrong_checksum_error.code, 0);

    // mark replica 1 as stale
    snprintf(command, MAX_COMMAND_LEN, "%s/%s -c %s -d %s -s", TEST_ROOT,
             BAD_REPLICA_SCRIPT, rods_root, obj_name);

    ret = system(command);
    if (ret != 0) raise(SIGTERM);

    baton_error_t stale_replica_error;
    result = list_path(conn, &rods_obj_path, PRINT_CHECKSUM,
                       &stale_replica_error);
    ck_assert_int_eq(stale_replica_error.code, 0);

    json_decref(result);

}
END_TEST

// Can we remove a data object
START_TEST(test_remove_data_obj) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    int resolve_status =
        resolve_rods_path(conn, &env, &rods_path, obj_path,
                          flags, &resolve_error);
    ck_assert_int_eq(resolve_error.code, 0);
    ck_assert_int_eq(resolve_status, EXIST_ST);
    ck_assert_int_eq(rods_path.objType, DATA_OBJ_T);

    baton_error_t remove_error;
    int remove_status =
        remove_data_object(conn, &rods_path, flags, &remove_error);
    ck_assert_int_eq(remove_error.code, 0);
    ck_assert_int_eq(remove_status, 0);

    resolve_status =
        resolve_rods_path(conn, &env, &rods_path, obj_path,
                          flags, &resolve_error);
    ck_assert_int_eq(resolve_error.code, 0);
    ck_assert_int_ne(resolve_status, EXIST_ST);
    ck_assert_int_ne(rods_path.objType, DATA_OBJ_T);
}
END_TEST

// Can we create a collection?
START_TEST(test_create_coll) {
    option_flags flags = RECURSIVE;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char coll_path[MAX_PATH_LEN];
    snprintf(coll_path, MAX_PATH_LEN, "%s/a/b/c", rods_root);

    rodsPath_t rods_coll_path;
    baton_error_t resolve_error;
    int resolve_status =
        resolve_rods_path(conn, &env, &rods_coll_path, coll_path,
                          flags, &resolve_error);
    ck_assert_int_eq(resolve_error.code, 0);
    ck_assert_int_ne(resolve_status, EXIST_ST);
    ck_assert_int_ne(rods_coll_path.objType, COLL_OBJ_T); // Not present

    baton_error_t create_error;
    int create_status =
        create_collection(conn, &rods_coll_path, flags, &create_error);
    ck_assert_int_eq(create_error.code, 0);
    ck_assert_int_eq(create_status, 0);

    resolve_rods_path(conn, &env, &rods_coll_path, coll_path,
                      flags, &resolve_error);

    ck_assert_int_eq(rods_coll_path.objType, COLL_OBJ_T); // Created
}
END_TEST

// Can we remove a collection?
START_TEST(test_remove_coll) {
    option_flags flags = RECURSIVE;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    char coll_path[MAX_PATH_LEN];
    snprintf(coll_path, MAX_PATH_LEN, "%s/a/b/c", rods_root);

    rodsPath_t rods_coll_path;
    baton_error_t resolve_error;
    resolve_rods_path(conn, &env, &rods_coll_path, coll_path,
                      flags, &resolve_error);
    ck_assert_int_eq(resolve_error.code, 0);

    baton_error_t create_error;
    int create_status =
        create_collection(conn, &rods_coll_path, flags, &create_error);
    ck_assert_int_eq(create_error.code, 0);
    ck_assert_int_eq(create_status, 0);

    resolve_rods_path(conn, &env, &rods_coll_path, coll_path,
                      flags, &resolve_error);
    ck_assert_int_eq(rods_coll_path.objType, COLL_OBJ_T); // Present

    baton_error_t remove_error;
    int remove_status =
        remove_collection(conn, &rods_coll_path, flags, &remove_error);
    ck_assert_int_eq(remove_error.code, 0);
    ck_assert_int_eq(remove_status, 0);

    resolve_rods_path(conn, &env, &rods_coll_path, coll_path,
                      flags, &resolve_error);
    ck_assert_int_eq(resolve_error.code, 0);
    ck_assert_int_ne(rods_coll_path.objType, COLL_OBJ_T); // Not present
}
END_TEST

// Can we do a sequence of baton operations described by a JSON
// stream?
START_TEST(test_do_operation) {
    option_flags flags = SEARCH_OBJECTS | REMOVE_AVU;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    FILE *json_tmp     = tmpfile();
    char *zone_name    = NULL;
    size_t buffer_size = 1024;
    int num_ops        = 4;
    baton_json_op ops[4] = { baton_json_list_op,
                             baton_json_chmod_op,
                             baton_json_metaquery_op,
                             baton_json_metamod_op
                             // baton_json_get_op,
                             // baton_json_put_op
                            };

    // Arbitrarily use 2 correct objects,
    json_t *perm = json_pack("{s:s, s:s, s:s}",
                             JSON_OWNER_KEY, env.rodsUserName,
                             JSON_ZONE_KEY,  env.rodsZone,
                             JSON_LEVEL_KEY, ACCESS_LEVEL_OWN);
    json_t *avu = json_pack("{s:s, s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1",
                            JSON_UNITS_KEY,     "units1");
    json_t *obj1 = json_pack("{s:s, s:s, s:[O], s:[O]}",
                             JSON_COLLECTION_KEY,  rods_root,
                             JSON_DATA_OBJECT_KEY, "f1.txt",
                             JSON_ACCESS_KEY,      perm,
                             JSON_AVUS_KEY,        avu);
    json_t *obj2 = json_pack("{s:s, s:s, s:[O], s:[O]}",
                             JSON_COLLECTION_KEY,  rods_root,
                             JSON_DATA_OBJECT_KEY, "f2.txt",
                             JSON_ACCESS_KEY,      perm,
                             JSON_AVUS_KEY,        avu);
    json_t *incorrect_obj = json_pack("{s:s, s:s}",
                                      JSON_COLLECTION_KEY,  rods_root,
                                      JSON_DATA_OBJECT_KEY, "INVALID");

    json_dumpf(obj1, json_tmp, 0);
    json_dumpf(obj2, json_tmp, 0);

    operation_args_t args = { .flags       = flags,
                              .buffer_size = buffer_size,
                              .zone_name   = zone_name };

    for (int i = 0; i < num_ops; i++) {
        rewind(json_tmp);
        int pass_status = do_operation(json_tmp, ops[i], &args);
        ck_assert_int_eq(pass_status, 0);
    }

    // Add JSON for a non-existent file; should fail
    json_dumpf(incorrect_obj, json_tmp, 0);

    rewind(json_tmp);
    int fail_status = do_operation(json_tmp, ops[0], &args);
    ck_assert_int_ne(fail_status, 0);

    fclose(json_tmp);
    json_tmp = NULL;

    json_decref(obj1);
    json_decref(obj2);
    json_decref(incorrect_obj);
    json_decref(perm);
    json_decref(avu);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Tests that the `irods_get_sql_for_specific_alias` method can be
// used to get the SQL associated to a given alias.
START_TEST(test_irods_get_sql_for_specific_alias_with_alias) {
    if (!have_rodsadmin()) {
        logmsg(WARN, "!!! Skipping specific query tests because we are "
               "not rodsadmin !!!");
        return;
    }
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    // This is an alias that is inside the specific query table
    const char *valid_alias = "dataModifiedIdOnly";
    const char *sql = irods_get_sql_for_specific_alias(conn, valid_alias);

    ck_assert_ptr_ne(sql, NULL);
    ck_assert_str_eq(sql,
                     "SELECT DISTINCT Data.data_id AS data_id FROM R_DATA_MAIN Data WHERE CAST(Data.modify_ts AS INT) > CAST(? AS INT) AND CAST(Data.modify_ts AS INT) <= CAST(? AS INT)");

    if (conn) rcDisconnect(conn);
}
END_TEST

// Tests that when the SQL associated to a non-existent alias is
// requested using the `irods_get_sql_for_specific_alias` method, null
// pointer is returned.
START_TEST(test_irods_get_sql_for_specific_alias_with_non_existent_alias) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    const char *invalid_alias = "invalidAlias";
    const char *sql = irods_get_sql_for_specific_alias(conn, invalid_alias);

    // Expect nullptr
    ck_assert_ptr_eq(sql, NULL);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Tests that `make_query_format_from_sql` correctly parses a simple
// SQL SELECT query.
START_TEST(test_make_query_format_from_sql_with_simple_select_query) {
    char *sql = "SELECT a, b, c from some_table";
    query_format_in_t *format = make_query_format_from_sql(sql);

    ck_assert_ptr_ne(format, NULL);
    ck_assert_int_eq(format->num_columns, 3);
    ck_assert_str_eq(format->labels[0], "a");
    ck_assert_str_eq(format->labels[1], "b");
    ck_assert_str_eq(format->labels[2], "c");
}
END_TEST

// Tests that `make_query_format_from_sql` correctly parses an SQL
// SELECT query that uses an alias.
START_TEST(test_make_query_format_from_sql_with_select_query_using_column_alias) {
    char *sql = "SELECT a as b from some_table";
    query_format_in_t *format = make_query_format_from_sql(sql);

    ck_assert_ptr_ne(format, NULL);
    ck_assert_int_eq(format->num_columns, 1);
    int number_of_labels = sizeof(format->labels[0]) / sizeof(char*);
    ck_assert_int_eq(number_of_labels, 1);
    ck_assert_str_eq(format->labels[0], "b");
}
END_TEST

// Tests that `make_query_format_from_sql` returns a null pointer if
// it is used to parse an invalid SQL statement.
START_TEST(test_make_query_format_from_sql_with_invalid_query) {
    char *sql = "INVALID";
    query_format_in_t *format = make_query_format_from_sql(sql);

    // Expecting nullptr
    ck_assert_ptr_eq(format, NULL);
}
END_TEST

// Tests that the `search_specific` method can be used with a valid
// setup.
START_TEST(test_search_specific_with_valid_setup) {
    if (!have_rodsadmin()) {
        logmsg(WARN, "!!! Skipping specific query tests because we are "
               "not rodsadmin !!!");
        return;
    }
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);
    baton_error_t search_error;

    char *zone_name = NULL;

    json_t *query_json = json_pack("{s: {s:[s], s:s}}",
                                   JSON_SPECIFIC_KEY,
                                   JSON_ARGS_KEY, "dataModifiedIdOnly",
                                   JSON_SQL_KEY,  "findQueryByAlias");
    ck_assert_ptr_ne(query_json, NULL);

    json_t *search_results =
        search_specific(conn, query_json, zone_name, &search_error);
    ck_assert_ptr_ne(search_results, NULL);

    char *search_results_str =
        json_dumps(search_results, JSON_COMPACT | JSON_SORT_KEYS);
    ck_assert_str_eq(search_results_str,
                     "[{\"alias\":\"dataModifiedIdOnly\",\"sqlStr\":\"SELECT DISTINCT Data.data_id AS data_id FROM R_DATA_MAIN Data WHERE CAST(Data.modify_ts AS INT) > CAST(? AS INT) AND CAST(Data.modify_ts AS INT) <= CAST(? AS INT)\"}]");

    free(search_results_str);
    json_decref(search_results);
    json_decref(query_json);

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_exit_flag_on_sigint) {
    apply_signal_handler();
    raise(SIGINT);
    ck_assert(exit_flag == 2);
}
END_TEST

START_TEST(test_exit_flag_on_sigterm) {
    apply_signal_handler();
    raise(SIGTERM);
    ck_assert(exit_flag == 5);
}
END_TEST

// Having metadata on an item of (a = x, a = y), a search for "a = x"
// gives correct results, as does a search for "a = y". However,
// searching for "a = x and a = y" does not (nothing is returned).
//
// This is caused by overzealous cropping of search terms introduced
// in commit d6d036
START_TEST(test_regression_github_issue83) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    json_t *avu1 = json_pack("{s:s, s:s}",
                             JSON_ATTRIBUTE_KEY, "a",
                             JSON_VALUE_KEY,     "x");
    json_t *avu2 = json_pack("{s:s, s:s}",
                             JSON_ATTRIBUTE_KEY, "a",
                             JSON_VALUE_KEY,     "y");

    json_t *avu3 = json_pack("{s:s, s:s}",
                             JSON_ATTRIBUTE_KEY, "b",
                             JSON_VALUE_KEY,     "x");
    json_t *avu4 = json_pack("{s:s, s:s}",
                             JSON_ATTRIBUTE_KEY, "b",
                             JSON_VALUE_KEY,     "y");
    json_t *avu5 = json_pack("{s:s, s:s}",
                             JSON_ATTRIBUTE_KEY, "b",
                             JSON_VALUE_KEY,     "z");

    flags = SEARCH_OBJECTS;
    json_t *expected = json_pack("[{s:s, s:s}]",
                                 JSON_COLLECTION_KEY, rods_path.outPath,
                                 JSON_DATA_OBJECT_KEY, "r1.txt");

    // 2 AVUs, the base case.
    json_t *query1 = json_pack("{s:s, s:[o, o]}",
                               JSON_COLLECTION_KEY, rods_path.outPath,
                               JSON_AVUS_KEY,
                               avu1, avu2);
    baton_error_t error1;
    json_t *results1 = search_metadata(conn, query1, NULL, flags, &error1);

    ck_assert_ptr_ne(NULL, results1);
    ck_assert_int_eq(json_equal(results1, expected), 1);
    ck_assert_int_eq(error1.code, 0);

    // 3 AVUs, or any greater number should succeed too.
    json_t *query2 = json_pack("{s:s, s:[o, o, o]}",
                               JSON_COLLECTION_KEY, rods_path.outPath,
                               JSON_AVUS_KEY,
                               avu3, avu4, avu5);
    baton_error_t error2;
    json_t *results2 = search_metadata(conn, query2, NULL, flags, &error2);

    ck_assert_ptr_ne(NULL, results2);
    ck_assert_int_eq(json_equal(results2, expected), 1);
    ck_assert_int_eq(error2.code, 0);

    json_decref(query1);
    json_decref(results1);

    json_decref(query2);
    json_decref(results2);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Are all the search operators accepted? "n>=", "n<=" were not being
// accepted by user input validation.
START_TEST(test_regression_github_issue137) {
    option_flags flags = SEARCH_OBJECTS;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    // Not testing 'in' here
    char *operators[] = { "=", "like", "not like", ">", "<",
                          "n>", "n<", ">=", "<=", "n>=", "n<=" };

    for (size_t i = 0; i < 11; i++) {
        json_t *avu = json_pack("{s:s, s:s, s:s}",
                                JSON_ATTRIBUTE_KEY, "numattr1",
                                JSON_VALUE_KEY,     "10",
                                JSON_OPERATOR_KEY,  operators[i]);
        json_t *query = json_pack("{s:[o]}", JSON_AVUS_KEY, avu);

        baton_error_t error;
        json_t *results = search_metadata(conn, query, NULL, flags, &error);
        ck_assert_msg(error.code == 0, "failed: %s", operators[i]);

        json_decref(query);
        json_decref(results);
    }

    // Test 'in' here
    json_t *avu = json_pack("{s:s, s:[s], s:s}",
                            JSON_ATTRIBUTE_KEY, "numattr1",
                            JSON_VALUE_KEY,     "10",
                            JSON_OPERATOR_KEY,  "in");
    json_t *query = json_pack("{s:[o]}", JSON_AVUS_KEY, avu);

    baton_error_t error;
    json_t *results = search_metadata(conn, query, NULL, flags, &error);
    ck_assert_msg(error.code == 0, "in");
    json_decref(query);
    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Ensure that we can create JSON representing replicates that have no
// checksum
START_TEST(test_regression_github_issue140) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path_in[MAX_PATH_LEN];
    snprintf(obj_path_in, MAX_PATH_LEN, "%s/f1.txt", rods_root);
    char obj_path_out[MAX_PATH_LEN];
    snprintf(obj_path_out, MAX_PATH_LEN, "%s/f1.txt.no_checksum", rods_root);

    // A Plain `icp' will create a copy having no checksum
    char command[MAX_COMMAND_LEN];
    snprintf(command, MAX_COMMAND_LEN, "icp %s %s", obj_path_in, obj_path_out);

    int ret = system(command);
    if (ret != 0) raise(SIGTERM);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path_out,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_replicates(conn, &rods_path, &error);

    ck_assert_int_eq(error.code, 0);
    ck_assert(json_is_array(results));

    size_t index;
    json_t *result;
    json_array_foreach(results, index, result) {
        ck_assert(json_is_object(result));
        ck_assert(json_object_get(result, JSON_LOCATION_KEY));
        ck_assert(json_object_get(result, JSON_RESOURCE_KEY));
        ck_assert(json_object_get(result, JSON_REPLICATE_STATUS_KEY));
        ck_assert(json_object_get(result, JSON_CHECKSUM_KEY));
        ck_assert(json_equal(json_object_get(result, JSON_CHECKSUM_KEY),
                             json_null()));
    }

    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// baton-do sometimes fails to print a document for the checksum
// operation. Investigating this led to finding an error where
// checksum JSON was overzealously freed. Fixing that also appeared to
// fix the bug, but I'm not 100% sure it is the root cause.
START_TEST(test_regression_github_issue242) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path_in[MAX_PATH_LEN];
    snprintf(obj_path_in, MAX_PATH_LEN, "%s/f1.txt", rods_root);
    char obj_path_out[MAX_PATH_LEN];
    snprintf(obj_path_out, MAX_PATH_LEN, "%s/f1.txt.no_checksum", rods_root);

    // A Plain `icp' will create a copy having no checksum
    char command[MAX_COMMAND_LEN];
    snprintf(command, MAX_COMMAND_LEN, "icp %s %s", obj_path_in, obj_path_out);

    int ret = system(command);
    if (ret != 0) raise(SIGTERM);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path_out,
                                       flags, &resolve_error), EXIST_ST);

    json_t *target = json_pack("{s:s, s:s}",
                               JSON_COLLECTION_KEY,  rods_root,
                               JSON_DATA_OBJECT_KEY, "f1.txt.no_checksum");
    operation_args_t args = { .flags            = 0,
                              .buffer_size      =  1024 * 64 * 16 * 2,
                              .zone_name        = "testZone",
                              .max_connect_time = DEFAULT_MAX_CONNECT_TIME};

    baton_error_t error;
    json_t *result = baton_json_checksum_op(&env, conn, target, &args, &error);
    ck_assert_int_eq(error.code, 0);
    ck_assert(json_is_object(result));
    ck_assert(json_object_get(result, JSON_CHECKSUM_KEY));
    ck_assert(json_equal(json_object_get(result, JSON_CHECKSUM_KEY),
                         json_string("d41d8cd98f00b204e9800998ecf8427e")));
}
END_TEST

START_TEST(test_regression_github_issue252) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(TEST_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/lorem_1k.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    // Remove read access to trigger the bug
    char ichmod_r[MAX_COMMAND_LEN];
    snprintf(ichmod_r, MAX_COMMAND_LEN, "ichmod null %s %s",
             env.rodsUserName, obj_path);

    int ret = system(ichmod_r);
    ck_assert_msg(ret == 0, ichmod_r);

    baton_error_t sver_error;
    char *server_version = get_server_version(conn, &sver_error);
    ck_assert_int_eq(sver_error.code, 0);

    baton_error_t open_error;
    data_obj_file_t *obj = open_data_obj(conn, &rods_path,
                                         O_RDONLY, 0, &open_error);

    // iRODS 4.2.7 lets you "open" a data object you can't read, but you
    // can't get bytes from it, giving the following error
    if (str_equals(server_version, "4.2.7", MAX_STR_LEN)) {
        baton_error_t slurp_error;
        slurp_data_obj(conn, obj, 512, &slurp_error);

        // Furthermore, iRODS 4.2.7 gives the inappropriate error
        // -190000 SYS_FILE_DESC_OUT_OF_RANGE if it gets this far.
        ck_assert_int_eq(slurp_error.code, -19000);
        ck_assert_int_eq(close_data_obj(conn, obj), -19000);
    } else {
        ck_assert_int_eq(open_error.code, -818000);
    }

    if (conn) rcDisconnect(conn);
}
END_TEST

Suite *baton_suite(void) {
    Suite *suite = suite_create("baton");

    TCase *utilities = tcase_create("utilities");
    tcase_add_test(utilities, test_str_equals);
    tcase_add_test(utilities, test_str_equals_ignore_case);
    tcase_add_test(utilities, test_str_starts_with);
    tcase_add_test(utilities, test_str_ends_with);
    tcase_add_test(utilities, test_parse_base_name);
    tcase_add_test(utilities, test_maybe_stdin);
    tcase_add_test(utilities, test_format_timestamp);
    tcase_add_test(utilities, test_parse_timestamp);
    tcase_add_test(utilities, test_parse_size);
    tcase_add_test(utilities, test_to_utf8);

    TCase *basic = tcase_create("basic");
    tcase_add_unchecked_fixture(basic, setup, teardown);
    tcase_add_checked_fixture(basic, basic_setup, basic_teardown);

    tcase_add_test(basic, test_get_version);
    tcase_add_test(basic, test_rods_login);
    tcase_add_test(basic, test_is_irods_available);
    tcase_add_test(basic, test_init_rods_path);
    tcase_add_test(basic, test_resolve_rods_path);
    tcase_add_test(basic, test_make_query_input);
    
    TCase *path = tcase_create("path");
    tcase_add_unchecked_fixture(path, setup, teardown);
    tcase_add_checked_fixture(path, basic_setup, basic_teardown);

    tcase_add_test(path, test_list_missing_path);
    tcase_add_test(path, test_list_obj);
    tcase_add_test(path, test_list_coll);
    tcase_add_test(path, test_list_coll_contents);
    tcase_add_test(path, test_list_permissions_missing_path);
    tcase_add_test(path, test_list_permissions_obj);
    tcase_add_test(path, test_list_permissions_coll);
    tcase_add_test(path, test_modify_permissions_obj);
    tcase_add_test(path, test_modify_json_permissions_obj);
    tcase_add_test(path, test_list_replicates_obj);
    tcase_add_test(path, test_list_timestamps_obj);
    tcase_add_test(path, test_list_timestamps_coll);

    TCase *metadata = tcase_create("metadata");
    tcase_add_unchecked_fixture(metadata, setup, teardown);
    tcase_add_checked_fixture(metadata, basic_setup, basic_teardown);

    tcase_add_test(metadata, test_list_metadata_obj);
    tcase_add_test(metadata, test_list_metadata_coll);
    tcase_add_test(metadata, test_contains_avu);
    tcase_add_test(metadata, test_add_metadata_missing_path);
    tcase_add_test(metadata, test_add_metadata_obj);
    tcase_add_test(metadata, test_remove_metadata_obj);
    tcase_add_test(metadata, test_add_json_metadata_obj);
    tcase_add_test(metadata, test_remove_json_metadata_obj);
    tcase_add_test(metadata, test_search_metadata_obj);
    tcase_add_test(metadata, test_search_metadata_coll);
    tcase_add_test(metadata, test_search_metadata_path_obj);
    tcase_add_test(metadata, test_search_metadata_perm_obj);
    tcase_add_test(metadata, test_search_metadata_tps_obj);

    TCase *read_write = tcase_create("read_write");
    tcase_add_unchecked_fixture(read_write, setup, teardown);
    tcase_add_checked_fixture(read_write, basic_setup, basic_teardown);

    tcase_add_test(read_write, test_get_data_obj_stream);
    tcase_add_test(read_write, test_get_data_obj_file);
    tcase_add_test(read_write, test_slurp_data_obj);
    tcase_add_test(read_write, test_ingest_data_obj);
    tcase_add_test(read_write, test_write_data_obj);
    tcase_add_test(read_write, test_put_data_obj);
    tcase_add_test(read_write, test_checksum_data_obj);
    tcase_add_test(read_write, test_checksum_ignore_stale);
    tcase_add_test(read_write, test_remove_data_obj);
    tcase_add_test(read_write, test_create_coll);
    tcase_add_test(read_write, test_remove_coll);

    TCase *json = tcase_create("json");
    tcase_add_unchecked_fixture(json, setup, teardown);
    tcase_add_checked_fixture(json, basic_setup, basic_teardown);

    tcase_add_test(json, test_represents_coll);
    tcase_add_test(json, test_represents_data_obj);
    tcase_add_test(json, test_represents_dir);
    tcase_add_test(json, test_represents_file);
    tcase_add_test(json, test_json_to_path);
    tcase_add_test(json, test_json_to_local_path);
    tcase_add_test(json, test_do_operation);

    TCase *specific_query = tcase_create("specific_query");
    tcase_add_unchecked_fixture(specific_query, setup, teardown);
    tcase_add_checked_fixture(specific_query, basic_setup, basic_teardown);

    tcase_add_test(specific_query,
                   test_irods_get_sql_for_specific_alias_with_alias);
    tcase_add_test(specific_query,
                   test_irods_get_sql_for_specific_alias_with_non_existent_alias);
    tcase_add_test(specific_query,
                   test_make_query_format_from_sql_with_simple_select_query);
    tcase_add_test(specific_query,
                   test_make_query_format_from_sql_with_select_query_using_column_alias);
    tcase_add_test(specific_query,
                   test_make_query_format_from_sql_with_invalid_query);
    tcase_add_test(specific_query,
                   test_search_specific_with_valid_setup);

    TCase *signal_handler = tcase_create("signal_handler");
    tcase_add_unchecked_fixture(signal_handler, setup, teardown);
    tcase_add_checked_fixture(signal_handler, basic_setup, basic_teardown);
    tcase_add_test(signal_handler, test_exit_flag_on_sigint);
    tcase_add_test(signal_handler, test_exit_flag_on_sigterm);

    TCase *regression = tcase_create("regression");
    tcase_add_unchecked_fixture(regression, setup, teardown);
    tcase_add_checked_fixture(regression, basic_setup, basic_teardown);

    tcase_add_test(regression, test_regression_github_issue83);
    tcase_add_test(regression, test_regression_github_issue137);
    tcase_add_test(regression, test_regression_github_issue140);
    tcase_add_test(regression, test_regression_github_issue242);
    tcase_add_test(regression, test_regression_github_issue252);

    suite_add_tcase(suite, utilities);
    suite_add_tcase(suite, basic);
    suite_add_tcase(suite, path);
    suite_add_tcase(suite, metadata);
    suite_add_tcase(suite, read_write);
    suite_add_tcase(suite, json);
    suite_add_tcase(suite, specific_query);
    suite_add_tcase(suite, signal_handler);
    suite_add_tcase(suite, regression);

    return suite;
}

int main (void) {
    Suite *suite = baton_suite();

    SRunner *runner = srunner_create(suite);
    srunner_run_all(runner, CK_VERBOSE);

    int number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
