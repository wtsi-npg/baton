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
 * @file query.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _BATON_JSON_QUERY_H
#define _BATON_JSON_QUERY_H

#include <jansson.h>

#include "config.h"
#include "error.h"
#include "irods_api.h"
#include "query.h"
#include "utilities.h"

/**
 * Log the current JSON error state through the underlying logging
 * mechanism.
 *
 * @param[in] level     The logging level.
 * @param[in] error     The JSON error state.
 */
void log_json_error(log_level level, json_error_t *error);

/**
 * Return a valid query operator, or return NULL and set error.
 *
 * @param[in]  operator  The candidate operator
 * @param[in,out] error  An error report struct.
 *
 * @return A query operator guaranteed to be valid.
 */
const char *ensure_valid_operator(const char *operator, baton_error_t *error);

/**
 * Execute a general query and obtain results as a JSON array of objects.
 * Columns in the query are mapped to JSON object properties specified
 * by the labels argument.
 *
 * @param[in]  conn          An open iRODS connection.
 * @param[in]  zone          The zone in which to search.
 * @param[in]  query         The search query formulated as JSON.
 * @param[in]  format        Query format parameters indicating which columns
 *                           to return.
 * @param[in]  prepare_avu   Callback to add any AVU-fetching clauses to the
 *                           query.
 * @param[in]  prepare_acl   Callback to add any ACL-fetching clauses to the
 *                           query.
 * @param[in]  prepare_cre   Callback to add any creation timestamp clauses
 *                           to the query.
 * @param[in]  prepare_m_cre Callback to add any metadata creation timestamp 
 *                           clauses to the query.
 * @param[in]  prepare_mod   Callback to add any modification timestamp clauses
 *                           to the query.
 * @param[in]  prepare_m_mod Callback to add any metadata modification timestamp
 *                           clauses to the query.
 * @param[in,out] error      An error report struct.
 *
 * @return A newly constructed JSON array of objects, one per result row. The
 * caller must free this after use.
 */
json_t *do_search(rcComm_t *conn, char *zone_name, json_t *query,
                  query_format_in_t *format,
                  prepare_avu_search_cb prepare_avu,
                  prepare_acl_search_cb prepare_acl,
                  prepare_tps_search_cb prepare_cre,
                  prepare_tps_search_cb prepare_m_cre,
                  prepare_tps_search_cb prepare_mod,
                  prepare_tps_search_cb prepare_m_mod,
                  baton_error_t *error);

/**
 * Execute a general query and obtain results as a JSON array of objects.
 * Columns in the query are mapped to JSON object properties specified
 * by the labels argument.
 *
 * @param[in]  conn          An open iRODS connection.
 * @param[in]  query_in      A populated query input.
 * @param[in]  labels        An array of as many labels as there were columns
 *                           selected in the query.
 * @param[in,out] error      An error report struct.
 *
 * @return A newly constructed JSON array of objects, one per result row. The
 * caller must free this after use.
 */
json_t *do_query(rcComm_t *conn, genQueryInp_t *query_in,
                 const char *labels[], baton_error_t *error);

/**
 * Construct a JSON array of objects from a query output. Columns in the
 * query are mapped to JSON object properties specified by the labels
 * argument.
 *
 * @param[in] query_out     A populated query output.
 * @param[in] labels        An array of as many labels as there were columns
 *                          selected in the query.
 *
 * @return A newly constructed JSON array of objects, one per result row. The
 * caller must free this after use.
 */
json_t *make_json_objects(genQueryOut_t *query_out, const char *labels[]);

/**
 * Build a query to search by AVU.
 *
 * @param[out]  query_in     A query input.
 * @param[in]   avus         A JSON representation of AVUs.
 * @param[in]   prepare      Callback to add any AVU-searching clauses to the
 *                           query.
 * @param[in,out] error      An error report struct.
 *
 * @return A modified query input with AVU-searching clauses added.
 */
genQueryInp_t *prepare_json_avu_search(genQueryInp_t *query_in,
                                       json_t *avus,
                                       prepare_avu_search_cb prepare,
                                       baton_error_t *error);
/**
 * Build a query to search by ACL.
 *
 * @param[out]  query_in     A query input.
 * @param[in]   avus         A JSON representation of ACLs. These must be
 *                           a JSON array of permission objects.
 * @param[in]   prepare      Callback to add any ACL-searching clauses to the
 *                           query.
 * @param[in,out] error      An error report struct.
 *
 * @return A modified query input with ACL-searching clauses added.
 */
genQueryInp_t *prepare_json_acl_search(genQueryInp_t *query_in,
                                       json_t *acl,
                                       prepare_acl_search_cb prepare,
                                       baton_error_t *error);

/**
 * Build a query to search by timestamp(s).
 *
 * @param[out]  query_in     A query input.
 * @param[in]   timestamps   A JSON representation of timestamps. These must
                             be a JSON array of timestamp objects.
 * @param[in]  prepare_cre   Callback to add any creation timestamp clauses
 *                           to the query.
 * @param[in]  prepare_m_cre Callback to add any metadata creation timestamp
 *                           clauses to the query.
 * @param[in]  prepare_mod   Callback to add any modification timestamp clauses
 *                           to the query.
 * @param[in]  prepare_m_mod Callback to add any metadata modification timestamp
 *                           clauses to the query.
 * @param[in,out] error      An error report struct.
 *
 * @return A modified query input with timestamp-searching clauses added.
 */
genQueryInp_t *prepare_json_tps_search(genQueryInp_t *query_in,
                                       json_t *timestamp,
                                       prepare_tps_search_cb prepare_cre,
                                       prepare_tps_search_cb prepare_m_cre,
                                       prepare_tps_search_cb prepare_mod,
                                       prepare_tps_search_cb prepare_m_mod,
                                       baton_error_t *error);

json_t *add_acl_json_array(rcComm_t *conn, json_t *target,
                           baton_error_t *error);

json_t *add_acl_json_object(rcComm_t *conn, json_t *target,
                            baton_error_t *error);

json_t *add_avus_json_array(rcComm_t *conn, json_t *target,
                            baton_error_t *error);

json_t *add_avus_json_object(rcComm_t *conn, json_t *target,
                             baton_error_t *error);

json_t *add_repl_json_array(rcComm_t *conn, json_t *array,
                           baton_error_t *error);

json_t *add_repl_json_object(rcComm_t *conn, json_t *object,
                             baton_error_t *error);

json_t *add_tps_json_array(rcComm_t *conn, json_t *array,
                           baton_error_t *error);

json_t *add_tps_json_object(rcComm_t *conn, json_t *target,
                            baton_error_t *error);

json_t *add_checksum_json_array(rcComm_t *conn, json_t *array,
                                baton_error_t *error);

json_t *add_checksum_json_object(rcComm_t *conn, json_t *object,
                                 baton_error_t *error);

json_t *map_access_args(json_t *access, baton_error_t *error);

json_t *revmap_access_result(json_t *access, baton_error_t *error);

json_t *revmap_replicate_results(json_t *results, baton_error_t *error);

#endif  // _BATON_JSON_QUERY_H
