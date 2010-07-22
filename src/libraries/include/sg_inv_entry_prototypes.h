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
 * @file sg_inv_entry_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_INV_ENTRY_PROTOTYPES_H
#define H_SG_INV_ENTRY_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_inv_entry__free(SG_context * pCtx, SG_inv_entry * pInvEntry);

void SG_inv_entry__alloc(SG_context * pCtx, struct _inv_dir * pInvDir_Back, SG_inv_entry ** ppInvEntry);

#if defined(DEBUG)
#define SG_INV_ENTRY__ALLOC(pCtx,pInvDir_Back,ppThis)		SG_STATEMENT(	SG_inv_entry * _pNew = NULL;										\
																			SG_inv_entry__alloc(pCtx,pInvDir_Back,&_pNew);						\
																			_sg_mem__set_caller_data(_pNew,__FILE__,__LINE__,"SG_inv_entry");	\
																			*(ppThis) = _pNew;													)
#else
#define SG_INV_ENTRY__ALLOC(pCtx,pInvDir_Back,ppThis)		SG_inv_entry__alloc(pCtx,pInvDir_Back,ppThis)
#endif

//////////////////////////////////////////////////////////////////

/**
 * Each InvEntry can have some associated instance data for the
 * entry that maps to it in the source view.
 *
 * This returns the pointer that was given to us; we don't own
 * it and neither do you.
 */
void SG_inv_entry__get_assoc_data__source(SG_context * pCtx,
										  SG_inv_entry * pInvEntry,
										  void ** ppVoidSource);

/**
 * Each InvEntry can have some associated instance data for the
 * entry that maps to it in the target view.
 *
 * This returns the pointer that was given to us; we don't own
 * it and neither do you.
 */
void SG_inv_entry__get_assoc_data__target(SG_context * pCtx,
										  SG_inv_entry * pInvEntry,
										  void ** ppVoidTarget);


//////////////////////////////////////////////////////////////////

/**
 *
 */
void SG_inv_entry__get_flags(SG_context * pCtx,
							 SG_inv_entry * pInvEntry,
							 SG_inv_entry_flags * pFlags);

/**
 *
 */
void SG_inv_entry__get_parked_path__ref(SG_context * pCtx,
										SG_inv_entry * pInvEntry,
										const SG_pathname ** ppPathParked);

/**
 * allocate a pathname for this entry in the parking lot.
 *
 * this will be something of the form:
 *     <wd-top>/.sgtemp/<purpose>_<session>/<source-gid>
 *
 * We own this pathname.
 */
void SG_inv_entry__create_parked_pathname(SG_context * pCtx, SG_inv_entry * pInvEntry, SG_inv_entry_flags reason);

void SG_inv_entry__get_gid__source__ref(SG_context * pCtx, SG_inv_entry * pInvEntry, const char ** ppszGid);

void SG_inv_entry__get_repo_path__source__ref(SG_context * pCtx, SG_inv_entry * pInvEntry, const SG_string ** ppStringRepoPath);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_INV_ENTRY_PROTOTYPES_H
