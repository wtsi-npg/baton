/**
 * Copyright (C) 2013, 2014, 2015 Genome Research Ltd. All rights
 * reserved.
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

static int MAX_COMMAND_LEN = 1024;
static int MAX_PATH_LEN    = 4096;

static char *BASIC_COLL          = "baton-basic-test";
static char *BASIC_DATA_PATH     = "data";
static char *BASIC_METADATA_PATH = "metadata/meta1.imeta";

static char *SETUP_SCRIPT    = "scripts/setup_irods.sh";
static char *TEARDOWN_SCRIPT = "scripts/teardown_irods.sh";

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
    set_log_threshold(WARN);
}

static void teardown() {

}

static void basic_setup() {
    char command[MAX_COMMAND_LEN];
    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    snprintf(command, MAX_COMMAND_LEN, "%s/%s %s/%s %s %s %s/%s",
             TEST_ROOT, SETUP_SCRIPT,
             TEST_ROOT, BASIC_DATA_PATH,
             rods_root,
             TEST_RESOURCE,
             TEST_ROOT, BASIC_METADATA_PATH);

    printf("Setup: %s\n", command);

    int ret = system(command);

    if (ret != 0) raise(SIGTERM);
}

static void basic_teardown() {
    char command[MAX_COMMAND_LEN];
    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    snprintf(command, MAX_COMMAND_LEN, "%s/%s %s",
             TEST_ROOT, TEARDOWN_SCRIPT,
             rods_root);

    printf("Teardown: %s\n", command);
    int ret = system(command);

    if (ret != 0) raise(SIGINT);
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
             TEST_ROOT, BASIC_DATA_PATH);

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

    char *invalid_path = '\0';
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
    set_current_rods_root(BASIC_COLL, rods_root);

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

    json_t *perm = json_pack("{s:s, s:s}",
                             JSON_OWNER_KEY, env.rodsUserName,
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

    json_decref(results1);
    json_decref(expected1);

    json_decref(results2);
    json_decref(expected2);

    json_decref(results3);
    json_decref(expected3);

    json_decref(results4);
    json_decref(expected4);

    json_decref(results5);

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
    set_current_rods_root(BASIC_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_path(conn, &rods_path, PRINT_ACL, &error);

    json_t *perms = json_pack("{s:s, s:s}",
                              JSON_OWNER_KEY, env.rodsUserName,
                              JSON_LEVEL_KEY, ACCESS_OWN);
    json_t *expected = json_pack("{s:s, s:[o]}",
                                 JSON_COLLECTION_KEY, rods_path.outPath,
                                 JSON_ACCESS_KEY,     perms);

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
    set_current_rods_root(BASIC_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_path(conn, &rods_path,
                                PRINT_SIZE | PRINT_ACL | PRINT_CONTENTS,
                                &error);

    char a[MAX_PATH_LEN];
    char b[MAX_PATH_LEN];
    char c[MAX_PATH_LEN];
    snprintf(a, MAX_PATH_LEN, "%s/a", rods_path.outPath);
    snprintf(b, MAX_PATH_LEN, "%s/b", rods_path.outPath);
    snprintf(c, MAX_PATH_LEN, "%s/c", rods_path.outPath);

    json_t *perms = json_pack("{s:s, s:s}",
                              JSON_OWNER_KEY, env.rodsUserName,
                              JSON_LEVEL_KEY, ACCESS_OWN);

    json_t *expected =
        json_pack("{s:s, s:[o],"
                  " s:[{s:s, s:s, s:i, s:[o]},"  // f1.txt
                  "    {s:s, s:s, s:i, s:[o]},"  // f2.txt
                  "    {s:s, s:s, s:i, s:[o]},"  // f3.txt
                  "    {s:s, s:s, s:i, s:[o]},"  // lorem_10k.txt
                  "    {s:s, s:s, s:i, s:[o]},"  // lorem_1b.txt
                  "    {s:s, s:s, s:i, s:[o]},"  // lorem_1k.txt
                  "    {s:s, s:s, s:i, s:[o]},"  // r1.txt
                  "    {s:s, s:[o]},"            // a
                  "    {s:s, s:[o]},"            // b
                  "    {s:s, s:[o]}]}",          // c
                  JSON_COLLECTION_KEY, rods_path.outPath,
                  JSON_ACCESS_KEY,     perms,

                  JSON_CONTENTS_KEY,
                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "f1.txt",
                  JSON_SIZE_KEY,        0,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "f2.txt",
                  JSON_SIZE_KEY,        0,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "f3.txt",
                  JSON_SIZE_KEY,        0,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "lorem_10k.txt",
                  JSON_SIZE_KEY,        10240,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "lorem_1b.txt",
                  JSON_SIZE_KEY,        1,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "lorem_1k.txt",
                  JSON_SIZE_KEY,        1024,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "r1.txt",
                  JSON_SIZE_KEY,        0,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  a,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  b,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  c,
                  JSON_ACCESS_KEY,      perms);

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
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_permissions(conn, &rods_path, &error);
    json_t *expected = json_pack("[{s:s, s:s}]",
                                 JSON_OWNER_KEY, env.rodsUserName,
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
    set_current_rods_root(BASIC_COLL, rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    json_t *results = list_permissions(conn, &rods_path, &error);
    json_t *expected = json_pack("[{s:s, s:s}]",
                                 JSON_OWNER_KEY, env.rodsUserName,
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
    set_current_rods_root(BASIC_COLL, rods_root);
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
    set_current_rods_root(BASIC_COLL, rods_root);
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
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    json_t *expected = json_pack("[{s:i, s:b}, {s:i, s:b}]",
                                 JSON_REPLICATE_NUMBER_KEY, 0,
                                 JSON_REPLICATE_STATUS_KEY, 1,
                                 JSON_REPLICATE_NUMBER_KEY, 1,
                                 JSON_REPLICATE_STATUS_KEY, 1);

    baton_error_t error;
    json_t *results = list_replicates(conn, &rods_path, &error);

    ck_assert_int_eq(error.code, 0);
    ck_assert(json_is_array(results));
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
    set_current_rods_root(BASIC_COLL, rods_root);
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
    set_current_rods_root(BASIC_COLL, rods_root);

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
    set_current_rods_root(BASIC_COLL, rods_root);

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
    set_current_rods_root(BASIC_COLL, rods_root);

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
    set_current_rods_root(BASIC_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t mod_error;
    init_baton_error(&mod_error);
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
    set_current_rods_root(BASIC_COLL, rods_root);

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

    char *iso_created = format_timestamp(raw_created, ISO8601_FORMAT);

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
    set_current_rods_root(BASIC_COLL, rods_root);

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
    set_current_rods_root(BASIC_COLL, rods_root);
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
    init_baton_error(&error);
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
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t error;
    init_baton_error(&error);
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
    set_current_rods_root(BASIC_COLL, rods_root);
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
    set_current_rods_root(BASIC_COLL, rods_root);
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
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    baton_error_t mod_error;
    init_baton_error(&mod_error);
    int rv = modify_permissions(conn, &rods_path, NO_RECURSE, "public",
                                ACCESS_LEVEL_READ, &mod_error);
    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(mod_error.code, 0);

    json_t *expected = json_pack("{s:s, s:s}",
                                 JSON_OWNER_KEY, "public",
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
    set_current_rods_root(BASIC_COLL, rods_root);
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
    json_t *perm = json_pack("{s:s, s:s}",
                             JSON_OWNER_KEY, "public",
                             JSON_LEVEL_KEY, ACCESS_LEVEL_READ);

    baton_error_t error;
    int rv = modify_json_permissions(conn, &rods_path, NO_RECURSE, perm,
                                     &error);
    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(error.code, 0);

    json_t *expected = json_pack("{s:s, s:s}",
                                 JSON_OWNER_KEY, "public",
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

    json_decref(bad_perm1);
    json_decref(bad_perm2);
    json_decref(perm);
    json_decref(expected);
    json_decref(acl);

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
START_TEST(test_represents_collection) {
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
START_TEST(test_represents_data_object) {
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
START_TEST(test_represents_directory) {
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

// Can we get user information?
START_TEST(test_get_user) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    baton_error_t expected_error;
    init_baton_error(&expected_error);
    json_t *bad_user = get_user(conn, "no_such_user", &expected_error);
    ck_assert_int_ne(expected_error.code, 0);
    ck_assert_ptr_eq(bad_user, NULL);

    baton_error_t error;
    init_baton_error(&error);
    json_t *user = get_user(conn, "public", &error);

    ck_assert_int_eq(error.code, 0);
    ck_assert_ptr_ne(NULL, json_object_get(user, JSON_USER_NAME_KEY));
    ck_assert_ptr_ne(NULL, json_object_get(user, JSON_USER_ID_KEY));
    ck_assert_ptr_ne(NULL, json_object_get(user, JSON_USER_TYPE_KEY));
    ck_assert_ptr_ne(NULL, json_object_get(user, JSON_USER_ZONE_KEY));

    json_decref(user);

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_write_path_to_stream) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/lorem_10k.txt", rods_root);

    rodsPath_t rods_obj_path;
    baton_error_t resolve_error;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_obj_path, obj_path,
                                       flags, &resolve_error), EXIST_ST);

    FILE *tmp = tmpfile();
    ck_assert_ptr_ne(NULL, tmp);

    size_t size = 1024;
    baton_error_t error;
    int status = write_path_to_stream(conn, &rods_obj_path, tmp, size, &error);
    ck_assert_int_eq(status, 10240);
    ck_assert_int_eq(error.code, 0);

    rewind(tmp);

    unsigned char digest[16];
    MD5_CTX context;
    MD5Init(&context);

    char buffer[1024];

    size_t n;
    while ((n = fread(buffer, 1, 1024, tmp)) > 0) {
        MD5Update(&context, (unsigned char *) buffer, n);
    }

    MD5Final(digest, &context);

    char *md5 = calloc(33, sizeof (char));
    for (int i = 0; i < 16; i++) {
        snprintf(md5 + i * 2, 3, "%02x", digest[i]);
    }

    ck_assert_str_eq(md5, "4efe0c1befd6f6ac4621cbdb13241246");

    fclose(tmp);
    tmp = NULL;

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_slurp_file) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

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
        init_baton_error(&open_error);

        data_obj_file_t *obj_file =
            open_data_obj(conn, &rods_obj_path, &open_error);
        ck_assert_int_eq(open_error.code, 0);

        baton_error_t slurp_error;
        init_baton_error(&slurp_error);

        char *data = slurp_data_object(conn, obj_file, buffer_sizes[i],
                                       &slurp_error);
        ck_assert_int_eq(slurp_error.code, 0);
        ck_assert_int_eq(strnlen(data, 10240), 10240);
        ck_assert_str_eq(obj_file->md5_last_read,
                         "4efe0c1befd6f6ac4621cbdb13241246");

        if (data) free(data);
    }

    if (conn) rcDisconnect(conn);
}
END_TEST

START_TEST(test_regression_github_issue83) {
    option_flags flags = 0;
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

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


Suite *baton_suite(void) {
    Suite *suite = suite_create("baton");

    TCase *utilities_tests = tcase_create("utilities");
    tcase_add_test(utilities_tests, test_str_equals);
    tcase_add_test(utilities_tests, test_str_equals_ignore_case);
    tcase_add_test(utilities_tests, test_str_starts_with);
    tcase_add_test(utilities_tests, test_str_ends_with);
    tcase_add_test(utilities_tests, test_parse_base_name);
    tcase_add_test(utilities_tests, test_maybe_stdin);
    tcase_add_test(utilities_tests, test_format_timestamp);
    tcase_add_test(utilities_tests, test_parse_timestamp);
    tcase_add_test(utilities_tests, test_parse_size);
    tcase_add_test(utilities_tests, test_to_utf8);

    TCase *basic_tests = tcase_create("basic");
    tcase_add_unchecked_fixture(basic_tests, setup, teardown);
    tcase_add_checked_fixture (basic_tests, basic_setup, basic_teardown);

    tcase_add_test(basic_tests, test_rods_login);
    tcase_add_test(basic_tests, test_is_irods_available);
    tcase_add_test(basic_tests, test_init_rods_path);
    tcase_add_test(basic_tests, test_resolve_rods_path);
    tcase_add_test(basic_tests, test_make_query_input);

    tcase_add_test(basic_tests, test_list_missing_path);
    tcase_add_test(basic_tests, test_list_obj);
    tcase_add_test(basic_tests, test_list_coll);
    tcase_add_test(basic_tests, test_list_coll_contents);

    tcase_add_test(basic_tests, test_list_permissions_missing_path);
    tcase_add_test(basic_tests, test_list_permissions_obj);
    tcase_add_test(basic_tests, test_list_permissions_coll);

    tcase_add_test(basic_tests, test_list_metadata_obj);
    tcase_add_test(basic_tests, test_list_metadata_coll);

    tcase_add_test(basic_tests, test_list_replicates_obj);

    tcase_add_test(basic_tests, test_list_timestamps_obj);
    tcase_add_test(basic_tests, test_list_timestamps_coll);

    tcase_add_test(basic_tests, test_search_metadata_obj);
    tcase_add_test(basic_tests, test_search_metadata_coll);
    tcase_add_test(basic_tests, test_search_metadata_path_obj);
    tcase_add_test(basic_tests, test_search_metadata_perm_obj);
    tcase_add_test(basic_tests, test_search_metadata_tps_obj);

    tcase_add_test(basic_tests, test_add_metadata_missing_path);
    tcase_add_test(basic_tests, test_remove_metadata_obj);
    tcase_add_test(basic_tests, test_add_json_metadata_obj);
    tcase_add_test(basic_tests, test_remove_json_metadata_obj);

    tcase_add_test(basic_tests, test_modify_permissions_obj);
    tcase_add_test(basic_tests, test_modify_json_permissions_obj);

    tcase_add_test(basic_tests, test_represents_collection);
    tcase_add_test(basic_tests, test_represents_data_object);
    tcase_add_test(basic_tests, test_represents_directory);
    tcase_add_test(basic_tests, test_represents_file);

    tcase_add_test(basic_tests, test_json_to_path);
    tcase_add_test(basic_tests, test_json_to_local_path);
    tcase_add_test(basic_tests, test_contains_avu);

    tcase_add_test(basic_tests, test_get_user);

    tcase_add_test(basic_tests, test_write_path_to_stream);
    tcase_add_test(basic_tests, test_slurp_file);

    TCase *regression_tests = tcase_create("regression");
    tcase_add_unchecked_fixture(regression_tests, setup, teardown);
    tcase_add_checked_fixture (regression_tests, basic_setup, basic_teardown);

    tcase_add_test(regression_tests, test_regression_github_issue83);

    suite_add_tcase(suite, utilities_tests);
    suite_add_tcase(suite, basic_tests);
    suite_add_tcase(suite, regression_tests);

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
