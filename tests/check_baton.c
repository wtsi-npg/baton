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
 * @file check_baton.c
 */

#include <assert.h>
#include <unistd.h>

#include <jansson.h>
#include <zlog.h>
#include <check.h>

#include "../src/baton.h"
#include "../src/json.h"

static int MAX_COMMAND_LEN = 1024;
static int MAX_PATH_LEN    = 4096;

static char *BASIC_COLL          = "baton-basic-test";
static char *BASIC_DATA_PATH     = "./data/";
static char *BASIC_METADATA_PATH = "./metadata/meta1.imeta";

static char *SETUP_SCRIPT    = "./scripts/setup_irods.sh";
static char *TEARDOWN_SCRIPT = "./scripts/teardown_irods.sh";

static void set_current_rods_root(char *in, char *out) {
    snprintf(out, MAX_PATH_LEN, "%s.%d", in, getpid());
}

static void setup() {
    zlog_init("../baton_zlog.conf");
}

static void teardown() {
    zlog_fini();
}

static void basic_setup() {
    char command[MAX_COMMAND_LEN];
    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    snprintf(command, MAX_COMMAND_LEN, "%s %s %s %s",
                      SETUP_SCRIPT, BASIC_DATA_PATH, rods_root,
                      BASIC_METADATA_PATH);

    printf("Setup: %s\n", command);
    int ret = system(command);

    if (ret != 0) raise(SIGINT);
}

static void basic_teardown() {
    char command[MAX_COMMAND_LEN];
    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    snprintf(command, MAX_COMMAND_LEN, "%s %s", TEARDOWN_SCRIPT, rods_root);

    printf("Teardown: %s\n", command);
    int ret = system(command);

    if (ret != 0) raise(SIGINT);
}

START_TEST(test_str_starts_with) {
    ck_assert_msg(str_starts_with("", ""),    "'' starts with ''");
    ck_assert_msg(str_starts_with("a", ""),   "'a' starts with ''");
    ck_assert_msg(str_starts_with("a", "a"),  "'a' starts with 'a'");
    ck_assert_msg(str_starts_with("ab", "a"), "'ab' starts with 'a'");

    ck_assert_msg(!str_starts_with("", "a"),   "'' !starts with 'a'");
    ck_assert_msg(!str_starts_with("b", "a"),  "'b' !starts with 'a'");
    ck_assert_msg(!str_starts_with("ba", "a"), "'ba' !starts with 'a'");
}
END_TEST

START_TEST(test_str_ends_with) {
    ck_assert_msg(str_ends_with("", ""),    "'' ends with ''");
    ck_assert_msg(str_ends_with("a", ""),   "'a' ends with ''");
    ck_assert_msg(str_ends_with("a", "a"),  "'a' ends with 'a'");
    ck_assert_msg(str_ends_with("ba", "a"), "'ba' ends with 'a'");

    ck_assert_msg(!str_ends_with("", "a"),   "'' !ends with 'a'");
    ck_assert_msg(!str_ends_with("b", "a"),  "'b' !ends with 'a'");
    ck_assert_msg(!str_ends_with("ab", "a"), "'ab' !ends with 'a'");
}
END_TEST

START_TEST(test_str_equals) {
    ck_assert_msg(str_equals("", ""),    "'' equals ''");
    ck_assert_msg(str_equals(" ", " "),  "' ' equals ' '");
    ck_assert_msg(str_equals("a", "a"),  "'a' equals 'a'");
    ck_assert_msg(!str_equals("a", "A"), "'a' !equals 'A'");
}
END_TEST

START_TEST(test_str_equals_ignore_case) {
    ck_assert_msg(str_equals_ignore_case("", ""),   "'' equals ''");
    ck_assert_msg(str_equals_ignore_case(" ", " "), "' ' equals ' '");
    ck_assert_msg(str_equals_ignore_case("a", "a"), "'a' equals 'a'");
    ck_assert_msg(str_equals_ignore_case("a", "A"), "'a' equals 'A'");
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
    rodsEnv env;
    rodsPath_t rods_path;

    rcComm_t *conn = rods_login(&env);
    char *path = "/";

    ck_assert_msg(is_irods_available(), "iRODS is not available");
    ck_assert(conn->loggedIn);
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, path), EXIST_ST);
    ck_assert_str_eq(rods_path.inPath, path);
    ck_assert_str_eq(rods_path.outPath, path);
    ck_assert_int_eq(rods_path.objType, COLL_OBJ_T);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we list a data object?
