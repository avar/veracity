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
 * @file sg_portability_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PORTABILITY_PROTOTYPES_H
#define H_SG_PORTABILITY_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_portability_dir__free(SG_context * pCtx, SG_portability_dir * pDir);

/**
 * Allocate a portability_dir structure to allow us to check for
 * collisions between a given list of entrynames.
 *
 * portMask contains the set of SG_PORT_FLAG__ bits that we are
 * interested in.
 *
 * szRelativePath contains the repo-path to the directory.
 *
 * We will append warnings to pStringLog.
 *
 * Once you allocate this, call __add_item() to add the individual
 * items to the collection.
 *
 */
void SG_portability_dir__alloc(SG_context * pCtx,
							   SG_portability_flags portMask,
							   const char * szRelativePath,
							   SG_utf8_converter * pConverterRepoCharSet,
							   SG_varray * pvaLog,
							   SG_portability_dir ** ppDir);

/**
 * Inspect the given file/directory entryname (which is a part of the
 * container's relative pathname) and see if it has any problematic
 * chars and add it to our container so that we can check for collisions.
 *
 * If we find a problem, we update the pItem->portWarnings.
 *
 * We try to be as verbose as possible and completely list warnings
 * rather than stopping after the first one.
 *
 * If this entryname is an exact-match with an existing entry already
 * in the collider, we DO NOT insert it and set *pbIsDuplicate.
 */
void SG_portability_dir__add_item(SG_context * pCtx,
								  SG_portability_dir * pDir,
								  const char * pszGid_Entry,		// optional, for logging.
								  const char * szEntryName,
								  SG_bool bCheckMyName_Indiv, SG_bool bCheckMyName_Collision,
								  SG_bool * pbIsDuplicate);

/**
 * Version of __add_item() where we store an optional assoc-data with
 * the entry.
 *
 * If this entryname is an exact-match with an existing entry already
 * in the collider, we DO NOT insert it and set *pbIsDuplicate and
 * optionally return the assoc-data for the one we matched.
 */
void SG_portability_dir__add_item__with_assoc(SG_context * pCtx,
											  SG_portability_dir * pDir,
											  const char * pszGid_Entry,	// optional, for logging.
											  const char * szEntryName,
											  SG_bool bCheckMyName_Indiv, SG_bool bCheckMyName_Collision,
											  void * pVoidAssocData, SG_varray * pvaLog,
											  SG_bool * pbIsDuplicate, void ** ppVoidAssocData_Original);

/**
 * Fetch the union of the portability-warning-flags.
 *
 * @param pFlagsWarningsAll (Optional) is set to the union of all issues detected.  This is
 * independent of the MASK and individual item CHECKED flags.
 *
 * @param pFlagsWarningsChecked (Optional) is set to the union of all issues detected for
 * only CHECKED items.  This is independent of the MASK.
 *
 * @param pFlagsLogged (Optional) is set to the union of all issues detected for only
 * CHECKED items filtered by the MASK.  Since we only generate log messages for MASKED
 * issues for CHECKED items, this value will be non-zero when log content was generated.
 *
 */
void SG_portability_dir__get_result_flags(SG_context * pCtx,
										  SG_portability_dir * pDir,
										  SG_portability_flags * pFlagsWarningsAll,
										  SG_portability_flags * pFlagsWarningsChecked,
										  SG_portability_flags * pFlagsLogged);

/**
 * Fetch the portability-warning-flags for the item with this original
 * entryname.  This name corresponds to the entryname used in __add_item().
 *
 * Warning: the complete set of collision flags cannot be computed until
 * after all the individual files/directories are added to the container.
 * Individual __INDIV__ bits are available immediately, but the __COLLISION__
 * bits are not.
 *
 * @param pFlagsWarnings (Optional) is the set of issues detected for this item.
 * This is independent of the MASK.
 *
 * @param pFlagLogged (Optional) is the set of issues detected for this item
 * filtered by the MASK.  That is, if this is non-zero, there should be a
 * message about this item somewhere in the log.
 *
 */
void SG_portability_dir__get_item_result_flags(SG_context * pCtx,
											   SG_portability_dir * pDir,
											   const char * szEntryName,
											   SG_portability_flags * pFlagsWarnings,
											   SG_portability_flags * pFlagsLogged);

/**
 * the __with_assoc() version also returns the optional user-data given
 * if the __add_item__with_assoc() version was used.
 */
void SG_portability_dir__get_item_result_flags__with_assoc(SG_context * pCtx,
														   SG_portability_dir * pDir,
														   const char * szEntryName,
														   SG_portability_flags * pFlagsWarnings,
														   SG_portability_flags * pFlagsLogged,
														   void ** ppVoidAssocData);

//////////////////////////////////////////////////////////////////

/**
 * Call the given callback for each item that had a warning set on it.
 *
 * If bAnyWarnings is set, we notify if anything was set on the item.
 *
 * If bAnyWarnings is not set, we only notify for items that were logged.
 *
 */
void SG_portability_dir__foreach_with_issues(SG_context * pCtx,
											 SG_portability_dir * pDir,
											 SG_bool bAnyWarnings,
											 SG_portability_dir_foreach_callback * pfn,
											 void * pVoidCallerData);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_PORTABILITY_PROTOTYPES_H
