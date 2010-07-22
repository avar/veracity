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
 * @file sg_dbrecord_prototypes.h
 *
 */

#ifndef H_SG_DBRECORD_PROTOTYPES_H
#define H_SG_DBRECORD_PROTOTYPES_H

/*
  TODO weird idea:  maybe we should store each field value as its own
  blob and then serialize a JSON which contains just the HIDs of each
  field value.  The repo will end up more compact because multiple versions
  of a db record will contain only one copy of unchanged fields.
*/

BEGIN_EXTERN_C

/**
 * Allocate a dbrecord object.
 */
void SG_dbrecord__alloc(SG_context*, SG_dbrecord** ppResult);

#if defined(DEBUG)
#define SG_DBRECORD__ALLOC(pCtx,ppNew)	SG_STATEMENT(	SG_dbrecord * _precNew = NULL;											\
														SG_dbrecord__alloc(pCtx,&_precNew);									\
														_sg_mem__set_caller_data(_precNew,__FILE__,__LINE__,"SG_dbrecord");	\
														*(ppNew) = _precNew;													)
#else
#define SG_DBRECORD__ALLOC(pCtx,ppNew)	SG_dbrecord__alloc(pCtx,ppNew)
#endif

void SG_dbrecord__freeze(SG_context*, SG_dbrecord* prec, SG_repo * pRepo, const char** ppsz_hid_rec);

void SG_dbrecord__get_hid__ref(SG_context*, const SG_dbrecord* prec, const char** pResult);

void SG_dbrecord__save_to_repo(SG_context*,
	SG_dbrecord * prec,
	SG_repo * pRepo,
	SG_repo_tx_handle* pRepoTx,
	SG_uint64* iBlobFullLength
	);

void SG_dbrecord__load_from_repo(SG_context*,
	SG_repo * pRepo,
	const char* pszidHidBlob,
	SG_dbrecord ** ppResult
	);

/**
 * Write a vhash table in JSON format.  This function handles all the details of
 * setting up the SG_jsonwriter and places the output stream in the given string.
 */
void SG_dbrecord__to_json(SG_context*,
	SG_dbrecord* prec,
	SG_string* pStr
	);

/**
 *	We allocate the vhash pointer; caller owns thereafter
 */
void SG_dbrecord__to_vhash(
	SG_context* pCtx,
	SG_dbrecord* prec,
	SG_vhash** ppVh);


void SG_dbrecord__free(SG_context * pCtx, SG_dbrecord* prec);

void SG_dbrecord__count_pairs(SG_context*,
	const SG_dbrecord* prec,
	SG_uint32* piResult
	);

void SG_dbrecord__add_pair(SG_context*,
	SG_dbrecord* prec,
	const char* putf8Name,
	const char* putf8Value
	);

void SG_dbrecord__add_pair__int64(SG_context*,
	SG_dbrecord* prec,
	const char* putf8Name,
	SG_int64 intValue
	);

void SG_dbrecord__get_nth_pair(SG_context*,
	const SG_dbrecord* prec,
	SG_uint32 ndx,
	const char** pputf8Name,
	const char** pputf8Value
	);

void SG_dbrecord__get_value(SG_context*,
	const SG_dbrecord* prec,
	const char* putf8Name,
	const char** pputf8Value
	);

void SG_dbrecord__check_value(SG_context* pCtx,
    const SG_dbrecord* prec,
    const char* putf8Name,
    SG_bool* pb,
    const char** pputf8Value
    );

void SG_dbrecord__get_value__int64(SG_context*,
	const SG_dbrecord* prec,
	const char* putf8Name,
	SG_int64* piValue
	);

void SG_dbrecord__has_value(SG_context*,
	const SG_dbrecord* prec,
	const char* putf8Name,
	SG_bool* pb
	);

END_EXTERN_C

#endif
