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
 * @file sg_repo__private.h
 *
 * @details Private declarations used by the various sg_repo*.c files.
 * Everything in this file should be considered STATIC (hidden).
 * 
 */

// TODO get rid of treendx_prototypes.h and dbndx (same).  move to
// private header files.

// TODO add error codes for change_blob_encoding:
// one for "we were told this blob should stay full"
// one for we tried to compress and it wasn't worth it

// TODO add error code for db/tree ndx not supported by this repo instance
//
// TODO add parameter to create to leave ndx off
//
// TODO comments explaining that only ndx methods are allowed to look inside blobs

//////////////////////////////////////////////////////////////////

#ifndef H_SG_REPO__PRIVATE_H
#define H_SG_REPO__PRIVATE_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////
// Opaque REPO VTABLE instance data.  (May contain DB handle and
// anything else the VTABLE implementation needs.)  This is stored
// in the SG_repo structure after binding.

typedef struct _sg_repo__vtable__instance_data sg_repo__vtable__instance_data;

//////////////////////////////////////////////////////////////////
// Function prototypes for the routines in the REPO VTABLE.
//
// TODO The documentation describing the REPO API VTABLE routines
// TODO should be moved and placed on the public SG_repo_ routines.
// TODO That is, FN__sg_repo__open_repo_instance() is the VTABLE
// TODO prototype, but users call SG_repo__open_repo_instance()
// TODO and we privately handle the vtable-switch.  So in theory,
// TODO this header should be private and not seen by sglib callers.

/**
 * Use this to open an existing repo instance.  The
 * repo's descriptor must already be set (which should
 * be obvious, since the descriptor was used to select
 * the vtable).
 */
typedef void FN__sg_repo__open_repo_instance(
		SG_context* pCtx, 
        SG_repo * pRepo
        );

/**
 * Use this to create a new repo instance. 
 */
typedef void FN__sg_repo__create_repo_instance(
		SG_context* pCtx,
        SG_repo * pRepo, 
        SG_bool b_indexes,
        const char* psz_hash_method,	/**< TODO is this optional and if so
										 * what is the default value and/or
										 * is it repo-specific? */
        const char* psz_repo_id,    /**< Required.  GID.  If you want to create
                                      an instance of an entirely new repo,
                                      create a new GID and pass it in.
                                      Otherwise, you are creating a clone, an
                                      instance of an existing repo, so just get
                                      the ID from that repo and pass that in
                                      here. */

        const char* psz_admin_id   /**< Required.  This is the admin GID of the repo. */
        );

/**
 * Use this to remove a repo instance from disk.
 */
typedef void FN__sg_repo__delete_repo_instance(
    SG_context * pCtx,
    SG_repo * pRepo
    );

typedef void FN__sg_repo__check_integrity(
    SG_context* pCtx,
    SG_repo * pRepo, 
    SG_uint16 cmd,
    SG_uint32 iDagNum, 
    SG_bool* p_ok,
    SG_vhash** pp_vhash
    );
             
/**
 * This method is used to ask questions about the capabilities
 * and limitations of a particular implementation of the repo
 * API.  The intent is that the implementation will respond
 * with answers which are hard-coded.  This is not to be used
 * for fetching data from the repo or about the repo's data.
 *
 * Note that it DOES NOT take a pRepo so it CANNOT answer
 * questions about a specific REPO on disk; only questions
 * specific to the implementation.
 */
typedef void FN__sg_repo__query_implementation(
    SG_context* pCtx,
    SG_uint16 question,
    SG_bool* p_bool,
    SG_int64* p_int,
    char* p_buf_string,
    SG_uint32 len_buf_string,
    SG_vhash** pp_vhash
    );
             
/**
 * This method is used to retrieve information
 * about a blob without actually fetching that
 * blob.
 */
typedef void FN__sg_repo__fetch_blob__info(
    SG_context* pCtx,
	SG_repo * pRepo,
    const char* psz_hid_blob,
    SG_bool* pb_blob_exists,
    char** ppsz_objectid,
    SG_blob_encoding* pBlobFormat,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* pLenRawData,
    SG_uint64* pLenFull
    );
             
