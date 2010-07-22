/*
Copyright 2010 SourceGear, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/**
 *
 * @file sg_dbndx_prototypes.h
 *
 */

#ifndef H_SG_DBNDX_H
#define H_SG_DBNDX_H

BEGIN_EXTERN_C

/**
 * Create a new dbndx object.  If the sql file doesn't exist yet,
 * it will be created.
 *
 */
void SG_dbndx__open(
	SG_context*,

	SG_repo* pRepo,                         /**< You still own this, but don't
	                                          free it until the dbndx is
	                                          freed.  It will retain this
	                                          pointer. */

	SG_uint32 iDagNum,                      /**< Which dag in that repo are we
	                                          indexing? */

	SG_pathname* pPath,

	SG_bool bQueryOnly,

	SG_dbndx** ppNew
	);

/**
 * Release the memory for an SG_dbndx object.  This does
 * nothing at all to the dbndx on disk.
 *
 */
void SG_dbndx__free(SG_context* pCtx, SG_dbndx* pdbc);

void SG_dbndx__lookup_audits(
        SG_context* pCtx, 
        SG_dbndx* pdbc, 
        const char* psz_csid,
        SG_varray** ppva
        );

void SG_dbndx__query_record_history(
        SG_context* pCtx,
        SG_dbndx* pdbc,
        const char* psz_recid,
        SG_varray** ppva
        );

/**
 * Query the dbrecords and return the IDs of all records that match a given criteria.
 *
 */
void SG_dbndx__query_list(
	SG_context*,
	SG_dbndx* pdbc,

	const char* pidState,          /**< The ID of the changeset representing
	                                 the state of the db on which you want to
	                                 filter.  If you pass NULL here, the query
	                                 will return all records that match the
	                                 criteria, without filtering on any db
	                                 state.  If you pass a changeset id, the
	                                 query will filter the results for all
	                                 records that are a member of that state.
	                                 */

	const SG_varray* pcrit,     /**< The criteria for the query.
									 * If you pass NULL here, the
									 * query will return all
									 * records. */
    SG_stringarray* psa_fields_slice,
	SG_stringarray** ppResult,
    SG_varray** ppva_sliced
	);

/**
 * Query the dbrecords and return the IDs of all records that match a given criteria.
 *
 */
void SG_dbndx__query_list__sorted(
	SG_context*,
	SG_dbndx* pdbc,

	const char* pidState,          /**< The ID of the changeset representing
                                     the state of the db on which you want to
                                     filter.  If you pass NULL here, the query
                                     will return all records that match the
                                     criteria, without filtering on any db
                                     state.  If you pass a changeset id, the
                                     query will filter the results for all
                                     records that are a member of that state.
                                     */

	const SG_varray* pcrit,     /**< The criteria for the query.
									 * If you pass NULL here, the
									 * query will return all
									 * records. */

	const SG_varray* pSort,         /**< The sort for the query. */

	SG_int32 iNumRecordsToReturn,   /**< The number of results to
									 * return.  If you pass 0 here,
									 * the number of results will not
									 * be constrained. */

	SG_int32 iNumRecordsToSkip,     /**< This is useful when sorting
									 * and returning only a portion of
									 * the results.  Use this to skip
									 * the first N results. */

    SG_stringarray* psa_fields_slice,
	SG_stringarray** ppResult,
    SG_varray** ppva_sliced
	);

void SG_dbndx__query(
	SG_context*,
	SG_dbndx** ppndx,

	const char* pidState,          /**< The ID of the changeset representing
                                     the state of the db on which you want to
                                     filter.  If you pass NULL here, the query
                                     will return all records that match the
                                     criteria, without filtering on any db
                                     state.  If you pass a changeset id, the
                                     query will filter the results for all
                                     records that are a member of that state.
                                     */

	const SG_varray* pcrit,     /**< The criteria for the query.
									 * If you pass NULL here, the
									 * query will return all
									 * records. */

	const SG_varray* pSort,         /**< The sort for the query. */

    SG_stringarray* psa_fields_slice,
    SG_dbndx_qresult** ppqr
	);

void SG_dbndx_qresult__count(
	SG_context* pCtx,
    SG_dbndx_qresult* pqr,
    SG_uint32* piCount
	);

void SG_dbndx_qresult__get__multiple(
	SG_context* pCtx,
    SG_dbndx_qresult* pqr,
    SG_int32 count_requested,
    SG_uint32* pi_count_retrieved,
    SG_varray* pva
	);

void SG_dbndx_qresult__get__one(
	SG_context* pCtx,
    SG_dbndx_qresult* pqr,
    SG_vhash** ppvh
	);

void SG_dbndx_qresult__done(
	SG_context* pCtx,
    SG_dbndx_qresult** pqr
	);

void SG_dbndx_qresult__free(SG_context* pCtx, SG_dbndx_qresult* pqr);

/**
 * This is used to return the number of records that match a given
 * criteria in each db state.  It useful for implementing things like
 * a graph of the number of open bugs over time.
 *
 */
void SG_dbndx__query_across_states(
	SG_context*,
	SG_dbndx* pdbc,
	const SG_varray* pcrit,
	SG_int32 gMin,
	SG_int32 gMax,
	SG_vhash** ppResults           /**< The results are returned in a
									* vhash, one entry for each state.
									* The key is the changeset ID.
									* The value for the key is another
									* vhash containing two elements,
									* timestamp and count, both of
                                    * which are integers.  TODO consider
                                    * putting the csid in here as well. */
	);

/**
 * Tell the dbndx to update itself with new dagnodes.
 *
 * Typical usage here is to call
 * this routine immediately after adding a changeset to the REPO.
 *
 */
void SG_dbndx__update__multiple(
	SG_context*,
	SG_dbndx* pdbc,
    SG_stringarray* psa
	);

void SG_dbndx__inject_audits(
	SG_context*,
	SG_dbndx* pdbc,
    SG_varray* pva_audits
	);

// this function is used by the sqlite and fs2 repos to do
// ndx updates.  it needs callbacks to fetch the actual
// indexes.

typedef void (SG_get_dbndx_callback)(SG_context* pCtx, void * pVoidData, SG_uint32 iDagNum, SG_dbndx** ppndx);
typedef void (SG_get_treendx_callback)(SG_context* pCtx, void * pVoidData, SG_uint32 iDagNum, SG_treendx** ppndx);

void sg_repo__update_ndx(
    SG_context * pCtx,
    SG_repo* pRepo,
    SG_rbtree* prb_new_dagnodes,
    SG_get_dbndx_callback* pfn_get_dbndx,
    void* p_arg_get_dbndx,
    SG_get_treendx_callback* pfn_get_treendx,
    void* p_arg_get_treendx
    );

void SG_dbndx__harvest_audits_for_injection(SG_context* pCtx, SG_repo* pRepo, const char* psz_csid, SG_varray* pva_audits);

END_EXTERN_C

#endif
