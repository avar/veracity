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
 * @file sg_inv_dirs_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_INV_DIRS_PROTOTYPES_H
#define H_SG_INV_DIRS_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_inv_dirs__free(SG_context * pCtx,
					   SG_inv_dirs * pInvDirs);

void SG_inv_dirs__alloc(SG_context * pCtx,
						const SG_pathname * pPathWorkingDirTop,
						const char * pszPurpose,
						SG_inv_dirs ** ppInvDirs);

#if defined(DEBUG)
#define SG_INV_DIRS__ALLOC(pCtx,pPathWorkingDirectory,pszPurpose,ppThis)	SG_STATEMENT(	SG_inv_dirs * _pNew = NULL;											\
																							SG_inv_dirs__alloc(pCtx,pPathWorkingDirectory,pszPurpose,&_pNew);	\
																							_sg_mem__set_caller_data(_pNew,__FILE__,__LINE__,"SG_inv_dirs");	\
																							*(ppThis) = _pNew;													)
#else
#define SG_INV_DIRS__ALLOC(pCtx,pPathWorkingDirectory,pszPurpose,ppThis)	SG_inv_dirs__alloc(pCtx,pPathWorkingDirectory,pszPurpose,ppThis)
#endif

//////////////////////////////////////////////////////////////////

/**
 * Given {entryname,entry-gid-object,parent-gid-object} in the "source" version of the tree,
 * "claim" the entryname in the directory.
 *
 * Let: e := _inv_entry(source := <entry-gid-object>, target := <unspecified>)
 *
 * We build 2 maps:
 * [1] _inv_dirs.dir[<parent-gid-object>].entry[<entryname>] --> e
 * [2] _inv_dirs.source[<entry-gid-object>]                  --> e
 */
void SG_inv_dirs__bind_source(SG_context * pCtx,
							  SG_inv_dirs * pInvDirs,
							  const char * pszGidDir, const char * pszRepoPathDir,
							  const char * pszGidEntry, const char * pszEntryName,
							  SG_bool bIsDirectory,
							  void * pVoidSource,
							  SG_bool bActive,
							  struct _inv_dir ** ppInvDir,
							  struct _inv_entry ** ppInvEntry);

/**
 * lookup binding by the gid-mapping.
 *
 * You do not own the returned pointer.
 */
void SG_inv_dirs__fetch_source_binding(SG_context * pCtx,
									   SG_inv_dirs * pInvDirs,
									   const char * pszGidEntry,
									   SG_bool * pbFound,
									   struct _inv_entry ** ppInvEntry,
									   void ** ppVoidSource);

/**
 * Claim the given entryname in the given directory in the "target" version of the tree.
 */
void SG_inv_dirs__bind_target(SG_context * pCtx,
							  SG_inv_dirs * pInvDirs,
							  const char * pszGidDir,
							  const char * pszGidEntry, const char * pszEntryName, void * pVoidTarget,
							  SG_bool bActive,
							  struct _inv_dir ** ppInvDir,
							  struct _inv_entry ** ppInvEntry);

void SG_inv_dirs__fetch_target_binding(SG_context * pCtx,
									   SG_inv_dirs * pInvDirs,
									   const char * pszGidEntry,
									   SG_bool * pbFound,
									   SG_inv_entry ** ppInvEntry,
									   void ** ppVoidTarget);

//////////////////////////////////////////////////////////////////

/**
 *
 */
void SG_inv_dirs__check_for_swaps(SG_context * pCtx,
								  SG_inv_dirs * pInvDirs);

//////////////////////////////////////////////////////////////////

/**
 *
 */
void SG_inv_dirs__check_for_portability(SG_context * pCtx,
										SG_inv_dirs * pInvDirs,
										SG_pendingtree * pPendingTree);

/**
 * Fetch the types of portability warnings that we have logged.
 * This is not the individual events, but rather a bitmask of
 * the various types of issues that we have seen.
 */
void SG_inv_dirs__get_portability_warnings_observed(SG_context * pCtx,
													SG_inv_dirs * pInvDirs,
													SG_portability_flags * pFlags);

//////////////////////////////////////////////////////////////////

/**
 * Return the pathname of the directory to be used for the parking lot.
 *
 * You DO NOT own this.
 */
void SG_inv_dirs__get_parking_lot_path__ref(SG_context * pCtx,
											struct _inv_dirs * pInvDirs,
											const SG_pathname ** ppPathParkingLot);

/**
 * Return the number of entries that need to be parked in the parking lot
 * during the transformation of the WD.  Also include sub-totals for things
 * that were __PARKED_FOR_SWAP and __PARKED_FOR_CYCLE.
 */
void SG_inv_dirs__get_parking_stats(SG_context * pCtx,
									SG_inv_dirs * pInvDirs,
									SG_uint32 * pNrParked,
									SG_uint32 * pNrParkedForSwap,
									SG_uint32 * pNrParkedForCycle);

/**
 * Call pfn with each entry that needs to be parked.
 */
void SG_inv_dirs__foreach_in_queue_for_parking(SG_context * pCtx,
											   SG_inv_dirs * pInvDirs,
											   SG_inv_dirs_foreach_in_queue_for_parking_callback * pfn,
											   void * pVoidData);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)

/**
 * dump the entire _inv_tree to the console.
 */
void SG_inv_dirs_debug__dump_to_console(SG_context * pCtx, SG_inv_dirs * pInvDirs);

/**
 * verify that the parking lot directory is empty.  after all of the structural changes
 * are made and the things that were parked have been moved back to the WD, the parking lot
 * directory should be empty -- if everything was accounted for.
 */
void SG_inv_dirs_debug__verify_parking_lot_empty(SG_context * pCtx, SG_inv_dirs * pInvDirs);

#endif

/**
 * Remove the parking lot directory in .sgtemp from the disk.
 * This should be used at the completion of a successful operation
 * to delete the parking lot directory and free the pathname that
 * we are holding.
 *
 * TODO 2010/06/30 For now, I'm going to require that the parking
 * TODO            lot be empty.  If the parking lot is not empty,
 * TODO            only the caller knows whether the stuff is trash
 * TODO            (such as temp files) or the live version of stuff
 * TODO            that was moved into the parking lot temporarily
 * TODO            as part of a swap of some kind.  So the caller
 * TODO            should clean up the mess before we do the rmdir.
 */
void SG_inv_dirs__delete_parking_lot(SG_context * pCtx, SG_inv_dirs * pInvDirs);

/**
 * Free the parking lot pathname WITHOUT removing it from the disk.
 * This is only to be used if we are throwing a hard error and want
 * to make sure that a caller doesn't try to clean up stuff.  For
 * example, if we put the user's modified/dirty files in the parking
 * lot because of a filename swap and then take a hard error and are
 * crashing, we shouldn't blindly delete the parking lot as we are
 * freeing pointers.
 */
void SG_inv_dirs__abandon_parking_lot(SG_context * pCtx, SG_inv_dirs * pInvDirs);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_INV_DIRS_PROTOTYPES_H