START_TEST(test_list_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root),
                     EXIST_ST);

    rodsPath_t rods_obj_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_obj_path, obj_path),
                     EXIST_ST);

    baton_error_t error;
    json_t *results = list_path(conn, &rods_obj_path, &error);

    json_t *perms = json_pack("{s:s, s:s}",
                              JSON_OWNER_KEY, env.rodsUserName,
                              JSON_LEVEL_KEY, ACCESS_OWN);
    json_t *expected = json_pack("{s:s, s:s, s:[o]}",
                                 JSON_COLLECTION_KEY,  rods_path.outPath,
                                 JSON_DATA_OBJECT_KEY, "f1.txt",
                                 JSON_ACCESS_KEY,      perms);

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we list a collection?
START_TEST(test_list_coll) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root),
                     EXIST_ST);

    baton_error_t error;
    json_t *results = list_path(conn, &rods_path, &error);

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
        json_pack("[{s:s, s:s, s:[o]},"  // f1.txt
                  " {s:s, s:s, s:[o]},"  // f2.txt
                  " {s:s, s:s, s:[o]},"  // f3.txt
                  " {s:s, s:[o]},"       // a
                  " {s:s, s:[o]},"       // b
                  " {s:s, s:[o]}]",      // c
                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "f1.txt",
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "f2.txt",
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  rods_path.outPath,
                  JSON_DATA_OBJECT_KEY, "f3.txt",
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  a,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  b,
                  JSON_ACCESS_KEY,      perms,

                  JSON_COLLECTION_KEY,  c,
                  JSON_ACCESS_KEY,      perms);

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

// Can we list the ACL of an object?
START_TEST(test_list_permissions_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path),
                     EXIST_ST);

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
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, rods_root),
                     EXIST_ST);

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
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path),
                     EXIST_ST);

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
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char coll_path[MAX_PATH_LEN];
    snprintf(coll_path, MAX_PATH_LEN, "%s/a", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, coll_path),
                     EXIST_ST);

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