/**
 * This method removes all blobs that exist in the repo
 * from prbBlobHids.
 */
typedef void FN__sg_repo__query_blob_existence(
	SG_context* pCtx,
	SG_repo * pRepo,
	SG_stringarray* psaQueryBlobHids,
	SG_stringarray** ppsaNonexistentBlobs
	);

/**
 * This method will remove a blob from a repo instance.
 * It does nothing to prevent that blob from getting
 * put back later.
 */
typedef void FN__sg_repo__obliterate_blob(
    SG_context* pCtx,
	SG_repo * pRepo,
    const char* psz_hid_blob
    );
             
/**
 * This method will be called when the SG_repo object is freed.  It is your
 * opportunity to clean up anything.
 */
typedef void FN__sg_repo__close_repo_instance(
		SG_context* pCtx,
		SG_repo * pRepo
        );

typedef void FN__sg_repo__begin_tx(SG_context* pCtx,
								   SG_repo* pRepo,
								   SG_repo_tx_handle** ppTx);

typedef void FN__sg_repo__commit_tx(SG_context* pCtx,
									SG_repo* pRepo,
									SG_repo_tx_handle** ppTx);

typedef void FN__sg_repo__abort_tx(SG_context* pCtx,
								   SG_repo* pRepo,
								   SG_repo_tx_handle** ppTx);

typedef void FN__sg_repo__hash__begin(
	SG_context* pCtx,
    SG_repo * pRepo,
    SG_repo_hash_handle** ppHandle
    );

typedef void FN__sg_repo__hash__chunk(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle* pHandle,
    SG_uint32 len_chunk,
    const SG_byte* p_chunk
    );

typedef void FN__sg_repo__hash__end(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle** pHandle,
    char** ppsz_hid_returned
    );

typedef void FN__sg_repo__hash__abort(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle** pHandle
    );

typedef void FN__sg_repo__store_blob__begin(
	SG_context* pCtx,
    SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    const char* psz_objectid,
    SG_blob_encoding blob_encoding,
    const char* psz_hid_vcdiff_reference,
    SG_uint64 lenFull,
	SG_uint64 lenEncoded,
    const char* psz_hid_known,
    SG_repo_store_blob_handle** ppHandle
    );

typedef void FN__sg_repo__store_blob__chunk(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_store_blob_handle* pHandle,
    SG_uint32 len_chunk,
    const SG_byte* p_chunk,
    SG_uint32* piWritten
    );

typedef void FN__sg_repo__store_blob__end(
    SG_context* pCtx,
	SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    SG_repo_store_blob_handle** pHandle,
    char** ppsz_hid_returned
    );

typedef void FN__sg_repo__store_blob__abort(
    SG_context* pCtx,
	SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    SG_repo_store_blob_handle** pHandle
    );

typedef void FN__sg_repo__fetch_blob__begin(
    SG_context* pCtx,
	SG_repo * pRepo,
    const char* psz_hid_blob,
    SG_bool b_convert_to_full,
    char** ppsz_objectid,
    SG_blob_encoding* pBlobFormat,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* pLenRawData,
    SG_uint64* pLenFull,
    SG_repo_fetch_blob_handle** ppHandle
    );
             
typedef void FN__sg_repo__fetch_blob__chunk(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_fetch_blob_handle* pHandle,
    SG_uint32 len_buf,
    SG_byte* p_buf,
    SG_uint32* p_len_got,
    SG_bool* pb_done
    );

typedef void FN__sg_repo__fetch_blob__end(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_fetch_blob_handle** ppHandle
    );

typedef void FN__sg_repo__fetch_blob__abort(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_fetch_blob_handle** ppHandle
    );

typedef void FN__sg_repo__fetch_repo__fragball(
	SG_context* pCtx,
	SG_repo* pRepo,
	const SG_pathname* pFragballDirPathname,
	char** ppszFragballName
	);

