/**
 * Copyright (c) 2013 Genome Research Ltd. All rights reserved.
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

#include <unistd.h>

#include <jansson.h>
#include <zlog.h>
#include <check.h>

#include "../src/baton.h"
#include "../src/json.h"

static int MAX_COMMAND_LEN = 1024;
static int MAX_PATH_LEN = 4096;

static char *BASIC_COLL = "baton-basic-test";
static char *BASIC_DATA_PATH = "./data/";
static char *BASIC_METADATA_PATH = "./metadata/meta1.imeta";

static char *SETUP_SCRIPT = "./scripts/setup_irods.sh";
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

    struct baton_error error;
    json_t *results = list_metadata(conn, &rods_path, NULL, &error);
    json_t *expected = json_pack("[{s:s, s:s, s:s}]",
                                 "attribute", "attr1",
                                 "value", "value1",
                                 "units", "units1");

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);
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

    struct baton_error error;
    json_t *results = list_metadata(conn, &rods_path, NULL, &error);
    json_t *expected = json_pack("[{s:s, s:s, s:s}]",
                                 "attribute", "attr2",
                                 "value", "value2",
                                 "units", "units2");

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);
}
END_TEST

// Can we search for data objects by their metadata?
START_TEST(test_search_metadata_obj) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    struct baton_error error;
    json_t *results = search_metadata(conn, "attr1", "value1", &error);
    ck_assert_int_eq(json_array_size(results), 12);

    const char *key1 = "collection";
    const char *key2 = "data_object";

    for (size_t i = 0; i < 12; i++) {
        json_t *obj = json_array_get(results, i);
        ck_assert_ptr_ne(json_object_get(obj, key1), NULL);
        ck_assert_ptr_ne(json_object_get(obj, key2), NULL);
    }
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
}
END_TEST

// Can we search for collections by their metadata?
START_TEST(test_search_metadata_coll) {
    rodsEnv env;
    rcComm_t *conn = rods_login(&env);

    char rods_root[MAX_PATH_LEN];
    set_current_rods_root(BASIC_COLL, rods_root);

    struct baton_error error;
    json_t *results = search_metadata(conn, "attr2", "value2", &error);
    ck_assert_int_eq(json_array_size(results), 3);
    for (size_t i = 0; i < 3; i++) {
        json_t *coll = json_array_get(results, i);
        ck_assert_ptr_ne(json_object_get(coll, "collection"), NULL);
        ck_assert_ptr_eq(json_object_get(coll, "data_object"), NULL);
    }
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
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

    struct baton_error error;
    int rv = modify_metadata(conn, &rods_path, META_ADD, "test_attr",
                             "test_value", "test_units", &error);
    ck_assert_int_eq(rv, 0);

    json_t *results = list_metadata(conn, &rods_path, "test_attr", &error);
    json_t *expected = json_pack("[{s:s, s:s, s:s}]",
                                 "attribute", "test_attr",
                                 "value", "test_value",
                                 "units", "test_units");

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);
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

    struct baton_error error;
    int rv = modify_metadata(conn, &rods_path, META_REM, "attr1",
                             "value1", "units1", &error);
    ck_assert_int_eq(rv, 0);

    json_t *results = list_metadata(conn, &rods_path, NULL, &error);
    json_t *expected = json_array(); // Empty

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);
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
                            "attribute", "test_attr",
                            "value", "test_value",
                            "units", "test_units");
    struct baton_error error;
    int rv = modify_json_metadata(conn, &rods_path, META_ADD, avu, &error);
    ck_assert_int_eq(rv, 0);

    json_t *results = list_metadata(conn, &rods_path, "test_attr", &error);
    json_t *expected = json_array();
    json_array_append_new(expected, avu);

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);
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
                            "attribute", "attr1",
                            "value", "value1",
                            "units", "units1");
    struct baton_error error;
    int rv = modify_json_metadata(conn, &rods_path, META_REM, avu, &error);
    ck_assert_int_eq(rv, 0);

    json_t *results = list_metadata(conn, &rods_path, NULL, &error);
    json_t *expected = json_array(); // Empty

    ck_assert_int_eq(json_equal(results, expected), 1);
    ck_assert_int_eq(error.code, 0);

    json_decref(results);
    json_decref(expected);
}
END_TEST


// json_t *rods_path_to_json(rcComm_t *conn, rodsPath_t *rods_path)

// json_t *data_object_path_to_json(const char *path)

// json_t *collection_path_to_json(const char *path)

// char *json_to_path(json_t *json)

Suite *baton_suite(void) {
    Suite *suite = suite_create("baton");

    TCase *basic_tests = tcase_create("basic");
    tcase_add_unchecked_fixture(basic_tests, setup, teardown);
    tcase_add_checked_fixture (basic_tests, basic_setup, basic_teardown);

    tcase_add_test(basic_tests, test_rods_login);
    tcase_add_test(basic_tests, test_is_irods_available);
    tcase_add_test(basic_tests, test_init_rods_path);
    tcase_add_test(basic_tests, test_resolve_rods_path);
    tcase_add_test(basic_tests, test_make_query_input);

    tcase_add_test(basic_tests, test_list_metadata_obj);
    tcase_add_test(basic_tests, test_list_metadata_coll);

    tcase_add_test(basic_tests, test_search_metadata_obj);
    tcase_add_test(basic_tests, test_search_metadata_coll);

    tcase_add_test(basic_tests, test_add_metadata_obj);
    tcase_add_test(basic_tests, test_remove_metadata_obj);
    tcase_add_test(basic_tests, test_add_json_metadata_obj);
    tcase_add_test(basic_tests, test_remove_json_metadata_obj);

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