// Can we search for data objects by their metadata?
START_TEST(test_search_metadata_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    json_t *avu = json_pack("{s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1");
    json_t *query = json_pack("{s:s, s:[o]}",
                              JSON_COLLECTION_KEY, rods_root,
                              JSON_AVUS_KEY,       avu);

    baton_error_t error;
    json_t *results = search_metadata(conn, query, NULL, &error);
    ck_assert_int_eq(json_array_size(results), 12);

    for (size_t i = 0; i < 12; i++) {
        json_t *obj = json_array_get(results, i);
        ck_assert_ptr_ne(json_object_get(obj, JSON_COLLECTION_KEY), NULL);
        ck_assert_ptr_ne(json_object_get(obj, JSON_DATA_OBJECT_KEY), NULL);
    }
    ck_assert_int_eq(error.code, 0);

    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we search for data objects by their metadata, limiting scope by path?
START_TEST(test_search_metadata_path_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char search_root[MAX_PATH_LEN];
    snprintf(search_root, MAX_PATH_LEN, "%s/a/x/m", rods_root);

    json_t *avu = json_pack("{s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1");
    json_t *query = json_pack("{s:s, s:[o]}",
                              JSON_COLLECTION_KEY, search_root,
                              JSON_AVUS_KEY,       avu);

    baton_error_t error;
    json_t *results = search_metadata(conn, query, NULL, &error);
    ck_assert_int_eq(json_array_size(results), 3);

    for (size_t i = 0; i < 3; i++) {
        json_t *obj = json_array_get(results, i);
        ck_assert_ptr_ne(json_object_get(obj, JSON_COLLECTION_KEY), NULL);
        ck_assert_ptr_ne(json_object_get(obj, JSON_DATA_OBJECT_KEY), NULL);
    }
    ck_assert_int_eq(error.code, 0);

    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we search for collections by their metadata?
START_TEST(test_search_metadata_coll) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    json_t *avu = json_pack("{s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr2",
                            JSON_VALUE_KEY,     "value2");
    json_t *query = json_pack("{s:s, s:[o]}",
                              JSON_COLLECTION_KEY, rods_root,
                              JSON_AVUS_KEY,       avu);

    baton_error_t error;
    json_t *results = search_metadata(conn, query, NULL, &error);
    ck_assert_int_eq(json_array_size(results), 3);
    for (size_t i = 0; i < 3; i++) {
        json_t *coll = json_array_get(results, i);
        ck_assert_ptr_ne(json_object_get(coll, JSON_COLLECTION_KEY), NULL);
        ck_assert_ptr_eq(json_object_get(coll, JSON_DATA_OBJECT_KEY), NULL);
    }
    ck_assert_int_eq(error.code, 0);

    json_decref(results);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we add an AVU to a data object?
START_TEST(test_add_metadata_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path),
                     EXIST_ST);

    baton_error_t error;
    init_baton_error(&error);
    int rv = modify_metadata(conn, &rods_path, META_ADD, "test_attr",
                             "test_value", "test_units", &error);
    ck_assert_int_eq(rv, 0);

    json_t *results = list_metadata(conn, &rods_path, "test_attr", &error);
    json_t *expected = json_pack("[{s:s, s:s, s:s}]",
                                 JSON_ATTRIBUTE_KEY, "test_attr",
                                 JSON_VALUE_KEY,     "test_value",
                                 JSON_UNITS_KEY,     "test_units");

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we remove an AVU from a data object?
START_TEST(test_remove_metadata_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path),
                     EXIST_ST);

    baton_error_t error;
    init_baton_error(&error);
    int rv = modify_metadata(conn, &rods_path, META_REM, "attr1",
                             "value1", "units1", &error);
    ck_assert_int_eq(rv, 0);

    json_t *results = list_metadata(conn, &rods_path, NULL, &error);
    json_t *expected = json_array(); // Empty

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we add a JSON AVU to a data object?
START_TEST(test_add_json_metadata_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path),
                     EXIST_ST);

    json_t *avu = json_pack("{s:s, s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "test_attr",
                            JSON_VALUE_KEY,     "test_value",
                            JSON_UNITS_KEY,     "test_units");
    baton_error_t error;
    int rv = modify_json_metadata(conn, &rods_path, META_ADD, avu, &error);
    ck_assert_int_eq(rv, 0);

    json_t *results = list_metadata(conn, &rods_path, "test_attr", &error);
    json_t *expected = json_array();
    json_array_append_new(expected, avu);

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we remove a JSON AVU from a data object?
START_TEST(test_remove_json_metadata_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path),
                     EXIST_ST);

    json_t *avu = json_pack("{s:s, s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1",
                            JSON_UNITS_KEY,     "units1");
    baton_error_t error;
    int rv = modify_json_metadata(conn, &rods_path, META_REM, avu, &error);
    ck_assert_int_eq(rv, 0);

    json_t *results = list_metadata(conn, &rods_path, NULL, &error);
    json_t *expected = json_array(); // Empty

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we change permissions on a data object?
START_TEST(test_modify_permissions_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path),
                     EXIST_ST);

    baton_error_t error;
    init_baton_error(&error);
    int rv = modify_permissions(conn, &rods_path, NO_RECURSE, "public",
                                ACCESS_READ, &error);
    ck_assert_int_eq(rv, 0);

    // TODO: check the new permissions are in place (there is no high
    // level iRODS API for this).

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we change JSON permissions on a data object?
START_TEST(test_modify_json_permissions_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path),
                     EXIST_ST);

    json_t *perm = json_pack("{s:s, s:s}",
                             JSON_OWNER_KEY, "public",
                             JSON_LEVEL_KEY,  ACCESS_READ);

    baton_error_t error;
    int rv = modify_json_permissions(conn, &rods_path, NO_RECURSE, perm,
                                     &error);
    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(error.code, 0);

    // TODO: check the new permissions are in place (there is no high
    // level iRODS API for this).

    json_decref(perm);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we convert an iRODS object to a JSON representation?
START_TEST(test_rods_path_to_json_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char obj_path[MAX_PATH_LEN];
    snprintf(obj_path, MAX_PATH_LEN, "%s/f1.txt", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, obj_path),
                     EXIST_ST);

    json_t *path = data_object_path_to_json(rods_path.outPath);
    json_t *avu = json_pack("{s:s, s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr1",
                            JSON_VALUE_KEY,     "value1",
                            JSON_UNITS_KEY,     "units1");
    json_t *expected = json_pack("{s:[o]}", JSON_AVUS_KEY, avu);
    json_object_update(expected, path);

    json_t *obj = rods_path_to_json(conn, &rods_path);
    ck_assert_int_eq(json_equal(obj, expected), 1);

    json_decref(obj);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we convert an iRODS collection to a JSON representation?