typedef void FN__sg_repo__vacuum(
    SG_context* pCtx,
	SG_repo * pRepo
    );

typedef void FN__sg_repo__change_blob_encoding(
    SG_context* pCtx,
	SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    const char* psz_hid_blob,
    SG_blob_encoding blob_encoding_desired,
    const char* psz_hid_vcdiff_reference_desired,
    SG_blob_encoding* p_blob_encoding_new,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* p_len_encoded,
    SG_uint64* p_len_full
);

typedef void FN__sg_repo__get_blob_stats(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_uint32* p_count_blobs_full,
    SG_uint32* p_count_blobs_alwaysfull,
    SG_uint32* p_count_blobs_zlib,
    SG_uint32* p_count_blobs_vcdiff,
    SG_uint64* p_total_blob_size_full,
    SG_uint64* p_total_blob_size_alwaysfull,
    SG_uint64* p_total_blob_size_zlib_full,
    SG_uint64* p_total_blob_size_zlib_encoded,
    SG_uint64* p_total_blob_size_vcdiff_full,
    SG_uint64* p_total_blob_size_vcdiff_encoded
    );

typedef void FN__sg_repo__fetch_dagnode(
        SG_context* pCtx,
		SG_repo * pRepo, 
        const char* pszidHidChangeset, 
        SG_dagnode ** ppNewDagnode
        );

/**
 * This function returns the current collection of leaves for a given DAG.
 *
 * If you are writing an implementation of repo storage, you should
 * plan to keep the leaf list precalculated.  Yes, this will
 * require a concurrency lock, but it's almost certainly worth
 * the trouble.  Calculating the leaf list on the fly will
 * probably cost a fair amount.
 */
typedef void FN__sg_repo__fetch_dag_leaves(
        SG_context* pCtx,
		SG_repo * pRepo, 
        SG_uint32 iDagNum, 
        SG_rbtree ** ppIdsetReturned    /**< Caller must free */
        );


/**
 * This function returns the current collection of children for a given dagnode
 * in a given DAG.
 *
 * You should not call this a lot.  Any dag walking should be done from the leaves-up,
 * since that is much more efficient than calling this function a lot.
 */
typedef void FN__sg_repo__fetch_dagnode_children(
        SG_context* pCtx,
		SG_repo * pRepo,
        SG_uint32 iDagNum,
        const char * pszDagnodeID,
        SG_rbtree ** ppIdsetReturned    /**< Caller must free */
        );

typedef void FN__sg_repo__check_dagfrag(
        SG_context* pCtx,
		SG_repo * pRepo,
        SG_dagfrag * pFrag,
		SG_bool * pbConnected,       /**< True if the provided dagfrag connects
									   to the repo's DAG, false if more nodes are 
									   needed. */
        SG_rbtree ** pprb_missing,   /**< If the insert would fail because the 
                                       DAG is disconnected (pbConnected is SG_FALSE), 
                                       this rbtree will contain a list of nodes
                                       which are missing.  You can use this as
                                       information to grow the fragment deeper
                                       for a retry.  Caller must free this. */

        SG_stringarray ** ppsaNew,   /**< If the insert would succeed, this contains
                                       a list of all the IDs of dagnodes which
                                       would be actually inserted.  This is
                                       important because the fragment may have
                                       contained nodes that were already
                                       present in the dag.  Use this list to
                                       constrain the effort required in further
                                       updates.  Typically, after a
                                       store_dagfrag when syncing repo
                                       instances, the next step is to load each
                                       changeset and sync the blobs.  This list
                                       tells you which changesets you need to
                                       actually bother with.  Caller must free
                                       this. */
		SG_rbtree ** pprbLeaves        /**< If the insert would succeed, this 
                                       contains a list of what the dags leaves 
									   would be.  Caller must free this. */
        );

/**
 * This function attempts to insert a dag fragment into
 * the dag.  If it fails, the error code will probably be
 * SG_ERR_CANNOT_CREATE_SPARSE_DAG, which indicates that
 * the fragment cannot be completely connected to nodes
 * already in the dag.  To avoid this, use check_dagfrag
 * BEFORE you begin_tx.  Inside the tx is not the place
 * to be fiddling with frags.
 */
typedef void FN__sg_repo__store_dagfrag(
        SG_context* pCtx,
		SG_repo * pRepo,
		SG_repo_tx_handle* pTx,
        SG_dagfrag * pFrag  /**< Caller no longer owns this */
        );

/**
 * This returns the ID of the repo.  This ID is the one which is shared by all
 * instances of that repo.  If you want to create a clone, a new instance of an
 * existing repo, then get this ID, and use it to create a new instance. */
typedef void FN__sg_repo__get_repo_id(
        SG_context* pCtx,
		SG_repo * pRepo, 
        char** ppsz_id  /**< Caller must free */
        );

typedef void FN__sg_repo__get_admin_id(
        SG_context* pCtx,
		SG_repo * pRepo, 
        char** ppsz_id  /**< Caller must free */
        );

/**
 * This function helps in situations where the user has typed
 * the beginning of an ID and you want to find all the IDs
 * that match.  There are two similar functions for this purpose.
 * This one matches only dagnodes in a given dagnum.  The other one, below,
 * will match any blob ID.
 */
typedef void FN__sg_repo__find_dagnodes_by_prefix(
        SG_context* pCtx,
		SG_repo * pRepo, 
        SG_uint32 iDagNum, 
        const char* psz_hid_prefix,
        SG_rbtree** pprb        /**< Caller must free */
        );

typedef void FN__sg_repo__find_blobs_by_prefix(
        SG_context* pCtx,
		SG_repo * pRepo, 
        const char* psz_hid_prefix, 
        SG_rbtree** pprb        /**< Caller must free */
        );

typedef void FN__sg_repo__list_blobs(
        SG_context* pCtx,
		SG_repo * pRepo, 
        SG_blob_encoding blob_encoding, /**< only return blobs of this encoding */
        SG_bool b_sort_descending,      /**< otherwise sort ascending */
        SG_bool b_sort_by_len_encoded,  /**< otherwise sort by len_full */
        SG_uint32 limit,                /**< only return this many entries */
        SG_uint32 offset,               /**< skip the first group of the sorted entries */
        SG_vhash** ppvh                 /**< Caller must free */
        );

/**
 * Retrieve the lowest common ancestor of a set of dag nodes.
 */
typedef void FN__sg_repo__get_dag_lca(
        SG_context* pCtx,
		SG_repo* pRepo, 
        SG_uint32 iDagNum,
        const SG_rbtree* prbNodes, 
        SG_daglca ** ppDagLca
        );

/**
 * This function will return a list of all the dagnums that
 * actually exist in this repo instance.
 */
typedef void FN__sg_repo__list_dags(
        SG_context* pCtx,
		SG_repo* pRepo, 
        SG_uint32* piCount, 
        SG_uint32** paDagNums       /**< Caller must free */
        );

typedef void FN__sg_repo__dbndx__lookup_audits(
	SG_context*,
    SG_repo* pRepo, 
    SG_uint32 iDagNum,
    const char* psz_csid,
    SG_varray** ppva
	);

typedef void FN__sg_repo__dbndx__query_record_history(
	SG_context*,
    SG_repo* pRepo, 
    SG_uint32 iDagNum,
    const char* psz_recid,  // this is NOT the hid of a record.  it is the zing recid
    SG_varray** ppva
	);

typedef void FN__sg_repo__dbndx__query(
	SG_context*,
    SG_repo* pRepo, 
    SG_uint32 iDagNum,

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
    SG_stringarray* psa_slice_fields,
    SG_repo_qresult** ppqr
	);

typedef void FN__sg_repo__qresult__count(
	SG_context*,
    SG_repo* pRepo, 
    SG_repo_qresult* pqr,
    SG_uint32* picount
    );