START_TEST(test_rods_path_to_json_coll) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);
    char coll_path[MAX_PATH_LEN];
    snprintf(coll_path, MAX_PATH_LEN, "%s/a", rods_root);

    rodsPath_t rods_path;
    ck_assert_int_eq(resolve_rods_path(conn, &env, &rods_path, coll_path),
                     EXIST_ST);

    json_t *path = collection_path_to_json(rods_path.outPath);
    json_t *avu = json_pack("{s:s, s:s, s:s}",
                            JSON_ATTRIBUTE_KEY, "attr2",
                            JSON_VALUE_KEY,     "value2",
                            JSON_UNITS_KEY,     "units2");
    json_t *expected = json_pack("{s:[o]}", JSON_AVUS_KEY, avu);
    json_object_update(expected, path);

    json_t *coll = rods_path_to_json(conn, &rods_path);
    ck_assert_int_eq(json_equal(coll, expected), 1);

    json_decref(coll);
    json_decref(expected);

    if (conn) rcDisconnect(conn);
}
END_TEST

// Can we convert JSON representation to a useful path string?
START_TEST(test_json_to_path) {
    const char *coll_path = "/a/b/c";
    json_t *coll = json_pack("{s:s}", JSON_COLLECTION_KEY, coll_path);
    baton_error_t error_col;
    ck_assert_str_eq(json_to_path(coll, &error_col), coll_path);
    ck_assert_int_eq(error_col.code, 0);
    json_decref(coll);

    const char *obj_path = "/a/b/c.txt";
    json_t *obj = json_pack("{s:s, s:s}",
                            JSON_COLLECTION_KEY,  "/a/b",
                            JSON_DATA_OBJECT_KEY, "c.txt");
    baton_error_t error_obj;
    ck_assert_str_eq(json_to_path(obj, &error_obj), obj_path);
    ck_assert_int_eq(error_obj.code, 0);
    json_decref(obj);

    // No collection key:value
    json_t *malformed_obj = json_pack("{s:s}", JSON_DATA_OBJECT_KEY, "c.txt");

    baton_error_t error;
    ck_assert_ptr_eq(json_to_path(malformed_obj, &error), NULL);
    ck_assert_int_ne(error.code, 0);
    json_decref(malformed_obj);
}
END_TEST

Suite *baton_suite(void) {
    Suite *suite = suite_create("baton");

    TCase *utilities_tests = tcase_create("utilities");
    tcase_add_test(utilities_tests, test_str_equals);
    tcase_add_test(utilities_tests, test_str_equals_ignore_case);
    tcase_add_test(utilities_tests, test_str_starts_with);
    tcase_add_test(utilities_tests, test_str_ends_with);

    TCase *basic_tests = tcase_create("basic");
    tcase_add_unchecked_fixture(basic_tests, setup, teardown);
    tcase_add_checked_fixture (basic_tests, basic_setup, basic_teardown);

    tcase_add_test(basic_tests, test_rods_login);
    tcase_add_test(basic_tests, test_is_irods_available);
    tcase_add_test(basic_tests, test_init_rods_path);
    tcase_add_test(basic_tests, test_resolve_rods_path);
    tcase_add_test(basic_tests, test_make_query_input);

    tcase_add_test(basic_tests, test_list_obj);
    tcase_add_test(basic_tests, test_list_coll);

    tcase_add_test(basic_tests, test_list_permissions_obj);
    tcase_add_test(basic_tests, test_list_permissions_coll);

    tcase_add_test(basic_tests, test_list_metadata_obj);
    tcase_add_test(basic_tests, test_list_metadata_coll);

    tcase_add_test(basic_tests, test_search_metadata_obj);
    tcase_add_test(basic_tests, test_search_metadata_coll);
    tcase_add_test(basic_tests, test_search_metadata_path_obj);

    tcase_add_test(basic_tests, test_add_metadata_obj);
    tcase_add_test(basic_tests, test_remove_metadata_obj);
    tcase_add_test(basic_tests, test_add_json_metadata_obj);
    tcase_add_test(basic_tests, test_remove_json_metadata_obj);

    tcase_add_test(basic_tests, test_modify_permissions_obj);
    tcase_add_test(basic_tests, test_modify_json_permissions_obj);

    tcase_add_test(basic_tests, test_rods_path_to_json_obj);
    tcase_add_test(basic_tests, test_rods_path_to_json_coll);

    tcase_add_test(basic_tests, test_json_to_path);

    suite_add_tcase(suite, utilities_tests);
    suite_add_tcase(suite, basic_tests);

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