typedef void FN__sg_repo__qresult__get__multiple(
	SG_context*,
    SG_repo* pRepo, 
    SG_repo_qresult* pqr,
    SG_int32 count_wanted,
    SG_uint32* pi_count_retrieved,
    SG_varray* pva
    );

typedef void FN__sg_repo__qresult__get__one(
	SG_context*,
    SG_repo* pRepo, 
    SG_repo_qresult* pqr,
    SG_vhash** ppvh
    );

typedef void FN__sg_repo__qresult__done(
	SG_context*,
    SG_repo* pRepo, 
    SG_repo_qresult** ppqr
    );

/**
 * This is used to return the number of records that match a given
 * criteria in each db state.  It useful for implementing things like
 * a graph of the number of open bugs over time.
 *
 */
typedef void FN__sg_repo__dbndx__query_across_states(
	SG_context*,
    SG_repo* pRepo, 
    SG_uint32 iDagNum,
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


typedef void FN__sg_repo__treendx__get_path_in_dagnode(
        SG_context* pCtx, 
        SG_repo* pRepo, 
        SG_uint32 iDagNum,
        const char* psz_search_item_gid, 
        const char* psz_changeset, 
        SG_treenode_entry ** ppTreeNodeEntry
        );

typedef void FN__sg_repo__treendx__get_all_paths(
        SG_context* pCtx, 
        SG_repo* pRepo, 
        SG_uint32 iDagNum,
        const char* psz_gid, 
        SG_stringarray ** ppResults
        );

typedef void FN__sg_repo__get_hash_method(
        SG_context* pCtx, 
        SG_repo* pRepo, 
        char** ppsz_result  /* caller must free */
        );

//////////////////////////////////////////////////////////////////
// The REPO VTABLE

struct _sg_repo__vtable
{
	const char * const                                      pszStorage;

	FN__sg_repo__open_repo_instance		        * const		open_repo_instance;
	FN__sg_repo__create_repo_instance		    * const		create_repo_instance;
	FN__sg_repo__delete_repo_instance           * const     delete_repo_instance;
	FN__sg_repo__close_repo_instance		    * const		close_repo_instance;

	FN__sg_repo__begin_tx                       * const		begin_tx;
	FN__sg_repo__commit_tx                      * const		commit_tx;
	FN__sg_repo__abort_tx                       * const		abort_tx;

	FN__sg_repo__store_blob__begin              * const		store_blob__begin;
	FN__sg_repo__store_blob__chunk              * const		store_blob__chunk;
	FN__sg_repo__store_blob__end                * const		store_blob__end;
	FN__sg_repo__store_blob__abort              * const		store_blob__abort;

	FN__sg_repo__store_dagfrag		            * const		store_dagfrag;

	FN__sg_repo__fetch_blob__begin              * const		fetch_blob__begin;
	FN__sg_repo__fetch_blob__chunk              * const		fetch_blob__chunk;
	FN__sg_repo__fetch_blob__end                * const		fetch_blob__end;
	FN__sg_repo__fetch_blob__abort              * const		fetch_blob__abort;

	FN__sg_repo__fetch_repo__fragball           * const		fetch_repo__fragball;

	FN__sg_repo__check_dagfrag		            * const		check_dagfrag;

	FN__sg_repo__change_blob_encoding           * const		change_blob_encoding;

	FN__sg_repo__get_blob_stats                 * const		get_blob_stats;

	FN__sg_repo__fetch_dagnode		            * const		fetch_dagnode;

    FN__sg_repo__find_dagnodes_by_prefix        * const     find_dagnodes_by_prefix;
    FN__sg_repo__find_blobs_by_prefix           * const     find_blobs_by_prefix;
    FN__sg_repo__list_blobs                     * const     list_blobs;
    FN__sg_repo__vacuum                         * const     vacuum;

	FN__sg_repo__list_dags		                * const		list_dags;
	FN__sg_repo__fetch_dag_leaves		        * const		fetch_dag_leaves;
	FN__sg_repo__fetch_dagnode_children		    * const		fetch_dagnode_children;
    FN__sg_repo__get_dag_lca                    * const     get_dag_lca;
	FN__sg_repo__get_repo_id	                * const		get_repo_id;
	FN__sg_repo__get_admin_id	                * const		get_admin_id;

	FN__sg_repo__query_implementation           * const		query_implementation;
	FN__sg_repo__obliterate_blob                * const		obliterate_blob;
	FN__sg_repo__fetch_blob__info               * const		fetch_blob__info;
	FN__sg_repo__query_blob_existence			* const		query_blob_existence;
    
	FN__sg_repo__dbndx__query                   * const		dbndx__query;
	FN__sg_repo__dbndx__query_across_states     * const		dbndx__query_across_states;
	FN__sg_repo__dbndx__query_record_history    * const		dbndx__query_record_history;
	FN__sg_repo__dbndx__lookup_audits           * const		dbndx__lookup_audits;
    
	FN__sg_repo__qresult__count                 * const		qresult__count;
	FN__sg_repo__qresult__get__multiple         * const		qresult__get__multiple;
	FN__sg_repo__qresult__get__one              * const		qresult__get__one;
	FN__sg_repo__qresult__done                  * const		qresult__done;

	FN__sg_repo__treendx__get_path_in_dagnode   * const		treendx__get_path_in_dagnode;
	FN__sg_repo__treendx__get_all_paths         * const		treendx__get_all_paths;

	FN__sg_repo__get_hash_method                * const		get_hash_method;
	FN__sg_repo__hash__begin                    * const		hash__begin;
	FN__sg_repo__hash__chunk                    * const		hash__chunk;
	FN__sg_repo__hash__end                      * const		hash__end;
	FN__sg_repo__hash__abort                    * const		hash__abort;
	FN__sg_repo__check_integrity                * const		check_integrity;

};

typedef struct _sg_repo__vtable sg_repo__vtable;

//////////////////////////////////////////////////////////////////
// Convenience macro to declare a prototype for each function in the
// REPO VTABLE for a specific implementation.

#define DCL__REPO_VTABLE_PROTOTYPES(name)												            \
	FN__sg_repo__open_repo_instance			    sg_repo__##name##__open_repo_instance; 	        	\
	FN__sg_repo__create_repo_instance			sg_repo__##name##__create_repo_instance; 	        \
    FN__sg_repo__delete_repo_instance           sg_repo__##name##__delete_repo_instance;            \
	FN__sg_repo__close_repo_instance			sg_repo__##name##__close_repo_instance;		        \
	FN__sg_repo__begin_tx						sg_repo__##name##__begin_tx;                        \
	FN__sg_repo__commit_tx						sg_repo__##name##__commit_tx;                       \
	FN__sg_repo__abort_tx						sg_repo__##name##__abort_tx;                        \
	FN__sg_repo__store_blob__begin              sg_repo__##name##__store_blob__begin;               \
	FN__sg_repo__store_blob__chunk              sg_repo__##name##__store_blob__chunk;               \
	FN__sg_repo__store_blob__end                sg_repo__##name##__store_blob__end;                 \
	FN__sg_repo__store_blob__abort              sg_repo__##name##__store_blob__abort;               \
	FN__sg_repo__store_dagfrag			        sg_repo__##name##__store_dagfrag;		            \
	FN__sg_repo__fetch_blob__begin              sg_repo__##name##__fetch_blob__begin;               \
	FN__sg_repo__fetch_blob__chunk              sg_repo__##name##__fetch_blob__chunk;               \
	FN__sg_repo__fetch_blob__end                sg_repo__##name##__fetch_blob__end;                 \
	FN__sg_repo__fetch_blob__abort              sg_repo__##name##__fetch_blob__abort;               \
	FN__sg_repo__fetch_repo__fragball           sg_repo__##name##__fetch_repo__fragball;            \
	FN__sg_repo__check_dagfrag			        sg_repo__##name##__check_dagfrag;		            \
	FN__sg_repo__change_blob_encoding           sg_repo__##name##__change_blob_encoding;            \
	FN__sg_repo__get_blob_stats                 sg_repo__##name##__get_blob_stats;                  \
	FN__sg_repo__fetch_dagnode			        sg_repo__##name##__fetch_dagnode;		            \
	FN__sg_repo__find_dagnodes_by_prefix        sg_repo__##name##__find_dagnodes_by_prefix;	        \
	FN__sg_repo__find_blobs_by_prefix		    sg_repo__##name##__find_blobs_by_prefix;	        \
	FN__sg_repo__list_blobs                     sg_repo__##name##__list_blobs;	                    \
	FN__sg_repo__vacuum                         sg_repo__##name##__vacuum;	                        \
	FN__sg_repo__list_dags			            sg_repo__##name##__list_dags;		                \
	FN__sg_repo__fetch_dag_leaves			    sg_repo__##name##__fetch_dag_leaves;		        \
	FN__sg_repo__get_dag_lca			        sg_repo__##name##__get_dag_lca;	    	            \
	FN__sg_repo__get_repo_id	        	    sg_repo__##name##__get_repo_id;                     \
	FN__sg_repo__get_admin_id	   	            sg_repo__##name##__get_admin_id;                    \
	FN__sg_repo__query_implementation           sg_repo__##name##__query_implementation;            \
	FN__sg_repo__obliterate_blob                sg_repo__##name##__obliterate_blob;                 \
	FN__sg_repo__fetch_blob__info               sg_repo__##name##__fetch_blob__info;                \
	FN__sg_repo__query_blob_existence           sg_repo__##name##__query_blob_existence;            \
	FN__sg_repo__dbndx__query                   sg_repo__##name##__dbndx__query;                    \
	FN__sg_repo__dbndx__query_across_states     sg_repo__##name##__dbndx__query_across_states;      \
	FN__sg_repo__dbndx__query_record_history    sg_repo__##name##__dbndx__query_record_history;     \
	FN__sg_repo__dbndx__lookup_audits           sg_repo__##name##__dbndx__lookup_audits;     \
	FN__sg_repo__qresult__count                 sg_repo__##name##__qresult__count;                  \
	FN__sg_repo__qresult__get__multiple         sg_repo__##name##__qresult__get__multiple;                    \
	FN__sg_repo__qresult__get__one              sg_repo__##name##__qresult__get__one;                    \
	FN__sg_repo__qresult__done                  sg_repo__##name##__qresult__done;                   \
	FN__sg_repo__treendx__get_path_in_dagnode   sg_repo__##name##__treendx__get_path_in_dagnode;    \
	FN__sg_repo__treendx__get_all_paths         sg_repo__##name##__treendx__get_all_paths;          \
	FN__sg_repo__get_hash_method                sg_repo__##name##__get_hash_method;                 \
	FN__sg_repo__hash__begin                    sg_repo__##name##__hash__begin;                     \
	FN__sg_repo__hash__chunk                    sg_repo__##name##__hash__chunk;                     \
	FN__sg_repo__hash__end                      sg_repo__##name##__hash__end;                       \
	FN__sg_repo__hash__abort                    sg_repo__##name##__hash__abort;                     \
	FN__sg_repo__check_integrity                sg_repo__##name##__check_integrity;                 \
	FN__sg_repo__fetch_dagnode_children         sg_repo__##name##__fetch_dagnode_children;          \


// Convenience macro to declare a properly initialized static
// REPO VTABLE for a specific implementation.

#define DCL__REPO_VTABLE(storage,name)						\
	static sg_repo__vtable s_repo_vtable__##name =  		\
	{	storage,											\
		sg_repo__##name##__open_repo_instance,			    \
		sg_repo__##name##__create_repo_instance,			\
        sg_repo__##name##__delete_repo_instance,            \
		sg_repo__##name##__close_repo_instance,				\
		sg_repo__##name##__begin_tx,						\
		sg_repo__##name##__commit_tx,						\
		sg_repo__##name##__abort_tx,						\
		sg_repo__##name##__store_blob__begin,               \
		sg_repo__##name##__store_blob__chunk,               \
		sg_repo__##name##__store_blob__end,                 \
		sg_repo__##name##__store_blob__abort,               \
		sg_repo__##name##__store_dagfrag,					\
		sg_repo__##name##__fetch_blob__begin,               \
		sg_repo__##name##__fetch_blob__chunk,               \
		sg_repo__##name##__fetch_blob__end,                 \
		sg_repo__##name##__fetch_blob__abort,               \
		sg_repo__##name##__fetch_repo__fragball,            \
		sg_repo__##name##__check_dagfrag,					\
		sg_repo__##name##__change_blob_encoding,            \
		sg_repo__##name##__get_blob_stats,                  \
		sg_repo__##name##__fetch_dagnode,				    \
		sg_repo__##name##__find_dagnodes_by_prefix,	        \
		sg_repo__##name##__find_blobs_by_prefix,   	        \
		sg_repo__##name##__list_blobs,          	        \
		sg_repo__##name##__vacuum,              	        \
		sg_repo__##name##__list_dags,				        \
		sg_repo__##name##__fetch_dag_leaves,				\
		sg_repo__##name##__fetch_dagnode_children,			\
		sg_repo__##name##__get_dag_lca,	    				\
		sg_repo__##name##__get_repo_id,     		        \
		sg_repo__##name##__get_admin_id,   			        \
        sg_repo__##name##__query_implementation,            \
        sg_repo__##name##__obliterate_blob,                 \
        sg_repo__##name##__fetch_blob__info,                \
        sg_repo__##name##__query_blob_existence,            \
        sg_repo__##name##__dbndx__query,                    \
        sg_repo__##name##__dbndx__query_across_states,      \
        sg_repo__##name##__dbndx__query_record_history,     \
        sg_repo__##name##__dbndx__lookup_audits,            \
        sg_repo__##name##__qresult__count,                  \
        sg_repo__##name##__qresult__get__multiple,          \
        sg_repo__##name##__qresult__get__one,               \
        sg_repo__##name##__qresult__done,                   \
        sg_repo__##name##__treendx__get_path_in_dagnode,    \
        sg_repo__##name##__treendx__get_all_paths,          \
        sg_repo__##name##__get_hash_method,                 \
        sg_repo__##name##__hash__begin,                     \
        sg_repo__##name##__hash__chunk,                     \
        sg_repo__##name##__hash__end,                       \
        sg_repo__##name##__hash__abort,                     \
        sg_repo__##name##__check_integrity,                 \
	}

//////////////////////////////////////////////////////////////////

/**
 * Private routine to select a specific VTABLE implementation
 * and bind it to the given pRepo.
 *
 * Note that this only sets pRepo->p_vtable.  It DOES NOT imply
 * that pRepo is associated with anything on disk; that is,
 * we DO NOT set pRepo->p_vtable_instance_data. 
 */
void sg_repo__bind_vtable(SG_context* pCtx, SG_repo * pRepo, const char * pszStorage);

/**
 * Private routine to unbind the VTABLE from the given pRepo.
 * This routine will NULL the vtable pointer in the SG_repo and
 * free the vtable if appropriate.
 */
void sg_repo__unbind_vtable(SG_context* pCtx, SG_repo * pRepo);

/**
 * Private routine to list all of the installed REPO implementations.
 *
 * You own the returned vhash.
 */
void sg_repo__query_implementation__list_vtables(SG_context * pCtx, SG_vhash ** pp_vhash);

//////////////////////////////////////////////////////////////////

/**
 * Private REPO instance data.  This is hidden from everybody except
 * sg_repo__ routines.
 */
struct _SG_repo
{
	char*								psz_descriptor_name;
    SG_vhash*							pvh_descriptor;			// this is used to select a VTABLE implementation

	sg_repo__vtable *		            p_vtable;		        // the binding to a specific REPO VTABLE implementation
	sg_repo__vtable__instance_data *	p_vtable_instance_data;	// binding-specific instance data (opaque outside of imp)
};

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_REPO__PRIVATE_H

